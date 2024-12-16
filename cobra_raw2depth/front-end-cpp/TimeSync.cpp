/**
 * @file TimeSync.cpp
 * @brief This file provides the implementation for the time synchronization class that
 *        contains all of the time synchronzation functionality
 *
 * @copyright Copyright (C) 2024 Lumotive, Inc. All rights reserved
 */
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <linux/pps.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <array>
#include <thread>
#include "LumoLogger.h"
#include "TimeSync.h"

#define NS_PER_SEC 1000000000
#define PPS_TIMEOUT_SEC 3 // time out fetch after 3 seconds

constexpr unsigned int PPS_DEVICE_PTP       { 0 };
constexpr unsigned int PPS_DEVICE_PPS       { 1 };

#define CMD_STOP_NTP "/bin/systemctl stop ntpd ntpdate"
#define CMD_SET_MUX_TO_PTP "/usr/bin/gpioset 8 13=1"
#define CMD_SET_MUX_TO_PPS "/usr/bin/gpioset 8 13=0"
#define CMD_START_PTP4L "/bin/systemctl start ptp4l"
#define CMD_CHECK_FOR_GM_CLOCK "/usr/sbin/pmc -u -b 0 \"get time_status_np\" | grep gmPresent | grep true"
#define CMD_CHECK_FOR_PTP4L_CONVERGENCE "/usr/sbin/pmc -u -b 0 \"get time_status_np\" | grep master_offset | awk '{ print ($2 < 0.0 ? -$2 : $2) < 100000 }' | grep 1"
#define CMD_START_PHC2SYS "/bin/systemctl start phc2sys"
#define CMD_CHECK_FOR_SYSTEM_CLOCK_SYNC "/usr/bin/timedatectl status | grep \"System clock synchronized:\" | grep yes"
#define CMD_CHECK_FOR_PPS1_PRESENT "/bin/grep -v \"0.000000000#0\" /sys/class/pps/pps1/assert"
#define CMD_ENABLE_PTP_PPS "/bin/echo 1 > /sys/class/ptp/ptp0/pps_enable"
#define CMD_DISABLE_PTP_PPS "/bin/echo 0 > /sys/class/ptp/ptp0/pps_enable"
#define CMD_DISABLE_SYNC_FPGA "/usr/sbin/i2ctransfer -y 2 w3@0x10 0x84 0x0d 0x00"
#define CMD_ENABLE_SYNC_FPGA "/usr/sbin/i2ctransfer -y 2 w3@0x10 0x84 0x0d 0x08"
#define RETRY_INTERVAL_USEC (1000000)
#define NO_SYNC_TIMESTAMP_RESET_INTERVAL_USEC (1001000)

constexpr unsigned int NUM_EXP_ELEMENTS { 9 };

std::array<int, NUM_EXP_ELEMENTS> s_exp_backoff { 1, 2, 5, 10, 60, 120, 300, 600, 3600 }; // NOLINT(readability-magic-numbers) Clearer with constants

/**
 * @brief Internal function to execute a command in the shell and log any status other than exiting normally
 * @param cmd C string containing command to execute
 * @return 0 if the command succeeds and returns 0
 * @return positive number if the command exits normally with a nonzero return code
 * @return -1 if any other failure occurs
 */
int TimeSync::systemInternal(const char *command)
{
    int ret = 0;

    ret = system(command);
    if (ret < 0) {
        LLogErr("system_internal_child:errno=" << errno);
    } else {
        if (WIFEXITED(ret)) {
            ret = WEXITSTATUS(ret);
        } else {
            if (WIFSIGNALED(ret)) {
                LLogErr("system_internal_signal:signal=" << WTERMSIG(ret));
                ret = -1;
            } else {
                LLogErr("system_internal_other:ret=" << ret);
                ret = -1;
            }
        }
    }
    return ret;
}

/**
 * @brief Internal function to repeatedly execute a command in the shell until the command succeeds.
 *        Messages are logged to syslog on failures, but the messages get exponentially less frequent
 *        The function returns when the command runs with an exit code of 0
 * @param command C string containing command to execute
 * @param name C string containing a name for the command that is logged to syslog on success or failure
 */
