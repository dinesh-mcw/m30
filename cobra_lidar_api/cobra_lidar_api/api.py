"""
file: api.py

Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.

HTTP REST API resource definitions and endpoint routing.

Each resource subclass is tied to a specified endpoint in ``configure_api``,
where each instance method defines the server response to the corresponding
HTTP request at that endpoint.

To enable the majority of GET requests for internal parameters,
settings are recorded per sensor head with default values stored.
This enables the user to use dedicated single-parameter endpoints,
such as ``/snr`` or pass through truncated objects to the SWDL endpoint
to update only select parameters at a given time. In these cases,
the _full_ settings object with specified fields updated is passed
through to the corresponding FW call
"""

import dataclasses as dc
import json
import queue
import subprocess
import time
import os
from pathlib import Path
from typing import Dict

from flask import send_file, request
from flask_restful import Api, Resource as BaseResource, abort
from pyzipper import AESZipFile, WZ_AES, ZIP_LZMA, ZIP_STORED

from cobra_system_control.cobra import Cobra
from cobra_system_control.cobra_log import log
from cobra_system_control.exceptions import (
    ScanPatternError, ScanPatternPowerError,
    ScanPatternSizeError, ScanPatternValueError,
)
from cobra_system_control.fpga_field_funcs import FpgaFieldFuncs
from cobra_system_control.itof import FrameSettings
from cobra_system_control.memory_map import M30_FPGA_MEMORY_MAP
from cobra_system_control.random_access_scanning import MaxRangeIdxMappedOv
from cobra_system_control.scan_control import get_scan_time_constants
from cobra_system_control.state import State

from cobra_lidar_api.schema import (
    SystemInfo, load_usr_json_to_system_schema
)

TOKEN_PATH = Path.home() / ".lumotive" / "credentials"

MAX_MESSAGES = 1000
MSG_Q = queue.Queue(maxsize=MAX_MESSAGES)
MSG_Q.put("Loading API...")


# Start with default settings for each sensor head
defaults = dict(
    angle_range=[[-45.0, 45.0, 1.0]],
    fps_multiple=[1],
    laser_power_percent=[100],
    inte_time_us=[15],
    max_range_m=[25.2],
    frame_rate_hz=[960],
    binning=[2],
    snr_threshold=[1.8],
    nn_level=[0],
    user_tag=[0x1],
    interleave=False,
    dsp_mode=0,
    hdr_threshold=4095,
    hdr_laser_power_percent=[40],
    hdr_inte_time_us=[5],
)

CURRENT_SYSTEM_INFO = SystemInfo(**defaults)

SCAN_PATTERN_ERRORS = (
    ScanPatternError,
    ScanPatternPowerError,
    ScanPatternSizeError,
    ScanPatternValueError,
)


STATUS_FIELDS = {'system_clock_synced', 'master_offset', 'gmPresent', 'pps1_assert'}
TSYNC_FILE = "/home/root/cobra/tsync.conf"
TIMESYNC_PERSISTENT_SETTINGS = {"frontend_options", "ptp4l_options"}
NTP_FILE = "/home/root/cobra/ntp.conf"
NTP_PERSISTENT_SETTINGS = {"ntp_server"}
NETWORK_FILE = "/etc/systemd/network/20-eth-static.network"
NETWORK_PERSISTENT_SETTINGS = {"ipaddr", "netmask_cidr", "gateway"}
ALL_PERSISTENT_SETTINGS = TIMESYNC_PERSISTENT_SETTINGS.union(NTP_PERSISTENT_SETTINGS).union(NETWORK_PERSISTENT_SETTINGS)


def beautify_settings(settings: Dict[str, list]):
    """Basically does ``json.dumps`` but only for one level (nested values are not dumped).
    """
    ret = "{"
    for param, values in settings.items():
        ret += f"\n\t{param}: {values}"
    ret += "\n}"
    return ret


