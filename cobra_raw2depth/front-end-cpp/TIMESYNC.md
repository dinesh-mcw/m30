# Overview

On M30, the point cloud data has a time stamp for every 64 pixel packet.
In order to time synchronize the point cloud data with other data sources,
for example, inertial sensor data, we need to base the timestamps on an
accurate source of time. On an M30 system we can base the time on one of
three time sources:

- PTP. The source of accurate time is a PTP grandmaster on the same
  network as the M30.

- 1PPS. The source of accurate time is a 1PPS signal connected the NCB.
  NTP is also used.

- No accurate time source. In this case the time is not synchronized. The
  timestamps are based on when the first scan is started and are
  incremented based on the FPGA clock.

When an accurate time source is available, the timestamps are always given
in UTC.

# Setting up the system for time synchronization

The user determines the time synchronization source by connecting the
proper equipment and setting up the operating system accordingly.

## Setting up PTP time synchronization

### Required equipment

You will need a PTP grandmaster that supports gPTP too. For testing the NCB,
we used the TimeMachines TM 2500C
(`https://timemachinescorp.com/product/gps-ntpptp-network-time-server-10mz-output-tm2500/`).
The PTP grandmaster must be on the same network as the M30.

### Steps to enable PTP based time synchronization

With PTP as the time source, the user must

1. Connect a PTP grandmaster on the same `10.20.30.xx` network that the sensor
   and the NCB are on. A gigabit Ethernet switch is required.

2. Make sure the grandmaster has a static IP address that isn't the NCB's IP
   address (`10.20.30.40`) or your PC's IP address (which we assume is
   `10.20.30.39`).

3. Configure the grandmaster PTP to use peer to peer, two-step, IPV4 UDP
   multicast. This is the default NCB configuration for PTP. **NOTE**: Your
   switch may not support this setup. In that case you should try to modify
   your grandmaster to support 802.1AS instead of IPV4 PDP. You will also
   need to tell the NCB to use 802.1AS by issuing the following command at
   your Linux host's command line:

   `curl -X POST -H "Content-Type: application/json" -d '{ "ptp4l_options": "-2 -P -H -i eth0 -s --logging_level=5 --tx_timestamp_timeout=50 --max_frequency=250000000 --tsproc_mode=raw_weight --step_threshold=0.05" }' http://10.20.30.40/persistent_settings`

   If you ever want to change it back to IPV4 UDP again you use this command:

   `curl -X POST -H "Content-Type: application/json" -d '{ "ptp4l_options": "-4 -P -H -i eth0 -s --logging_level=5 --tx_timestamp_timeout=50 --max_frequency=250000000 --tsproc_mode=raw_weight --step_threshold=0.05" }' http://10.20.30.40/persistent_settings`

4. Now tell the NCB to use PTP time synchronization:

   `curl -X POST -H "Content-Type: application/json" -d '{ "frontend_options": "-s ptp" }' http://10.20.30.40/persistent_settings`

5. Shutdown the NCB from the user interface and wait for all the LEDS on the
   NCB to shut off.

6. Cycle the power on the NCB to start it up again.

## Setting up time synchronization with an external 1PPS signal

### Required equipment

You will need a 1PPS signal connected to the U.FL connector at J8 on the NCB.
The input is 5V tolerant. The 1PPS signal rising edge must occur at the top of
the second. The M30 determines the timestamps using NTP, so the 1PPS signal must
be synchronized with real time. You can test with an unsynchronized 1PPS signal,
but your timestamps will only be accurate within a second at first and will
drift farther away over time. For testing the NCB, we generated the 1PPS signal
using the TimeMachines TM 2500C
(`https://timemachinescorp.com/product/gps-ntpptp-network-time-server-10mz-output-tm2500/`).

### Steps to enable 1PPS based time synchronization

The NCB will need access to one or more NTP servers for this to work. We will use your PC as an NTP server.

1. Get ntp on your Ubuntu machine

   `sudo apt-get install ntp`

3. Connect the 1PPS source (which must be synchronized with real time) to the
   1PPS input on the NCB (The U.FL connector on J8).

4. Now tell the NCB to user PTP time synchronization by issuing the following
   command from your Linux host:

   `curl -X POST -H 'Content-Type: application/json' -d '{ "frontend_options": "-s pps" }' http://10.20.30.40/persistent_settings`

5. If your host machine's IP address is not 10.20.30.39, you will need to modify
   NTP server IP address. You do this by issuing the following command from your
   Linux host:

   `curl -X POST -H 'Content-Type: application/json' -d '{ "ntp_server": "<ip_address_of_your_linux_host> iburst" }' http://10.20.30.40/persistent_settings`

