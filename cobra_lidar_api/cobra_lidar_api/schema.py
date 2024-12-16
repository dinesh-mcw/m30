"""
file: schema.py

Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.

Marshmallow schema and derived dataclasses for:
* parsing and validating user input for system-control communication
* marshalling sensor information to a standard output format
* storing pseudo-queryable sensor information in the API
    * (information the user should be able to query, but we can't get from system-control)
"""
import dataclasses as dc
import math
from typing import List

from flask import request
from marshmallow import (
    Schema, ValidationError, fields, pre_load,
    validate, validates_schema,
)
import numpy as np

from cobra_system_control.cobra_log import log
from cobra_system_control.exceptions import ScanPatternSizeError
from cobra_system_control.functional_utilities import get_common_length
from cobra_system_control.laser import LASER_PERCENT_OPTIONS
from cobra_system_control.random_access_scanning import (
    PerVirtualSensorMetadata, FpsMultipleOv, NnLevelOv, INTE_TIME_US_OPTIONS,
    FrameRateOv, MaxRangeIdxMappedOv,
)
from cobra_system_control.scan_control import SnrThresholdBv, BinningOv


def precision_floor(num, precision: int = 0):
    """Rounds the provided number to the specified
    number of significant digits specified by the precision
    argument.
    """
    return float(np.true_divide(np.floor(num * 10**precision), 10**precision))


def floor10(num):
    return math.floor(num / 10) * 10


# Single-place location of valid parameter ranges and options.
# This is used for direct input into marshmallow schemas.
# Use the `SystemInfo.api_options` classmethod to convert for API endpoints.
VALID_PARAMETERS = {
    "user_tag": {"min": 0, "max": 2**PerVirtualSensorMetadata.user_tag.size - 1},
    "angle_range": {"angle_min": -45.0, "angle_max": 45.0,
                    "step_min": precision_floor(0.3, 1),
                    "step_max": precision_floor(10.0, 1)},
    "fps_multiple": {
        "min": min(FpsMultipleOv.OPTIONS),
        "max": max(FpsMultipleOv.OPTIONS),
    },
    "laser_power_percent": {
        "min": min(LASER_PERCENT_OPTIONS),
        "max": max(LASER_PERCENT_OPTIONS),
    },
    "inte_time_us": {
        "min": min(INTE_TIME_US_OPTIONS),
        "max": max(INTE_TIME_US_OPTIONS),
    },
    "max_range_m": {"choices": list(MaxRangeIdxMappedOv.OPTIONS)},
    "binning": {"choices": list(BinningOv.OPTIONS)},
    "snr_threshold": {
        "min": precision_floor(min(SnrThresholdBv.LIMITS), 1),
        "max": precision_floor(max(SnrThresholdBv.LIMITS), 1),
    },
    "nn_level": {"choices": [opt for opt in NnLevelOv.OPTIONS]},
    "interleave": {"choices": [True, False]},
    "frame_rate_hz": {
        "min": min(FrameRateOv.OPTIONS),
        "max": max(FrameRateOv.OPTIONS),
    },
    "dsp_mode": {"choices": [0, 1]},
    "hdr_threshold": {"min": 0, "max": 4095},
    "hdr_laser_power_percent": {
        "min": min(LASER_PERCENT_OPTIONS),
        "max": max(LASER_PERCENT_OPTIONS),
    },
    "hdr_inte_time_us": {
        "min": min(INTE_TIME_US_OPTIONS),
        "max": max(INTE_TIME_US_OPTIONS),
    },
}

# Fields that cannot be explicitly set
READ_ONLY_FIELDS = ("state", "system_version", "sensor_id")

# Maximum number of FOVs that are settable
MAX_N_FOV = 8

log.debug("VALID_PARAMETERS=%s", VALID_PARAMETERS)
log.debug("READ_ONLY_FIELDS=%s", READ_ONLY_FIELDS)
log.debug("MAX_N_FOV=%s", MAX_N_FOV)


