#pragma once

/**
 * @file MockSensorHeadThread.h
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the interface for the MockSensorHeadThread class,
 *        a subclass of the SensorHeadThread class.
 */

#include "SensorHeadThread.h"

/**
 * @brief MockSensorHeadThread class is responsible for reading mock files
 * from disk and forwarding the data to the MIPI ROI handler in the
 * SensorHeadThread superclass that ultimately sends the ROI data to
 * the raw to depth code.
 */
class MockSensorHeadThread : public SensorHeadThread {

public:
    MockSensorHeadThread(int headNum,
                         std::string mockPath,
                         int mockDelayTimeMs,
                         const char *outPrefix,
                         int outMaxRois,
                         int maxNetFrames,
                         const char *calFileName,
                         const char *pixmapFileName,
                         int basePort);
    MockSensorHeadThread(MockSensorHeadThread& shThread) = delete;
    MockSensorHeadThread(MockSensorHeadThread&& shThread) = delete;
    MockSensorHeadThread& operator=(const MockSensorHeadThread&) = delete;
    MockSensorHeadThread& operator=(MockSensorHeadThread&&) = delete;
    ~MockSensorHeadThread() override;
    void run() override;
    static void selfRun(MockSensorHeadThread *self) { self->run(); }

private:
    static int sizeOfFile(const char *name); // check if file exists and get its size
    int sendFrameFromFile(const char *name, uint32_t size); // send a frame from a file
    int sendNextFrame(int num);
    std::string m_pathPrefix;
    int m_delayTimeUs;
    std::array<uint16_t, METADATA_SIZE + IMAGE_WIDTH * NUM_GPIXEL_PHASES * NUM_GPIXEL_PERMUTATIONS * 2 * MAX_IMAGE_HEIGHT> m_frame;
};