void TimeSync::waitForCommand(const char *command, const char *name)
{
    bool done = false;
    unsigned int tries = 0;
    unsigned int waitIdx = 0;

    while (!done) {
        int ret = systemInternal(command);
        tries++;
        if (ret < 0) {
            LLogErr(name << "_failed:errno=" << errno << ",tries=" << tries);
        }
        if (ret == 0) {
            done = true;
        } else {
            if (waitIdx < s_exp_backoff.size() && tries >= s_exp_backoff[waitIdx]) {
                LLogWarning(name << "_not_successful:ret=" << ret << ",tries=" << tries);
                waitIdx++;
            }
            usleep(RETRY_INTERVAL_USEC);
       }
    }
    LLogInfo(name << "_succeeded:tries=" << tries);
}

#define TSTAMP_SYNC_AUX_EN_OFFSET   0x840dU
#define TSTAMP_SYNC_AUX_EN_POS      3U
#define TSTAMP_SYNC_AUX_EN_MASK     0x08U
#define SCAN_TSTAMP_ENABLE_OFFSET   0x8401U
#define SCAN_TSTAMP_ENABLE_POS      6U
#define SCAN_TSTAMP_ENABLE_MASK     0x40U
#define HIGH_BYTE_SHIFT             8U
#define LOW_BYTE_MASK               0xffU
#define SET_FPGA_FIELD(_fd,_i2cAddr,_reg,_val) setFpgaField(_fd,_i2cAddr,_reg##_OFFSET,_reg##_POS,_reg##_MASK,_val)


/**
 * @brief Internal function to set a field in a register in the sensor head FPGA
 *        This function reads the register, applies the value with mask to the register, and writes the new register contents back again
 *        The SET_FPGA_FIELD macro simplifies the function call so you can refer to the field by its prefix
 *        Errors are logged to syslog.
 * @param i2cfd File description of the opened i2c-dev device
 * @param i2cAddr The i2c address of the FPGA
 * @param offset The address of the FPGA register to which the function will write
 * @param pos The bit position of least significant bit of the field to which the function will write
 * @param mask The bit mask of the field
 * @return 0 if the i2c operations succeeded, -1 if it didn't.
 */
int TimeSync::setFpgaField(int i2cfd, unsigned int i2cAddr, unsigned int offset, unsigned int pos, unsigned int mask, unsigned int newValue)
{
    std::array<uint8_t, 2>addr {};
    addr[0] = (uint8_t)(offset >> 8U);      // NOLINT(readability-magic-numbers) Clearer with constants
    addr[1] = (uint8_t)(offset & 0xffU);    // NOLINT(readability-magic-numbers) Clearer with constants
    uint8_t readValue;

    std::array<struct i2c_msg, 2> msg{};

    msg[0].addr = i2cAddr & 0xffU;          // NOLINT(readability-magic-numbers) Clearer with constants
    msg[0].flags = 0;
    msg[0].len = addr.size();
    msg[0].buf = addr.data();
    msg[1].addr = i2cAddr & 0xffU;          // NOLINT(readability-magic-numbers) Clearer with constants
    msg[1].flags = I2C_M_RD;
    msg[1].len = sizeof(uint8_t);
    msg[1].buf = &readValue;

    struct i2c_rdwr_ioctl_data iocdat = { .msgs = msg.data(), .nmsgs = msg.size() };

    if (ioctl(i2cfd, I2C_RDWR, &iocdat) < 0) {                     // NOLINT(hicpp-vararg) Calling Linux vararg API
        LLogErr("i2c_read_failed:offset=" << offset << ",errno=" << errno);
        return -1;
    }

    std::array<uint8_t, 3>wbuf {};
    wbuf[0] = (uint8_t)(offset >> 8U);      // NOLINT(readability-magic-numbers) Clearer with constants
    wbuf[1] = (uint8_t)(offset & 0xffU);    // NOLINT(readability-magic-numbers) Clearer with constants
    wbuf[2] = (uint8_t)(((unsigned int)readValue & ~mask) | ((newValue << pos) & mask));

    msg[0].addr = i2cAddr & 0xffU;          // NOLINT(readability-magic-numbers) Clearer with constants
    msg[0].flags = 0;
    msg[0].len = wbuf.size();
    msg[0].buf = wbuf.data();

    iocdat.msgs = msg.data();
    iocdat.nmsgs = 1;

    if (ioctl(i2cfd, I2C_RDWR, &iocdat) < 0) {                     // NOLINT(hicpp-vararg) Calling Linux vararg API
        LLogErr("i2c_write_failed:offset=" << offset << ",errno=" << errno);
        return -1;
    }
    return 0;
}

#define I2C_DEVICE "/dev/i2c-2"

