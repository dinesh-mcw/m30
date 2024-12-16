/**
 * @file SensorHeadThread.cpp
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the implementation of the SensorHeadThread class,
 * which is the superclass for the concrete V4LSensorHeadThread and
 * MockSensorHeadThread classes. The concrete classes are responsible for
 * retrieving region of interest (ROI) data and sending it to the raw to
 * depth processing code. The SensorHeadThread class contains code that
 * is common to the two concrete classes and supports the following
 * functionality:
 *    1. Communicating with the main thread
 *    2. Sending MIPI frame and ROI data to the Raw to Depth code
 */

#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <climits>
#include <vector>
#include <string>
#include <sstream>
#include "frontend.h"
#include "LumoLogger.h"
#include "LumoTimers.h"
#include "SensorHeadThread.h"

/**
 * @brief Construct a SensorHeadThread
 *
 * @param headNum The head number for this thread, only allowed to be 0 on NCB
 * @param outPrefix The path prefix for the raw output files when raw streaming is enabled. Set to nullptr to disable raw streaming to files
 * @param outMaxRois The maximum number of ROIs that will be output in a session.
 * @param calFileName The name of the mapping table file. Set to nullptr to use the system control provided mapping table
 * @param pixmapFileName The name of the pixel map file. Set to nullptr to use the system control provided pixel map
 * @param maxNetFrames The maximum number of output frames that will be sent over the network. Set to zero for unlimited frames
 * @param basePort The TCP base port number for the point cloud data. The ports used will be basePort, basePort + 1, ... basePort + 7
 */
SensorHeadThread::SensorHeadThread(int headNum, const char *outPrefix, int outMaxRois, const char *calFileName, const char *pixmapFileName, int maxNetFrames, int basePort) :
    m_headNum(headNum),
    m_rawToFov(std::make_shared<RawToFovs>(headNum)),
    m_netWrappers({}),
    m_sendFrame_Timers("sendFrame loop"),
    m_outPrefix(outPrefix),
    m_outMaxRois(outMaxRois),
    m_maxNetFrames(maxNetFrames),
    m_outSessionNum(0),
    m_outRoiNum(0),
    m_outStreaming(false),
    m_calFileName(calFileName),
    m_pixmapFileName(pixmapFileName),
    m_calLoaded(false),
    m_rawStreamingSuspended(true),
    m_firstRawRoi(false),
    m_stopped(false) {

    // create socket pair
    std::array<int, 2> socks = { 0, 0 };
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socks.data()) < 0) {
        LLogErr("socketpair:errno=" << errno);
        m_waitFd = -1;
        m_trigFd = -1;
    } else {
        m_waitFd = socks[0];
        m_trigFd = socks[1];
        LLogDebug("m_waitFd=" << m_waitFd << ",m_trigFd=" << m_trigFd);
    }

    // Spin up all our output streams up front (for now -- want to stress performance)
    // TODO (Do this on the fly/per-FOV?)
    for (unsigned int fov = 0; fov < FOV_STREAMS_PER_HEAD; fov++) {
        m_netWrappers[fov] = new LidarPipeline::CobraNetPipelineWrapper((int)(fov + FOV_STREAMS_PER_HEAD * headNum), maxNetFrames, basePort);
    }

    // Net wrapper for raw data (will be instantiated at runtime)
    m_rawDataNetWrapper = nullptr;
}

SensorHeadThread::~SensorHeadThread() {
    if (m_waitFd >= 0) {
        close(m_waitFd);
        m_waitFd = -1;
    }
    if (m_trigFd >= 0) {
        close(m_trigFd);
        m_trigFd = -1;
    }
}

/**
 * @brief Sends a control byte from the main thread to the sensor head thread. This function is called from the handleControlByte function.
 *
 * @param controlByte The command to be sent to the sensor head thread
 */
void SensorHeadThread::notifyThread(uint8_t controlByte) const {
    ssize_t ret = 0;
    do {
        ret = write(m_trigFd, &controlByte, sizeof(controlByte));
        if (ret < 0) {
            if (errno != EINTR) {
                LLogErr("send_notification:errno=" << errno << ":failed to send notification; ignoring");
                return;
            }
        }
    } while (ret < 0);
}

/**
 * @brief Loads the mapping table and pixel map from the filesystem. This function is called from the sensor head thread.
 */
void SensorHeadThread::reloadCalibrationData() {
    m_rawToFov->reloadCalibrationData(std::string(m_calFileName), std::string(m_pixmapFileName));
    m_calLoaded = true;
    LLogInfo("reload_cal:headNum=" << m_headNum << ",calFileName=" << m_calFileName << ",pixmapFileName=" << m_pixmapFileName);
}

