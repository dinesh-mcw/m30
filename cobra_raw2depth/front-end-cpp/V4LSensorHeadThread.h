#pragma once

/**
 * @file V4LSensorHeadThread.h
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the interface for the V4LSensorHeadThread class.
 */
#include "SensorHeadThread.h"
#include "TimeSync.h"
#include "frontend.h"

#define NUM_V4L_BUFFERS 32

/**
 * @brief V4LSensorHeadThread class is a subclass of SensorHeadThread.
 * This class is responsible for setting up and tearing down Video for Linux
 * streaming, receiving raw frames from Video for Linux, and forwarding the
 * frames to the MIPI ROI handler in the SensorHeadThread superclass, which
 * ultimately sends the frames to Raw to Depth. This class also detects and
 * logs frame drop incidents.
 */
class V4LSensorHeadThread : public SensorHeadThread {

public:
    V4LSensorHeadThread(int headNum,
                        std::string devicePath,
                        const char *outPrefix,
                        int outMaxRois,
                        int maxNetFrames,
                        const char *calFileName,
                        const char *pixmapFileName,
                        int basePort,
                        std::shared_ptr<TimeSync> timesync,
                        unsigned int i2cAddress);
    V4LSensorHeadThread(V4LSensorHeadThread& shThread) = delete;
    V4LSensorHeadThread(V4LSensorHeadThread&& shThread) = delete;
    V4LSensorHeadThread& operator=(const V4LSensorHeadThread&) = delete;
    V4LSensorHeadThread& operator=(V4LSensorHeadThread&&) = delete;
    ~V4LSensorHeadThread() override;
    void run() override;
    void syncTimeOnNextSession() override;
    static int uninterruptedIoctl(int deviceFd, int request, void *arg);

private:
    int openDevice();
    void closeDevice();
    int getMetadata(); // returns the mode
    int startSession(uint8_t mode, uint8_t note);
    void endSession();
    void retrieveAndSendMipiFrame();
    void reportDroppedRois();
    void lookForDroppedRoisAndAdjustTime(uint8_t *mipiFrameData);
    int handleNotification(uint8_t note);
    static void adjustMetadataTimestamp(uint8_t *ptr, uint64_t offset);
    typedef struct {
        void *buffer;
        size_t length;
    } membuf_t;
    std::array<membuf_t, NUM_V4L_BUFFERS> m_buffers;
    std::string m_devicePath;
    int m_videoFd;
    bool m_streaming;
    int m_seqNum;
    uint32_t m_frameCount;
    uint32_t m_droppedFrames;
    uint32_t m_dropEvents;
    uint32_t m_roiSize;
    uint32_t m_numRois;
    std::shared_ptr<TimeSync> m_timeSync;
    unsigned int m_i2cAddress;
    std::atomic_bool m_syncTimeOnNextSession;
    uint64_t m_timeOffset;
    std::array<int, FOV_STREAMS_PER_HEAD> m_lastUserTags;
};