/**
 * @brief Internal function that initializes the FPGA if time synchronization is disabled
 *        This is called from syncTime if the startup mode is STARTUP_MODE_NO_TIMESYNC
 *
 * @param i2cAddress address of the sensor head FPGA on the i2c bus
 */
void TimeSync::syncNoTimeSync(unsigned int i2cAddress)
{
    int i2cFd = open(I2C_DEVICE, O_RDWR);                       // NOLINT(hicpp-vararg) Calling Linux vararg API
    if (i2cFd < 0) {
        LLogErr("i2c_device_open:dev=" << I2C_DEVICE << ",errno=" << errno);
        return;
    }

    // turn on auxiliary time stamp clock source
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, TSTAMP_SYNC_AUX_EN, 1) < 0) {
        close(i2cFd);
        return;
    }

    // reset fpga timestamps by turning off timestamping ...
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, SCAN_TSTAMP_ENABLE, 0) < 0) {
        close(i2cFd);
        return;
    }

    usleep(NO_SYNC_TIMESTAMP_RESET_INTERVAL_USEC);

    // ... and turning on timestamping again
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, SCAN_TSTAMP_ENABLE, 1) < 0) {
        close(i2cFd);
        return;
    }

    close(i2cFd);
}

/**
 * @brief Starts the FPGA timestamp servo and returns the UTC offset to add to the FPGA timestamp
 *        to get the real time
 * @param i2cAddress address of the sensor head FPGA on the i2c bus
 * @return The offset that can be added to the FPGA timestamp in the metadata for an ROI to get the real time the ROI occurred
 */
uint64_t TimeSync::syncTime(unsigned int i2cAddress)
{
    // ignore request if pps device has not been set
    unsigned int ppsDevice = PPS_DEVICE_PTP;
    if (m_startMode == STARTUP_MODE_PTP_TIMESYNC) {
        ppsDevice = PPS_DEVICE_PTP;
    } else if (m_startMode == STARTUP_MODE_PPS_TIMESYNC) {
        ppsDevice = PPS_DEVICE_PPS;
    } else {
        if (m_startMode == STARTUP_MODE_NO_TIMESYNC) {
            // just set up the FPGA but don't calculate a time offset
            syncNoTimeSync(i2cAddress);
        } else {
            LLogErr("unknown start mode " << m_startMode);
        }
        return 0;
    }

    // construct pps device name
    std::stringstream ppsName;
    ppsName << "/dev/pps" << ppsDevice;

    LLogInfo("time_sync:pps_device=" << ppsName.str() << ":synchronizing time");

    // open pps device
    int ppsFd = open(ppsName.str().c_str(), O_RDWR);            // NOLINT(hicpp-vararg) Calling Linux vararg API
    if (ppsFd < 0) {
        LLogErr("pps_device_open:dev=" << ppsName.str() << ",errno=" << errno);
        return 0;
    }

    int i2cFd = open(I2C_DEVICE, O_RDWR);                       // NOLINT(hicpp-vararg) Calling Linux vararg API
    if (i2cFd < 0) {
        LLogErr("i2c_device_open:dev=" << I2C_DEVICE << ",errno=" << errno);
        close(ppsFd);
        return 0;
    }

    // turn off auxiliary time stamp clock source
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, TSTAMP_SYNC_AUX_EN, 0) < 0) {
        // sorry, linting rules don't allow goto; when modifying be sure that both files are closed on a failure
        close(ppsFd);
        close(i2cFd);
        return 0;
    }

    // turn off timestamping
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, SCAN_TSTAMP_ENABLE, 0) < 0) {
        close(ppsFd);
        close(i2cFd);
        return 0;
    }

    // wait for the pps event
    struct pps_fdata fdata {};
    // memset(&fdata, 0, sizeof(fdata));            // striking this line because C++ has already cleared the structure
    fdata.timeout.sec = PPS_TIMEOUT_SEC;
    fdata.timeout.nsec = 0;
    fdata.timeout.flags = 0;

    if (ioctl(ppsFd, PPS_FETCH, &fdata) < 0) {      // NOLINT(hicpp-vararg) Calling Linux vararg API
        LLogErr("fetch_failed:errno=" << errno);
        close(ppsFd);
        close(i2cFd);
        return 0;
    }

    // turn on timestamping
    if (SET_FPGA_FIELD(i2cFd, i2cAddress, SCAN_TSTAMP_ENABLE, 1) < 0) {
        close(ppsFd);
        close(i2cFd);
        return 0;
    }

    // wait for the next pps even when the FPGA will get synchronized
    memset(&fdata, 0, sizeof(fdata));
    fdata.timeout.sec = PPS_TIMEOUT_SEC;
    fdata.timeout.nsec = 0;
    fdata.timeout.flags = 0;

    if (ioctl(ppsFd, PPS_FETCH, &fdata) < 0) {      // NOLINT(hicpp-vararg) Calling Linux vararg API
        LLogErr("fetch2_failed:errno=" << errno);
        close(ppsFd);
        close(i2cFd);
        return 0;
    }

    close(ppsFd);
    close(i2cFd);

    uint64_t offset = fdata.info.assert_tu.sec;

    // add 1 second if 1PPS is ahead of NTP
    if (fdata.info.assert_tu.nsec > NS_PER_SEC / 2) {
        offset += 1;
    }

    return offset;

}