6. Shutdown the NCB from the user interface and wait for all the LEDS on the
   NCB to shut off.

7. Cycle the power on the NCB to start it up again.

Note that NTP will take about two minutes to synchronize the time.

# Using time synchronization

Now that you have booted up with time synchronization enabled, you must start
a new scan session _after_ the NCB system clock is synchronized.

1. Ensure the NCB system clock is synchronized. For PTP, this happens relatively
   quickly, but for NTP, the process can take about two minutes.

   From your host Linux PC execute the following command:

   `curl http://10.20.30.40/time_sync_status`

   Look for the value of the `system_clock_synced` key. If it is `yes` then the
   system time is synchronized. If it is `no` then you need to keep executing
   the curl command until the value of the `system_clock_synced` key is `yes`.

   Now that the clock is synchronized, you need to start a new scanning session.

2. From your browser, access the web-based M30 user interface. It is typically
   at `http://10.20.30.40`.

3. If a scan is in progress, click the Stop Scan button in the UI.

4. Start a new scan by clicking the Start Scan button.

The point cloud packets will now have timestamps that are synchronized with UTC
within tens of microseconds.

# Troubleshooting

## Troubleshooting PTP time synchronization

If your timestamps are not in UTC while using PTP based time synchronization, it
could be for the following reasons:

- The PTP servo daemon (`ptp4l`) can't find a PTP grandmaster clock on the
  network. You can check if the PTP daemon sees a grandmaster clock by issuing
  the following command:

  `curl http://10.20.30.40/time_sync_status`

  Look for the value of the `gmPresent` key. It should be `true`. If it is
  `false`, the ptp4l daemon has not found a grandmaster clock. There are three
  possible reasons for no clock being present:

  - The grandmaster is not physically connected. Solution: connect a grandmaster
    clock to the network.

  - The PTP4L settings do not match the settings of your grandmaster clock.
    Solution: make sure that your grandmaster clock is set to use IPv4 UDP peer
    to peer multicast by executing the following command on your Linux host:

    `curl -X POST -H "Content-Type: application/json" -d '{ "ptp4l_options": "-4 -P -H -i eth0 -s --logging_level=5 --tx_timestamp_timeout=50 --max_frequency=250000000 --tsproc_mode=raw_weight --step_threshold=0.05" }' http://10.20.30.40/persistent_settings`

  - The PTP settings in `/lib/systemd/system/frontend.service` and the grandmaster
    clock are not compatible with your switch or router. Solution: try using
    802.1AS by executing the following command on your Linux host:

    `curl -X POST -H "Content-Type: application/json" -d '{ "ptp4l_options": "-2 -P -H -i eth0 -s --logging_level=5 --tx_timestamp_timeout=50 --max_frequency=250000000 --tsproc_mode=raw_weight --step_threshold=0.05" }' http://10.20.30.40/persistent_settings`

- The PTP servo daemon (`ptp4l`) hasn't converged yet. You can check the
  convergence of `ptp4l` by running the following command on your Linux host:

  `curl http://10.20.30.40/time_sync_status`

  Look for the value of the `master_offset` key. It should be less than
  100000 (100,000ns or 100us). If it is not, there may be an issue with the
  grandmaster clock.

- The system clock has not synchronized yet. You can check whether the system
  clock has synchronized by executing the following command on your Linux
  host:

  `curl http://10.20.30.40/time_sync_status`

  Look for the value of the `system_clock_synced` field and make sure it is
  `yes`.

- You did not restart the scan after the system clock synchronized. After you
  have verified that the system clock has synchronized (see the last bullet
  point) stop and start the scan in the M30 user interface.

## Troubleshooting time synchronization based on a 1PPS signal

If your timestamps are not in UTC while using time synchronization based on a
1PPS signal, it could be for the following reasons:

- The 1PPS signal is not connected or not working. Use an oscilloscope to look at
  the output of your 1PPS source. You can also probe the resistor next to J7 to
  see if the signal is making it onto the NCB. To see if the software can see the
  1PPS signal, execute the following command on your Linux host:

  `curl http://10.20.30.40/time_sync_status`

  Look for the value of the `pps1_assert` key. This value should change every
  second. If it is not, make sure your PPS signal levels are sufficient to
  trigger the Schmitt trigger (approx 1.6 V to assert and 1.1 V to deassert).

- The system clock has not synchronized yet. You can check whether the system
  clock has synchronized by executing the following command on your Linux
  host:

  `curl http://10.20.30.40/time_sync_status`

  Look for the value of the `system_clock_synced` field and make sure it is
  `yes`.

- You did not restart the scan after the system clock synchronized. After you
  have verified that the system clock has synchronized (see the last bullet
  point) stop and start the scan in the M30 user interface.