def log_for_user(msg: str):
    """Logs a message at the ``info`` level and inserts it into the user
    message queue for readout.

    If you are going to add multiple messages at a single endpoint,
    add them as a single joined string instead of back-to-back calls
    to ensure they can be read correctly in UI interactions.

    Examples:
        >>> log_for_user("Oh no, something bad happened!")

        >>> m1 = "Message 1"
        >>> m2 = "Another message!"
        >>> m3 = "Oh yes!"
        >>> log_for_user("\\n".join([m1, m2, m3]))
    """
    log.info(msg)
    if MSG_Q.full():
        MSG_Q.get()
    MSG_Q.put(msg)


def get_updated_settings(c: Cobra) -> dict:
    updated = {}
    ras = c.sen.random_access_scan.ras_scan_parameters
    for param in SystemInfo.all_fields():
        if param == "state":
            new_val = State(c.sen.state).name
        elif param == "system_version":
            new_val = c.system_version
        elif param == "sensor_id":
            new_val = c.sen.sensor_id
        else:
            # Query from the random access scan property
            try:
                new_val = ras[param]
            except AttributeError:
                # Random access scan hasn't been set yet - get API default
                new_val = getattr(CURRENT_SYSTEM_INFO, param)

        updated[param] = new_val
    return updated


def get_actual_frame_rate(settings: dict) -> list:
    """Calculates the actual frame rate from the given scan
    settings and logs for the user.
    Returns a list of messages.
    """
    messages = []
    binning = settings['binning']
    frame_rate_hz = settings['frame_rate_hz']
    inte_time_us = settings['inte_time_us']
    max_range_m = settings['max_range_m']
    ff = FpgaFieldFuncs(memmap_fpga=M30_FPGA_MEMORY_MAP)

    for j, (bnx, frx, inx, mrix) in enumerate(zip(
            binning, frame_rate_hz, inte_time_us, max_range_m)):
        mod_freqs = MaxRangeIdxMappedOv.MAP[mrix]
        frame = FrameSettings(
            0, mod_freq_int=mod_freqs,
            inte_time_s=(inx*1e-6, inx*1e-6),
        )
        _, _, tp1_fields, pol_cnt_tc, _, _ = get_scan_time_constants(
            ff, frame, bnx, frx)
        tp1_vals = [ff.getv_tp1_period(x, 0) for x in tp1_fields]
        pol_cnt = [ff.getv_pol_cnt_tc(x) for x in pol_cnt_tc]
        frame_time_us = (
            (tp1_vals[0] * pol_cnt[0] * 2)
            + (tp1_vals[1] * pol_cnt[1] * 2)
        )
        frame_time_hz = 1 / (frame_time_us * 1e-6)
        messages.append(
            f'Virtual Sensor {j+1}, actual Depth Measurement Rate [Hz] '
            f'is {frame_time_hz:.0f} Hz')
    return messages


def apply_random_access_scan_settings(settings: dict):
    """Takes new settings object, merges it with the current settings
    stored in the API, and applies them to the sensor head
    (when available) via ``apply_random_access_scan_settings``.

    Responses are dictated by the success of the settings application,
    with HTTP 200 meaning sensor head updated successfully, and
    HTTP 5XX meaning sensor failed in application.

    This will always update _all_ parameters, regardless if the user
    provided a truncated set of parameters. Validation of compatible
    parameter values is performed in the firmware.

    Note that this must be called inside a ``Cobra`` context manager block
    so as to avoid nesting ``Cobra.remote()`` calls when possible.
    """

    # Prepare return information and status code for the user
    # 200 == _all_ succeeded
    # 5XX == _any_ sensor head failed
    status = 200
    messages = []

    c = Cobra('m30', 'nxp')
    sen = c.sen

    sen.connect()
    sen.setup()

    is_scanning = State(sen.state) is State.SCANNING
    try:
        log.debug("start_apply_settings")
        # Leave the frontend alone if we were already scanning.
        # Stop is now in the apply_settings in an optimized location
        # sh.stop(stop_fe_streaming=not is_scanning)
        # Apply the settings. There is now a stop in apply_settings()
        try:
            sen.apply_random_access_scan_settings(**settings)
        except Exception as e:
            print(e)
            
        msgs = get_actual_frame_rate(settings)
        messages.extend(msgs)
        #  Restart scan if we were previously scanning
        if is_scanning:
            # Leave the frontend alone if we were already scanning.
            # Using the API, the frontend mode will not change.
            sen.start(start_fe_streaming=not is_scanning)
        log.debug("end_apply_settings")
    except SCAN_PATTERN_ERRORS as e:
        msg = (f"Applying settings on sensor head failed "
               f"due to the following reason: {e!s}")
        messages.append(msg)

        log.exception(msg, exc_info=e)
        ret = f"FAILED: {e!s}"
        status = 555
    except Exception as e:
        messages.append("Applying settings on sensor head failed!")

        log.exception("Apply RAS on failed.", exc_info=e)
        ret = "FAILED"
        status = 555
    else:
        # Update the stored values with what was written
        # Store the new values on success
        # pylint disable global since I don't know how to make it work without
        # pylint: disable-next=global-statement
        global CURRENT_SYSTEM_INFO
        CURRENT_SYSTEM_INFO = dc.replace(CURRENT_SYSTEM_INFO, **settings)
        ret = "SUCCESS"

        messages.append("Applying settings on sensor head was successful.")
    log_for_user("\n".join(messages))
    return ret, status


