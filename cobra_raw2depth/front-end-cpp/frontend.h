#pragma once

/**
 * @file frontend.h
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the formats and bit fields for the control bytes that are used to
 *   1. Communicate between the System Config and Control code and the front end
 *   2. Communicate between the main thread and the sensor head threads 
 *
 * Control byte as sent to thread differs from command from the front end controller app (fectrl)
 *
 * fectrl command formats:
 *
 * 00FFFFHH -- start streaming with format FFFF on head HH
 * 010000HH -- stop streaming on head HH
 * 100000HH -- reload calibration data on head HH
 * 01001DDD -- set the debug level to DD
 * 010100HH -- start raw streaming on head HH
 * 010101HH -- suspend raw streaming on head HH
 * 010110HH -- resynchronize with 1PPS on head HH
 *
 * We can add a bunch more commands 
 *
 * New thread to thread format; head number is not needed
 *
 * 0000FFFF -- start streaming with format FFFF
 * 0001FFFF -- start streaming with format FFFF and reload cal data
 * 0010XXXX to 1110XXXX -- add up to 12 more commands with parameters
 * 11110000 -- stop streaming
 * 11110001 -- reload calibration data
 * 11110010 -- exit thread
 * 11110011 -- nothing happened
 * 11110100 -- start raw streaming
 * 11110101 -- suspend raw streaming
 * 11110110 to 11111110 -- add up to 9 more commands without parameters
 * 11111111 -- error
 */

constexpr unsigned int FEC_NOTIFY_START_STREAMING                { 0x00 };
constexpr unsigned int FEC_NOTIFY_START_STREAMING_MASK           { 0xc0 };
constexpr unsigned int FEC_NOTIFY_FORMAT_MASK                    { 0x3c };
constexpr unsigned int FEC_NOTIFY_FORMAT_SHIFT                   { 0x02 };
constexpr unsigned int FEC_NOTIFY_STOP_STREAMING                 { 0x40 };
constexpr unsigned int FEC_NOTIFY_STOP_STREAMING_MASK            { 0xfc };
constexpr unsigned int FEC_NOTIFY_RELOAD_CAL_DATA                { 0x80 };
constexpr unsigned int FEC_NOTIFY_RELOAD_CAL_DATA_MASK           { 0xfc };
constexpr unsigned int FEC_NOTIFY_HEADNUM_MASK                   { 0x03 };
constexpr unsigned int FEC_NOTIFY_SET_DEBUG_LEVEL                { 0x48 };
constexpr unsigned int FEC_NOTIFY_SET_DEBUG_LEVEL_MASK           { 0xf8 };
constexpr unsigned int FEC_NOTIFY_DEBUG_LEVEL_MASK               { 0x07 };
constexpr unsigned int FEC_NOTIFY_START_RAW_STREAMING            { 0x50 };
constexpr unsigned int FEC_NOTIFY_START_RAW_STREAMING_MASK       { 0xfc };
constexpr unsigned int FEC_NOTIFY_SUSPEND_RAW_STREAMING          { 0x54 };
constexpr unsigned int FEC_NOTIFY_SUSPEND_RAW_STREAMING_MASK     { 0xfc };
constexpr unsigned int FEC_NOTIFY_SYNC_TIME                      { 0x58 };
constexpr unsigned int FEC_NOTIFY_SYNC_TIME_MASK                 { 0xfc };
constexpr unsigned int FEC_NOTIFY_ERROR                          { 0xff }; // only gets returned

constexpr unsigned int THR_NOTIFY_COMMAND_MASK                   { 0xf0 };
constexpr unsigned int THR_NOTIFY_PARAM_MASK                     { 0x0f };
constexpr unsigned int THR_NOTIFY_START_STREAMING                { 0x00 };
constexpr unsigned int THR_NOTIFY_START_STREAMING_WITH_RELOAD    { 0x10 };
constexpr unsigned int THR_NOTIFY_STOP_STREAMING                 { 0xf0 };
constexpr unsigned int THR_NOTIFY_RELOAD_CAL_DATA                { 0xf1 };
constexpr unsigned int THR_NOTIFY_EXIT_THREAD                    { 0xf2 };
constexpr unsigned int THR_NOTIFY_NOTHING_HAPPENED               { 0xf3 };
constexpr unsigned int THR_NOTIFY_START_RAW_STREAMING            { 0xf4 };
constexpr unsigned int THR_NOTIFY_SUSPEND_RAW_STREAMING          { 0xf5 };

constexpr unsigned int LISTEN_PORT                              { 1234 };

constexpr int MILLISECONDS_PER_SECOND                       { 1000 };
constexpr int MICROSECONDS_PER_MILLISECOND                  { 1000 };

typedef enum {
    STARTUP_MODE_NO_TIMESYNC,
    STARTUP_MODE_PTP_TIMESYNC,
    STARTUP_MODE_PPS_TIMESYNC
} startup_mode_enum_t;
