#pragma once

/**
 * @file SensorHeadThread.h
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the interface for the SensorHeadThread class.
 */

#include <cstdint>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <array>
#include <RawToFovs.h>
#include <cobra_net_pipeline.hpp>

constexpr unsigned int METADATA_SIZE                { static_cast<unsigned long>(IMAGE_WIDTH) * NUM_GPIXEL_PHASES * sizeof(uint16_t) };     ///< Size in bytes of metadata
constexpr unsigned int FOV_STREAMS_PER_HEAD         { 8 };      ///< Number of network threads
constexpr unsigned int THR_CONTROL_ACK_TIMEOUT_MS   { 10000 };  ///< Timeout in milliseconds to wait for sensor head thread to execute and acknowledge a command

/**
 * @brief SensorHeadThread class is the base class for the V4LSensorHeadThread
 *        and MockSensorHeadThread classes.
 *        The SensorHeadThread class supports the following functionality:
 *        1. Communicating with the main thread
 *        2. Sending MIPI frame and region of interest (ROI) data to the raw to
 *           depth code
 *        The derived classes are responsible for retrieving video data.
 */

class SensorHeadThread {

public:
    SensorHeadThread(int headNum, const char *outPrefix, int outMaxRois, const char *calFileName, const char *pixmapFileName, int maxNetFrames, int basePort);
    SensorHeadThread(SensorHeadThread& shThread) = delete;
    SensorHeadThread(SensorHeadThread&& shThread) = delete;
    SensorHeadThread& operator=(const SensorHeadThread&) = delete;
    SensorHeadThread& operator=(SensorHeadThread&&) = delete;
    virtual ~SensorHeadThread();
    void handleControlByte(uint8_t controlByte);    // waits for reply
    void ackControlByte(uint8_t controlByte) const;
    void exitThread();
    virtual void run() {}
    int getTrigFd() const;
    void notifyShutdown() const;
    static void selfRun(SensorHeadThread *self) { self->run(); }
    virtual void syncTimeOnNextSession() {}
    void markThreadAsStopped() { m_stopped = true; }
    bool threadStopped() const { return m_stopped; }

protected:
    void sendMipiFrame(const uint8_t *data, uint32_t dataSizePerRoi, uint32_t numRoisInFrame); // functionality depends on mode
    uint8_t receiveNotification();
    int getWaitFd() const;
    void reloadCalibrationData();
    int m_headNum;

private:
    void notifyThread(uint8_t controlByte) const;        // does not wait for reply
    void sendRoi(const uint8_t *dataU8, unsigned int dataSizePerRoi);
    int m_waitFd;
    int m_trigFd;
    std::shared_ptr<RawToFovs> m_rawToFov;
    std::array<LidarPipeline::CobraNetPipelineWrapper*, FOV_STREAMS_PER_HEAD> m_netWrappers; // net wrappers for processed data (1 per FoV)
    LidarPipeline::CobraRawDataNetPipelineWrapper* m_rawDataNetWrapper; // net wrapper for raw data (1 per sensor head)
    LumoTimers m_sendFrame_Timers;
    const char *m_outPrefix;
    int m_outMaxRois;
    int m_maxNetFrames;
    int m_outSessionNum;
    int m_outRoiNum;
    bool m_outStreaming;
    const char *m_calFileName;
    const char *m_pixmapFileName;
    std::atomic_bool m_calLoaded;
    bool m_rawStreamingSuspended;
    bool m_firstRawRoi;
    bool m_stopped;
};