/**
 * @brief Destructor for the time synchronization object, which cancels and joins the thread that initializes the OS for time synchronization
 */
TimeSync::~TimeSync() {
    if (m_threadP != nullptr) {
        pthread_cancel(m_threadP->native_handle());
        m_threadP->join();
    }
}

/**
 * @brief Internal function that initializes the OS to enable the PTP servo (ptp4l), system clock synchronization (phc2sys) and the PTP based 1PPS device
 */
void *TimeSync::startPtpTimesync()
{
    // Turn off ntpd and ntpdate
    waitForCommand(CMD_STOP_NTP, "stop_ntp");

    // Set the mux select GPIO to feed the PTP generated PPS to the FPGA
    waitForCommand(CMD_SET_MUX_TO_PTP, "set_mux_to_PTP");

    // Turn off pps0
    waitForCommand(CMD_DISABLE_PTP_PPS, "disable_ptp_pps");

    // Start the ptp4l daemon
    waitForCommand(CMD_START_PTP4L, "start_ptp4l");

    // Wait until a grandmaster clock appears in the system
    waitForCommand(CMD_CHECK_FOR_GM_CLOCK, "check_for_gm_clock");

    // Wait until the ptp4l has converged
    waitForCommand(CMD_CHECK_FOR_PTP4L_CONVERGENCE, "check_for_ptp4l_convergence");

    // Start phc2sys
    waitForCommand(CMD_START_PHC2SYS, "start_phc2sys");

    // Wait until system clock is synchronized
    waitForCommand(CMD_CHECK_FOR_SYSTEM_CLOCK_SYNC, "check_for_system_clock_sync");

    // Turn on pps0 based on ptp clock
    waitForCommand(CMD_ENABLE_PTP_PPS, "enable_ptp_pps");

    LLogInfo("ptp_timesync_initialization_successful");

    // Mark system as initialized
    m_initialized = true;

    return nullptr;
}

/**
 * @brief Internal function that checks that the OS is ready to support time synchronization using an external 1PPS source together with NTP
 */
void *TimeSync::startPpsTimesync()
{
    // Set the mux select GPIO to feed the external PPS to the FPGA
    waitForCommand(CMD_SET_MUX_TO_PPS, "set_mux_to_PPS");

    // Make sure PPS1 is pulsing
    waitForCommand(CMD_CHECK_FOR_PPS1_PRESENT, "check_for_pps1_present");

    // Wait until system clock is synchronized indicating NTP is working
    waitForCommand(CMD_CHECK_FOR_SYSTEM_CLOCK_SYNC, "check_for_system_clock_sync");

    LLogInfo("external_pps_timesync_initialization_successful");

    // Mark system as initialized
    m_initialized = true;

    return nullptr;
}

/**
 * @brief TimeSync constructor
 *
 * @param startMode The startup mode of the front end, which specifies whether the time synchronization is based on PTP, the external 1PPS, or there is no time synchronization.
 */
TimeSync::TimeSync(startup_mode_enum_t startMode) :
    m_startMode(startMode),
    m_initialized(false),
    m_threadP(nullptr)
{
    if (startMode == STARTUP_MODE_PTP_TIMESYNC) {
        m_threadP = std::make_unique<std::thread>([this](){ this->startPtpTimesync(); });
    } else if (startMode == STARTUP_MODE_PPS_TIMESYNC) {
        m_threadP = std::make_unique<std::thread>([this](){ this->startPpsTimesync(); });
    } else if (startMode == STARTUP_MODE_NO_TIMESYNC) {
        m_initialized = true;
        LLogInfo("no_timesync:no time synchronization requested; using 25 MHz FPGA clock");
    } else {
        LLogErr("unexpected mode in start_timesync " << startMode);
    }
}