class Resource(BaseResource):
    """Hack to make ``flask-restful`` return 405 errors with React.
    TODO: Configure the server and webapp correctly to avoid this

    Basically:

    ``flask-restful`` will respond with HTTP 405 on any resource for which the
    respectively called HTTP method is not defined. This is because the resource
    exists, but the action to take is undefined and so is disallowed.

    However, we define the server to host static files at static_url ``/``.
    This means that any endpoint sharing the base URL (AKA _every_ endpoint)
    with an undefined action will defer to checking for a static file.
    e.g., ``GET /update`` will attempt a GET request at ``/update``,
    see it's not allowed, _then check for a static resource at ``/update``
    and return a corresponding HTTP 404 error because we have no static
    resource named "update"_.
    This 404 status code takes precedence over the prior mentioned 405 error
    due to execution order (or something).

    This is _not_ the behavior we want! We want 405 if it's disallowed.
    So add definitions to the base ``Resource`` class that enforce
    this HTTP code to be returned, since we _do not_ host static resources
    with the same names as any of our endpoints.
    """
    def get(self):
        abort(405)

    def post(self):
        abort(405)

    def put(self):
        abort(405)

    def patch(self):
        abort(405)

    def delete(self):
        abort(405)


# System operations
class Disable(Resource):
    """Disables the system and removes power
    to sensor head for a safe shutdown.
    """
    def post(self):
        log.debug("POST called on disable resource.")
        return_message = "Sensor head disabled and powered down.\n"

        with Cobra.remote() as c:
            c.stop()
            c.shutdown_sh_power()
        subprocess.run(["sudo", "systemctl", "stop", "remote"], check=True)

        log_for_user(return_message)
        return return_message


class Shutdown(Resource):
    """Shuts down the compute platform so power can be removed safely.
    """
    def post(self):
        log.debug("POST called on shutdown resource.")
        return_message = "Shutting down system.\n"

        subprocess.run(["sudo", "shutdown", "-h", "now"], check=True)

        log_for_user(return_message)
        return return_message


class Restart(Resource):
    """Reboots the sensor head by restarting the remote object.
    """
    def post(self):
        log.debug("POST called on Restart resource.")
        return_message = "Sensor Head was restarted.\n"

        log.debug("Restarting Sensor Head")
        subprocess.run(["sudo", "systemctl", "restart", "remote"], check=True)

        log_for_user(return_message)
        return "Success"


class Update(Resource):
    """Switches system to Recovery mode which is used for applying SW updates.
    """

    def post(self):
        log.debug("POST called on Update resource.")
        log_for_user("Rebooting into update mode. "
                     "Please wait 90 seconds for the page to reload.\n")

        # Brief sleep to allow message to be polled
        time.sleep(1)

        log.debug("Rebooting into recovery mode")
        subprocess.run(["fw_setenv", "BOOT_MAIN_LEFT", "0"], check=True)
        subprocess.run(["fw_setenv", "BOOT_RESCUE_LEFT", "5"], check=True)
        subprocess.run(["sync"], check=True)
        subprocess.run(["reboot"], check=True)
        return "Switching to Update Mode."