def make_nested_param(param, value) -> list:
    """Ensures that the input ``value`` for ``param`` is contained within a list.
    Scalar values are coerced into a single-valued list, while the special case
    of ``angle_range`` is coerced into a nested list.
    If the input parameter is already nested in a list, this does nothing.
    """
    if param == "angle_range":
        if all(not isinstance(i, list) for i in value):
            value = [value]

        if value.count(value[0]) == len(value):
            # All angle ranges are the same - only use the first one
            value = value[:1]
    elif (param == "interleave") or (param == "hdr_threshold") or (param == "dsp_mode"):
        # Should just be one value, not a list
        # People are just gonna have to deal with this
        if isinstance(value, list):
            value = value[0]
    else:
        if not isinstance(value, list):
            value = [value]
    return value


def load_usr_json_to_system_schema() -> dict:
    """Gets the information posted in the request JSON body.
    This will only load fields that the user has specified into the output.
    Conversion of scalar values to common-length sequences happens
    in cobra-system-control.

    This must be called inside a request context.
    """
    log.debug("load_usr_json_to_system_schema called")

    usr_json = request.get_json()
    log.debug("usr_json=%s", usr_json)

    schema = SensorSchema()
    _ = schema.validate(usr_json)
    ret = schema.load(usr_json)
    log.debug("Loaded usr_json as %s", ret)
    return ret


@dc.dataclass(frozen=True)
class SystemInfo:
    """Information for a full sensor head.

    This is retained within the API and used to apply values to system-control.
    All values are stored as lists (sequences) for simple marshalling
    with the schema provided in marshmallow. Tuples are not allowed
    because they carry different behavior and don't exist in JSON.

    The ``settings_dict`` instance method removes read-only fields
    and unwraps parameter sequences into scalar values (when appropriate)
    so that the values can be immediately applied to system-control.
    """

    # Virtual Sensor angle declaration
    angle_range: List[List[int]]

    # Integer parameters
    fps_multiple: List[int]
    laser_power_percent: List[int]
    inte_time_us: List[int]
    max_range_m: List[float]
    binning: List[int]
    frame_rate_hz: List[int]

    # Bool parameters
    interleave: bool

    # Filtering parameters
    snr_threshold: List[float]
    nn_level: List[int]

    # HDR parameters
    hdr_threshold: int
    hdr_laser_power_percent: List[int]
    hdr_inte_time_us: List[int]

    # Metadata tags. system-control will provide if the user does not specify.
    user_tag: List[int]

    dsp_mode: int

    # The below are "read-only" and can only be populated from the sensor head
    system_version: dict = None
    sensor_id: str = None
    state: str = None

    def __post_init__(self):
        write_fields = SystemInfo.all_fields_except(
            *READ_ONLY_FIELDS,
            "interleave",  # not an arrayed param
            "hdr_threshold",  # not an arrayed param
            "dsp_mode",
        )

        common_len = get_common_length(
            **{field: make_nested_param(field, getattr(self, field))
               for field in write_fields}
        )

        # Convert to appropriate lengths
        for field in write_fields:
            # Get copy of contents to avoid shallow copy issues
            if field != "angle_range":
                val = make_nested_param(field, getattr(self, field))[:]
                if len(val) != common_len:
                    # Set the common length
                    val *= common_len
                    object.__setattr__(self, field, val)

            else:
                val = getattr(self, field)
                if isinstance(val[0], (int, float)):
                    # Is already a list of ints. need to wrap in list
                    val = [val]

                object.__setattr__(self, field, val)

    @classmethod
    def api_options(cls, param: str) -> dict:
        """Converts the return in ``VALID_PARAMETERS`` into a format
        consistent with the API returns.
        """
        opts = VALID_PARAMETERS[param]
        if "step_min" in opts:
            return opts
        elif "choices" in opts:
            return {"options": opts["choices"]}
        else:
            return {"low": opts["min"], "high": opts["max"]}

    @classmethod
    def all_fields_except(cls, *args) -> tuple:
        """Returns all fields except those specified in args.
        Pass no arguments to get all fields.
        """
        return tuple(f.name for f in dc.fields(cls) if f.name not in args)

    @classmethod
    def writable_field(cls) -> tuple:
        return cls.all_fields_except(*READ_ONLY_FIELDS)

    @classmethod
    def all_fields(cls) -> tuple:
        return tuple(f.name for f in dc.fields(cls))

    def settings_dict(self) -> dict:
        """Returns a dictionary form of this dataclass with
        system-control-compatible setting fields.
        """
        d = dc.asdict(self)

        # Remove read-only fields
        for field in READ_ONLY_FIELDS:
            d.pop(field)

        return d


