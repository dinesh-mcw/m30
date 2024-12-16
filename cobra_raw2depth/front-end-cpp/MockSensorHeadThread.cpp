/**
 * @file: MockSensorHeadThread.cpp
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the implementation of the MockSensorHeadThread
 *        class, a subclass of the SensorHeadThread class.
 */

#include "SensorHeadThread.h"
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <climits>
#include "LumoLogger.h"
#include "frontend.h"
#include "MockSensorHeadThread.h"

/**
 * @brief Construct a MockSensorHeadThread
 *
 * @param headNum The head number for this thread, only allowed to be 0 on NCB
 * @param mockPathPrefix The path prefix for the raw ROI input files that are sent to raw to depth
 * @param mockDelayTimeMs The time delay in milliseconds between the ROIs that are sent to raw to depth
 * @param outPrefix The path prefix for the raw output files when raw streaming is enabled. Set to nullptr to disable raw streaming to files
 * @param outMaxRois The maximum number of ROIs that will be output in a session.
 * @param maxNetFrames The maximum number of output frames that will be sent over the network. Set to zero for unlimited frames
 * @param calFileName The name of the mapping table file. Set to nullptr to use the system control provided mapping table
 * @param pixmapFileName The name of the pixel map file. Set to nullptr to use the system control provided pixel map
 * @param basePort The TCP base port number for the point cloud data. The ports used will be basePort, basePort + 1, ... basePort + 7
 */
MockSensorHeadThread::MockSensorHeadThread(int headNum,
                                           std::string mockPathPrefix,
                                           int mockDelayTimeMs,
                                           const char *outPrefix,
                                           int outMaxRois,
                                           int maxNetFrames,
                                           const char *calFileName,
                                           const char *pixmapFileName,
                                           int basePort) :
    SensorHeadThread::SensorHeadThread(headNum, outPrefix, outMaxRois, calFileName, pixmapFileName, maxNetFrames, basePort),
    m_pathPrefix(mockPathPrefix),
    m_delayTimeUs(mockDelayTimeMs * MICROSECONDS_PER_MILLISECOND),
    m_frame({}) {
}

MockSensorHeadThread::~MockSensorHeadThread() = default;

/**
 * @brief Internal function for computing the size of a file
 *
 * @param name The name of the file as a C-style string
 *
 * @return The size in bytes of the file or -1 if the file cannot be stat'ed
 */
int MockSensorHeadThread::sizeOfFile(const char *name) {
    struct stat statBuf = {};
    if (stat(name, &statBuf) < 0) {
        return -1;
    }
    return (int)statBuf.st_size;
}

/**
 * @brief Internal function to read a file with the specified name and size and send it to raw to depth
 *
 * @param name Name of the file
 * @param size Size of the file
 * @return 0 if the send succeeds or -1 if it fails
 */
int MockSensorHeadThread::sendFrameFromFile(const char *name, uint32_t size) {
    // check the size
    if (size > static_cast<ssize_t>(sizeof(m_frame))) {
        LLogWarning("file_too_big:name=" << name <<
                    ",size=" << size <<
                    ",expected=" << sizeof(m_frame) <<
                    ":file too big for buffer; ignoring excess");
        size = sizeof(m_frame);
    }

    ssize_t remainingBytes = size - METADATA_SIZE;
    if (remainingBytes < 0) {
        LLogWarning("file_too_small:name=" << name <<
                    ",size=" << size <<
                    ":file too small for metadata; skipping");
        errno = EIO;
        return -1;
    }

    // read the file
    int readFd = open(name, O_RDONLY); // NOLINT(hicpp-vararg) calling LINUX vararg API
    if (readFd < 0) {
        LLogWarning("open_mock:name=" << name << ",errno=" << errno << ":skipping");
        return -1;
    }
    auto *bufPtr = (uint8_t *)m_frame.data();
    ssize_t pos = 0;
    while (pos < size) {
        ssize_t bytesRead = ::read(readFd, &bufPtr[pos], m_frame.size() - pos);
        if (bytesRead < 0) {
            LLogWarning("read_mock:name=" << name << ",errno=" << errno << ":can't read mock file; skipping");
            close(readFd);
            return -1;
        }
        pos += bytesRead;
    }
    close(readFd);

    // send the data
    sendMipiFrame((const uint8_t *)m_frame.data(), size, 1);
    return 0;
}

/**
 * @brief Internal function to reads the file based on the ROI number and sends it to raw to depth
 *
 * @param num The ROI number
 * @return The next ROI number or -1 if num == 0 and the file does not exist, which indicates a bad mock file prefix
 */
int MockSensorHeadThread::sendNextFrame(int num) {
    std::array<char, PATH_MAX> nameBuf{};

    int size;
    do {
        std::stringstream name;
        name << m_pathPrefix << std::setfill('0') << std::setw(4) << num << ".bin";
        size = sizeOfFile(name.str().c_str());
        if (size < 0) { // file doesn't exist
            if (num != 0) {
                num = 0; // reset and try to get new data
            } else {
                LLogErr("no_files:prefix=" << m_pathPrefix <<
                        ",name=" << name.str() <<
                        ":no files with specified prefix; shutting down thread");
                return -1; // an error occurred
            }
        } else {
            // file is good; use it and go back to main loop
            sendFrameFromFile(name.str().c_str(), size);
        }
    } while (size < 0);
    return num + 1;
}
 
/**
 * @brief The mock sensor head thread main loop
 */
void MockSensorHeadThread::run() {
    int num = 0;
    bool exitThread = false;

    // on startup, reload the cal data
    reloadCalibrationData();

    while (!exitThread) {
        uint8_t note = receiveNotification();
        if (note == THR_NOTIFY_EXIT_THREAD) {
            exitThread = true;
        } else if (note != THR_NOTIFY_NOTHING_HAPPENED) {
            ackControlByte(note);
        }

        if (!exitThread) {
            if (m_delayTimeUs >= 0) {
                if (m_delayTimeUs > 0) {
                    usleep(m_delayTimeUs);
                }
                num = sendNextFrame(num);
            }
            // If the file name is bad, die
            if (num < 0) {
                LLogInfo("die_thread::thread is dying");
                exitThread = true;
            }
        }
    }

    SensorHeadThread::notifyShutdown();
}