# Full system resources
class MappingTable(Resource):

    def get(self):
        log.debug("GET called on MappingTable resource")
        zipname = "mapping_table.zip"

        log_for_user("Downloading mapping tables. This may take a while...\n")

        with Cobra.remote() as c:
            sh = c.sen
            mapping_path = Path(sh.mapping_table_path).resolve()

        cob_dir = Path.home() / "cobra"
        cob_dir.mkdir(parents=True, exist_ok=True)
        zip_file = (cob_dir / zipname).resolve()

        # Return zipfile of all CSVs, but don't encrypt it
        with AESZipFile(zip_file, "w", compression=ZIP_STORED) as zf:
            if mapping_path.exists():
                log_for_user("Getting mapping table for sensor head.\n")
                zf.write(mapping_path, mapping_path.name)

        return send_file(zip_file, mimetype="application/zip")


class SystemLogs(Resource):

    def get(self):
        log.debug("GET called on SystemLogs resource")

        # Get reference to log directory and make if it doesn't already exist
        log_dir = Path.home() / "cobra"
        log_dir.mkdir(parents=True, exist_ok=True)

        # Get cal paths from each sensor head
        with Cobra.remote() as c:
            sh = c.sen
            cal_path = Path(sh.cal_data_path).resolve()

        # Resolve file paths for each of the files we want to send
        zip_file = (log_dir / "logs.zip").resolve()
        db_file = (log_dir / "lidar_monitors.db").resolve()
        log_file = Path("/", "var", "log", "messages").resolve()
        log0_file = Path("/", "var", "log", "messages.0").resolve()

        # Create encrypted zipfile
        with AESZipFile(zip_file, "w", compression=ZIP_LZMA, encryption=WZ_AES) as zf:
            # Encrypt with the locally-stored ``logkey`` token
            with open(TOKEN_PATH, "r+", encoding='utf-8') as tok:
                cfg = json.load(tok)
                zf.setpassword(cfg["logkey"].encode("utf-8"))

            # Write syslog
            if log_file.exists():
                zf.write(log_file, log_file.name)

            if log0_file.exists():
                zf.write(log0_file, log0_file.name)

            # Write database file
            if db_file.exists():
                zf.write(db_file, db_file.name)

            # Write cal data for sensor head
            if cal_path.exists():
                zf.write(cal_path, cal_path.name)

        # Returned the newly-created zip
        return send_file(zip_file, mimetype="application/zip")


class SystemVersion(Resource):

    def get(self):
        log.debug("GET called on SystemVersion resource")

        with Cobra.remote() as c_:
            dver = c_.system_version
            dver['api_version'] = "4.2.0"
            return {"system_version": dver}


class SensorId(Resource):

    def get(self):
        log.debug("GET called on SensorId resource")

        with Cobra.remote() as c_:
            return {"sensor_id": c_.sen.sensor_id}


class LidarState(Resource):

    def get(self):
        log.debug("GET called on LidarState resource")

        with Cobra.remote() as c_:
            return {"state": State(c_.sen.state).name}


# Scan control
class StartScan(Resource):

    def post(self):
        log.debug("POST called on StartScan resource.")

        status = 200

        with Cobra.remote() as c:
            sh = c.sen

            current_state = State(sh.state)
            if current_state is not State.ENERGIZED:
                # Can only start scanning when in state ``ENERGIZED``
                if current_state is State.SCANNING:
                    ret = "FAILED: ALREADY SCANNING"
                    msg = "Failed to start scanning because it is already scanning."
                else:
                    ret = "FAILED: NOT IN 'ENERGIZED'"
                    msg = "Failed to start scanning because it is not in 'ENERGIZED'."
                status = 555
                log_for_user(msg)
                return ret, status

            try:
                # Must apply existing settings when starting scan
                sh.start()
            except SCAN_PATTERN_ERRORS as e:
                msg = (f"Starting the scan on sensor head failed "
                       f"due to the following reason: {e!s}\n")
                log.exception(msg, exc_info=e)
                ret = f"FAILED: {e!s}"
                status = 555
                log_for_user(msg)
            except Exception as e:
                log.exception("Starting scan failed", exc_info=e)
                ret = "FAILED"
                status = 555
                log_for_user("Failed to start scanning for an unknown reason.")
            else:
                # Don't update state here - that gets updated on all reads
                ret = "SUCCESS"
                log_for_user("Starting the scan on sensor succeeded.\n")

        return ret, status