class SensorSchema(Schema):
    """Schema for validating and translating user input to a ``SensorInfo``
    object in order to appropriately communicate with system control.
    """

    angle_range = fields.List(
        # Contents should be arrays of length 3
        fields.List(
            fields.Float(
                required=True,
                validate=validate.Range(
                    min=VALID_PARAMETERS["angle_range"]["angle_min"],
                    max=VALID_PARAMETERS["angle_range"]["angle_max"])
            ),
            validate=validate.Length(min=3, max=3),
        ),
        validate=validate.Length(max=MAX_N_FOV),
    )

    # Scalar parameters
    fps_multiple = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["fps_multiple"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    laser_power_percent = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["laser_power_percent"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    inte_time_us = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["inte_time_us"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    max_range_m = fields.List(fields.Float(
        required=True,
        validate=validate.OneOf(**VALID_PARAMETERS["max_range_m"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    binning = fields.List(fields.Integer(
        required=True,
        validate=validate.OneOf(**VALID_PARAMETERS["binning"]),
    ), validate=validate.Length(max=MAX_N_FOV))

    nn_level = fields.List(fields.Integer(
        required=True,
        validate=validate.OneOf(**VALID_PARAMETERS["nn_level"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    snr_threshold = fields.List(fields.Float(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["snr_threshold"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    frame_rate_hz = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["frame_rate_hz"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    interleave = fields.Boolean(
        required=True, validate=validate.OneOf(**VALID_PARAMETERS["interleave"]))
    dsp_mode = fields.Integer(
        required=True, validate=validate.OneOf(**VALID_PARAMETERS["dsp_mode"]))

    user_tag = fields.List(fields.Integer(
        required=True,
    ), validate=validate.Length(max=MAX_N_FOV))

    hdr_threshold = fields.Integer(required=True)
    hdr_laser_power_percent = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["hdr_laser_power_percent"]),
    ), validate=validate.Length(max=MAX_N_FOV))
    hdr_inte_time_us = fields.List(fields.Integer(
        required=True,
        validate=validate.Range(**VALID_PARAMETERS["hdr_inte_time_us"]),
    ), validate=validate.Length(max=MAX_N_FOV))

    # Read-only fields from the sensor. Considered always valid.
    # This is consistent with marshmallow's validate-only-on-load behavior.
    system_version = fields.Dict(keys=fields.Str(dump_only=True),
                                 values=fields.Str(dump_only=True))
    sensor_id = fields.Str(dump_only=True)
    state = fields.Str(dump_only=True)

    @pre_load
    def handle_input_values(
            self,
            data,
            many,  # pylint: disable=unused-argument
            **kwargs,  # pylint: disable=unused-argument
    ):
        """Marshals scalar user input into array formats for compatibility
        with the field definitions listed above, so both scalar and array
        inputs are allowed, which is not default behavior with marshmallow.

        Marshals float inputs for some parameters into proper
        values as expected by cobra-system-control

        The pylint disables above are so we don't upset under-the-hood
        functionality.
        """
        log.debug("Checking input multiplicity")

        for k, v in data.items():
            newval = make_nested_param(k, v)

            if k == "angle_range":
                adjusted_val = []
                for val in newval:
                    # Round step to one decimal
                    adjusted_val.append([val[0], val[1], precision_floor(val[2], 1)])
                newval = adjusted_val
            elif ("inte_time_us" in k) or ("laser_power_percent" in k):
                # 'in k' covers hdr as well
                # Make an int
                newval = [math.floor(x) for x in newval]
            elif k == "frame_rate_hz":
                # Make an int rounded to 10s place
                newval = [floor10(x) for x in newval]
            elif k == "snr_threshold":
                # Round to two decimals
                newval = [precision_floor(x, 1) for x in newval]
            elif k == "user_tag":
                try:
                    newval = [int(val, 16) for val in newval[:]]
                except TypeError:
                    pass

            data[k] = newval
            log.debug("Loading %s=%s", k, newval)
        return data

    @validates_schema
    def check_angle_range_step_size(self, data, **kwargs):  # pylint: disable=unused-argument
        """Checks that the step sizes are compatible with the limits set.
        marshmallow.validate.Range is not flexible enough on it's own.
        """
        key = 'angle_range'
        angle_ranges = data[key]
        for vs in angle_ranges:
            # loop through all the virtual sensors
            if vs[2] > VALID_PARAMETERS[key]['step_max']:
                raise ValidationError(
                    f'Angle step size for triplet {vs} is '
                    'greater than the maximum '
                    f'{VALID_PARAMETERS[key]["step_max"]}')
            elif vs[2] < VALID_PARAMETERS[key]['step_min']:
                raise ValidationError(
                    f'Angle step size for triplet {vs} is '
                    'less than the minimum '
                    f'{VALID_PARAMETERS[key]["step_min"]}')

    @validates_schema
    def check_common_len(self, data, **kwargs):  # pylint: disable=unused-argument
        """Checks that all input fields are of compatible input lengths.
        Specifically, this means that all input fields (which are arrays)
        that have len > 1 are equal in length.

        For example, param1 = [1, 2, 3], param2 = [3, 4, 5], etc.
        where all values are len=3 is valid.
        Similarly, param1 = [1], param2 = [1, 2, 3], param3 = [4]
        is also valid, because len=1 arrays can be safely extended.

        However, param1 = [1, 2], param2 = [3, 4, 5] is invalid, as even with
        a defining key for common lengths, it is ambiguous as to which
        fields should be either duplicated or truncated.
        For simplicity, this does __not__ consider the special case
        of arrays with a single value (e.g., [1, 1, 1, 1]).

        pylint disable on kwargs to not upset functionality
        """
        try:
            log.debug("Checking common length in schema.")
            get_common_length(
                **{k: v for k, v in data.items()
                   if k not in ['interleave', 'dsp_mode', *READ_ONLY_FIELDS]}
            )
        except ScanPatternSizeError as exc:
            log.error("Failed common length check")
            raise ValidationError(
                "The number of values defined for each parameter does "
                "not match the number of FOVs. If you are changing the number "
                "of FOVs, remember to redefine all parameter values. "
            ) from exc

    @validates_schema
    def check_all_required_present(self, data, **kwargs):  # pylint: disable=unused-argument
        """Checks that all the required fields are present in
        a scan_parameters setting post

        pylint disable on kwargs to not upset functionality
        """
        input_keys = set(data.keys())
        required_keys = set(
            ['angle_range', 'fps_multiple', 'laser_power_percent',
             'inte_time_us', 'max_range_m', 'binning', 'frame_rate_hz',
             'nn_level', 'snr_threshold', 'interleave', 'user_tag', 'dsp_mode',
             'hdr_threshold', 'hdr_laser_power_percent',
             'hdr_inte_time_us',
             ])
        try:
            assert input_keys == required_keys
        except AssertionError as exc:
            raise ValidationError(
                "The post to the scan_parameters does not contain "
                "all the required keys. Missing data for required field."
                f"missing {required_keys.difference(input_keys)}"
            ) from exc


class DataField(fields.Field):
    # pylint: disable-next=unused-argument
    def _deserialize(self, value, attr, data, **kwargs):
        """pylint disable on kwargs to not upset functionality
        """
        if isinstance(value, list) or isinstance(value, bool):
            return value
        else:
            raise ValidationError(
                f"Field should be bool or list but is {type(value)}")