/**
 * @brief Synchronously executes a control byte
 *
 * This function is called from the master thread. It does the following:
 * 1. Sends the control byte to the sensor head thread
 * 2. Waits for sensor head thread to execute the command and then reply back
 *
 * @param controlByte The command to be sent to the sensor head thread
 */
void SensorHeadThread::handleControlByte(uint8_t controlByte) {

    // tell thread to load cal data before streaming if it hasn't been loaded yet
    if ((controlByte & THR_NOTIFY_COMMAND_MASK) == THR_NOTIFY_START_STREAMING && !m_calLoaded) {
        controlByte = THR_NOTIFY_START_STREAMING_WITH_RELOAD | (controlByte & THR_NOTIFY_PARAM_MASK);
    }

    notifyThread(controlByte);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_trigFd, &fds);
    struct timeval timeout {};
    timeout.tv_sec = THR_CONTROL_ACK_TIMEOUT_MS / MILLISECONDS_PER_SECOND;
    timeout.tv_usec = static_cast<unsigned long>(THR_CONTROL_ACK_TIMEOUT_MS % MILLISECONDS_PER_SECOND) * MICROSECONDS_PER_MILLISECOND;
    int ret = select(m_trigFd + 1, &fds, NULL, NULL, &timeout );
    if (ret == 0) {
        LLogErr("new_session_ack_timeout:timeoutMs=" << THR_CONTROL_ACK_TIMEOUT_MS);
        return;
    }
    uint8_t byte;
    if (read(m_trigFd, &byte, sizeof(byte)) < 0) {
        LLogErr("new_session_ack_read:errno=" << errno << ":failure reading session start ack");
        return;
    }
    if (byte != controlByte) {
        LLogErr("new_session_ack_val:byte=" << byte << ",expected=" << controlByte << ":wrong ack value");
        return;
    }
}

/**
 * @brief Sends the command byte acknowledgement from the sensor head thread to main thread. This function is called from the sensor head thread after it executes the command.
 *
 * @param controlByte The command to be sent back to the main thread
 */
void SensorHeadThread::ackControlByte(uint8_t controlByte) const {
    int ret = static_cast<int>(write(m_waitFd, &controlByte, sizeof(controlByte)));
    if (ret < 0) {
        LLogErr("ack_new_session:errno=" << errno);
    }
}

/**
 * @brief Notify the main thread that this sensor head thread has shut down
 *
 * This function is called from the sensor head thread
 *
 * A control byte sent from the sensor head thread that is not an acknowledgement to a command sent
 * from the main thread notifies the main thread that the sensor head thread has shut down gracefully
 */
void SensorHeadThread::notifyShutdown() const {
    uint8_t zero = 0;
    LLogInfo("shutdown_notify:m_waitFd=" << m_waitFd);
    if (write(m_waitFd, &zero, sizeof(zero)) < 0) {
        LLogErr("notify_shutdown_failed:errno=" << errno);
    }
}

/**
 * @brief Tell the sensor head to shut down
 *
 * This function is called from the app's signal handler to gracefully shut down all of the threads
 */
void SensorHeadThread::exitThread() {
    notifyThread(THR_NOTIFY_EXIT_THREAD);
    m_rawToFov->shutdown();
}

/**
 * @brief Receives a control byte from the main thread and executes commands common to all sensor head thread types
 *
 * This function is called from the sensor head thread when the wait file descriptor has read data
 *
 * @return The control byte received from the main thread
 */
uint8_t SensorHeadThread::receiveNotification() {
    uint8_t note;
    ssize_t ret = 0;
    do {
        ret = recv(m_waitFd, &note, sizeof(note), MSG_DONTWAIT);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return THR_NOTIFY_NOTHING_HAPPENED;
            }
            if (errno != EINTR) {
                LLogErr("recv_notification:errno=" << errno <<
                        ":failed to receive notification; exiting thread");
                return THR_NOTIFY_EXIT_THREAD;
            }
        }
    } while(ret < 0);

    // This is where we reload the calibration data
    if (note == THR_NOTIFY_RELOAD_CAL_DATA ||
        (note & THR_NOTIFY_COMMAND_MASK) == THR_NOTIFY_START_STREAMING_WITH_RELOAD) {
        reloadCalibrationData();
    } else if (note == THR_NOTIFY_START_RAW_STREAMING) {
        if (m_rawDataNetWrapper != nullptr) {
            LLogInfo("raw_stream:head=" << m_headNum << ":started raw streaming");
            const unsigned int numROIsInBuffer = 91;
            m_rawDataNetWrapper = new LidarPipeline::CobraRawDataNetPipelineWrapper(m_headNum, m_maxNetFrames, numROIsInBuffer);
        } else {
            LLogInfo("raw_stream_running:head=" << m_headNum << ":raw streaming already running");
        }
        if (m_rawStreamingSuspended) {
            m_rawStreamingSuspended = false;
            m_firstRawRoi = true;
        }
    } else if (note == THR_NOTIFY_SUSPEND_RAW_STREAMING) {
        m_rawStreamingSuspended = true;
    }

    if (m_outPrefix != nullptr) {
        if ((note & THR_NOTIFY_COMMAND_MASK) == THR_NOTIFY_START_STREAMING) {
            m_outSessionNum++; // session starts with 1 unfortunately
            m_outRoiNum = 0;
            m_outStreaming = true;
        }
    }

    return note;
}