class StopScan(Resource):

    def post(self):
        log.debug("POST called on StopScan resource.")
        status = 200

        messages = []
        with Cobra.remote() as c:
            sh = c.sen

            try:
                sh.stop()
            except Exception as e:
                ret = "FAILED"
                status = 555
                messages.append(
                    f"Failed to stop scanning for an unknown reason. {e}")
            else:
                # Don't update state here - that gets updated on all reads
                ret = "SUCCESS"
                messages.append("Stopping the scan on sensor succeeded.\n")

        log_for_user("\n".join(messages))
        return ret, status


# class SoftwareDefinedLidar(Resource):
#     """Resource for getting and setting all scan information compatible
#     with software defined lidar (SWDL).

#     These are the parameters defined in the ``SensorInfo`` dataclass.
#     """
#     def get(self):
#         log.debug("GET called on SoftwareDefinedLidar resource")

#         with Cobra.remote() as c_:
#             return c_.sen.random_access_scan.ras_scan_parameters

#     def post(self):
#         log.debug("POST called on SoftwareDefinedLidar resource.")
#         settings = load_usr_json_to_system_schema()

#         # with Cobra.remote() as c_:
#         
#         return ret

class SoftwareDefinedLidar(Resource):
    def get(self):
        return {"message": "GET request received", "scan_parameters": "example_parameters"}
    
    def post(self):
        data = request.get_json()
        print(f"POST request received with data: {data}")
        ret = apply_random_access_scan_settings(data)
        return {"message": "POST request received", "received_data": ret}


class SoftwareDefinedLidarOptions(Resource):
    def get(self):
        ret = {}
        for param in SystemInfo.writable_field():
            ret[param] = SystemInfo.api_options(param)
        return ret


def collect_timesync_settings():
    """ Read the persistent settings related to time synchronization and
    return them in a dictionary.
    """
    try:
        with open(TSYNC_FILE, "r") as ts_file:
            data = ts_file.read()
    except Exception as e:
        log.exception('failed to read timesync config file', exc_info=e)
        abort(422)

    ret = {}
    for line in data.split('\n'):
        parts = line.split('=', 1)
        if len(parts) == 2:
            ret[parts[0]] = parts[1]
    return ret


def collect_ntp_settings():
    """ Read the persistent setting related to NTP and return it in a
    dictionary.
    """
    try:
        with open(NTP_FILE, "r") as ntp_file:
            data = ntp_file.read()
    except Exception as e:
        log.exception('failed to read ntp config file', exc_info=e)
        abort(422)

    ret = {}
    for line in data.split('\n'):
        parts = line.split(' ', 1)
        if len(parts) == 2 and parts[0] == 'server':
            ret['ntp_server'] = parts[1]
    return ret


def collect_network_settings():
    """ Read the IPv4 persistent network settings and return them in
    a dictionary.
    """
    try:
        with open(NETWORK_FILE, "r") as network_file:
            data = network_file.read()
    except Exception as e:
        log.exception('failed to read network config file', exc_info=e)
        abort(422)

    ret = {}
    for line in data.split('\n'):
        parts = line.split('=', 1)
        if len(parts) == 2:
            if parts[0] == 'Address':
                addr_parts = parts[1].split('/')
                if len(addr_parts) == 2:
                    ret['ipaddr'] = addr_parts[0]
                    ret['netmask_cidr'] = addr_parts[1]
            elif parts[0] == 'Gateway':
                ret['gateway'] = parts[1]

    return ret


def collect_persistent_settings():
    """ Read all of the persistent settings (settings that persist
    across reboots) from their locations on the filesystem and return
    them in a dictionary
    """
    settings = collect_timesync_settings()
    settings.update(collect_ntp_settings())
    settings.update(collect_network_settings())
    return settings


