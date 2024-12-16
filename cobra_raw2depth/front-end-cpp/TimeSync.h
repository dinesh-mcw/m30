/**
 * @file TimeSync.h
 * @brief This file provides the interface for the time synchronization class that
 *        contains all of the time synchronzation functionality
 *
 * @copyright Copyright (C) 2024 Lumotive, Inc. All rights reserved
 */

#pragma once

#include "frontend.h"
#include <atomic>
#include <memory>
/**
 * @brief TimeSync class provides time synchronization capabilities. More specifically it provides the following functionality:
 *        1. Initializes PTP daemons when the startup mode is STARTUP_MODE_PTP_TIMESYNC
 *        2. Checks that the system clock is synchronized in startup modes except STARTUP_MODE_NO_TIMESYNC
 *        3. Synchronizes the timestamp that is send to R2D with UTC time (unless the startup mode is STARTUP_MODE_NO_TIMESYNC)
 *        4. Informs the V4LSensorHeadThread that time synchronization is available
 */
class TimeSync {
public:
    explicit TimeSync(startup_mode_enum_t startMode);
    TimeSync(TimeSync& shThread) = delete;
    TimeSync(TimeSync&& shThread) = delete;
    TimeSync& operator=(const TimeSync&) = delete;
    TimeSync& operator=(TimeSync&&) = delete;
    virtual ~TimeSync();
    uint64_t syncTime(unsigned int i2cAddress);
    bool initialized() { return m_initialized; }
private:
    static int systemInternal(const char *command);
    static void waitForCommand(const char *command, const char *name);
    static int setFpgaField(int i2cfd, unsigned int i2cAddr, unsigned int offset, unsigned int pos, unsigned int mask, unsigned int newValue);
    static void syncNoTimeSync(unsigned int i2cAddress);
    void *startPtpTimesync();
    void *startPpsTimesync();

    startup_mode_enum_t m_startMode;
    std::atomic_bool m_initialized;
    std::unique_ptr<std::thread> m_threadP;
};