#define OUTPUT_FILE_MODE 0666

/**
 * @brief Sends a single ROI to raw to depth
 *
 * @param dataU8            Pointer to the data buffer containing the ROI
 * @param dataSizePerRoi    Size of the ROI data in bytes
 */
void SensorHeadThread::sendRoi(const uint8_t *dataU8, unsigned int dataSizePerRoi) {
    // output the data to an output file
    if (m_outStreaming) {

        std::stringstream name;
        name << m_outPrefix << '_' <<
                std::setfill('0') << std::setw(1) << m_headNum << '_' <<
                std::setw(2) << m_outSessionNum << '_' <<
                std::setw(4) << m_outRoiNum << ".bin";
        LLogDebug("open_outfile:name=" << name.str());

        // NOLINTNEXTLINE(hicpp-vararg) calling LINUX vararg API
        int outFd = open(name.str().c_str(), (unsigned int)O_CREAT | (unsigned int)O_TRUNC | (unsigned int)O_WRONLY, OUTPUT_FILE_MODE);
        if (outFd < 0) {
            LLogErr("open_outfile:path=" << name.str() <<
                    ",errno=" << errno <<
                    ":can't open output file; streaming to file disabled");
        } else {
            ssize_t pos = 0;
            while (pos < dataSizePerRoi) {
                ssize_t bytesWritten = write(outFd, dataU8 + pos, dataSizePerRoi - pos);
                if (bytesWritten < 0) {
                    LLogErr("write_outfile:errno=" << errno << ":can't write to output file");
                    break;
                }
                pos += bytesWritten;
            }
            close(outFd);
            m_outRoiNum++;
            if (m_outRoiNum >= m_outMaxRois) {
                m_outStreaming = false;
            }
        }
    }

    if (m_rawDataNetWrapper != nullptr && !m_rawStreamingSuspended) {
        m_rawDataNetWrapper->HandInCobraROI((const char *) dataU8, (int)dataSizePerRoi, m_firstRawRoi);
        m_firstRawRoi = false;
    }

    {
        constexpr uint32_t REPORTING_RATIO = { 1000 }; // report every 100 frames
        auto localTimer = LumoTimers::ScopedTimer(m_sendFrame_Timers, "rtd", REPORTING_RATIO); //alternative to small scope: wrap processRoi() in timers.start()/stop()
        m_rawToFov->processRoi((uint16_t *)dataU8, dataSizePerRoi);
    }

    for(auto fovIdx: m_rawToFov->fovsAvailable()) {
        auto fovData = m_rawToFov->getData(fovIdx);

        m_netWrappers[fovIdx]->HandInCobraDepth(fovData);
    }
}

/**
 * @brief Send a (possibly aggregated) frame of MIPI data to raw to depth
 *
 * @param data              Pointer to buffer data
 * @param dataSizePerRoi    Size of the data in bytes
 * @param numRoisInFrame    Number of ROIs aggregated in the frame
 */
void SensorHeadThread::sendMipiFrame(const uint8_t *data, uint32_t dataSizePerRoi, uint32_t numRoisInFrame) {
    if (data == nullptr) {
        LLogWarning("bad_frame_data:data=nullptr:ignoring frame");
        return;
    }

    auto *dataU8 = (uint8_t *)data;

    for (unsigned int roi = 0; roi < numRoisInFrame; roi++) {
        sendRoi(dataU8, dataSizePerRoi);
        dataU8 += dataSizePerRoi;
    }

    m_sendFrame_Timers.report();
}

/**
 * @brief Get the wait file descriptor
 *
 * @returns File descriptor for the socket used by the sensor head thread to communicate with the main thread
 */
int SensorHeadThread::getWaitFd() const {
    return m_waitFd;
}

/**
 * @brief Get the trigger file descriptor
 *
 * @returns File descriptor for the socket used by the main thread to communicate with the sensor head thread
 */
int SensorHeadThread::getTrigFd() const {
    return m_trigFd;
}