def save_timesync_settings(settings):
    """ Writes back the time synchronization settings to their
    persistent file on the filesystem.
    """
    # Pull out timesync settings only
    timesync_settings = {}
    for setting in settings:
        if setting in TIMESYNC_PERSISTENT_SETTINGS:
            timesync_settings[setting] = settings[setting]
    try:
        with open(TSYNC_FILE, "w") as ts_file:
            for key in timesync_settings:
                ts_file.write(f'{key}={timesync_settings[key]}\n')
    except Exception as e:
        log.exception('failed to write timesync config file', exc_info=e)
        abort(422)


def save_ntp_settings(settings):
    """ Writes back the ntp setting to its persistent file on the
    filesystem.
    """
    # The following code only works because there is only one setting
    try:
        with open(NTP_FILE, "w") as ntp_file:
            ntp_file.write(f'server {settings["ntp_server"]}\n')
    except Exception as e:
        log.exception('failed to write ntp config file', exc_info=e)
        abort(422)


def save_network_settings(settings):
    """ Writes back the network settings to their persistent file on
    the filesystem. This code simply overwrites the entire file. The
    gateway key is optional.
    """
    try:
        with open(NETWORK_FILE, "w") as network_file:
            network_file.write('[Match]\nName=eth0\n\n[Network]\n')
            network_file.write(f'Address={settings["ipaddr"]}/{settings["netmask_cidr"]}\n')
            if "gateway" in settings.keys():
                network_file.write(f'Gateway={settings["gateway"]}\n')
    except Exception as e:
        log.exception('failed to write network config file', exc_info=e)
        abort(422)
    os.system('/bin/cp /etc/systemd/network/20-eth-static.network /run/media/rescue-mmcblk0p1/etc/systemd/network/ ; sync')


class PersistentSettings(Resource):
    """Resource for getting and setting the settings that persist
    across a reboot. Currently they only affect the front end
    """
    def get(self):
        return collect_persistent_settings()

    def post(self):
        new_settings = request.get_json()
        # Make sure that all requested settings have valid keys
        for setting in new_settings:
            if not setting in ALL_PERSISTENT_SETTINGS:
                abort(400)

        # Create new persistent settings containing new settings
        cur_settings = collect_persistent_settings()
        cur_settings.update(new_settings)

        if TIMESYNC_PERSISTENT_SETTINGS & new_settings.keys():
            save_timesync_settings(cur_settings)
        if NTP_PERSISTENT_SETTINGS & new_settings.keys():
            save_ntp_settings(cur_settings)
        if NETWORK_PERSISTENT_SETTINGS & new_settings.keys():
            save_network_settings(cur_settings)
        return "SUCCESS"


class TimeSyncStatus(Resource):
    """ Resource for getting the current time synchronization status
    of the sensor. These status values are needed to debug the issues
    with time synchronization
    """
    def get(self):
        try:
            output = subprocess.run('/usr/sbin/tsync_status', stdout=subprocess.PIPE)
        except Exception as e:
            log.error('failed to execute tsync_status', exc_info=e)
            abort(422)

        json = {}
        for line in output.stdout.decode('UTF-8').split('\n'):
            parts = line.split(':')
            if len(parts) == 2 and parts[0] in STATUS_FIELDS:
                json[parts[0]] = parts[1]

        return json


def configure_api(api: Api):
    # File resources
    api.add_resource(MappingTable, "/mapping")
    api.add_resource(SystemLogs, "/logs")

    # Read-only
    api.add_resource(SystemVersion, "/system_version")
    api.add_resource(SensorId, "/sensor_id")

    # Resources that get or modify system state
    api.add_resource(Disable, "/disable")
    api.add_resource(Shutdown, "/shutdown")
    api.add_resource(LidarState, "/state")
    api.add_resource(StartScan, "/start_scan")
    api.add_resource(StopScan, "/stop_scan")
    api.add_resource(Restart, "/restart")

    # Full system information and operations
    api.add_resource(Update, "/update")

    # Dedicated Programmable Lidar resource
    api.add_resource(SoftwareDefinedLidar, "/scan_parameters")
    api.add_resource(SoftwareDefinedLidarOptions, "/scan_parameters/opts")

    # Persistent settings
    api.add_resource(PersistentSettings, "/persistent_settings")
    api.add_resource(TimeSyncStatus, "/time_sync_status")
