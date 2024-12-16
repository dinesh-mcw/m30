#ifndef COBRA_NET_PIPELINE_HPP
#define COBRA_NET_PIPELINE_HPP

/**
 * @file cobra_net_pipeline.hpp
 * @brief This file contains the definitions for the CobraNetPipelineWrapper,
 *        the class that manages the network pipelines that sends point cloud
 *        data over TCP. There is one network pipeline for each field of view
 *        (FoV) supported (currently 8).
 *
 *        This file also contains definitions for the parallel
 *        CobraRawDataNetPipelineWrapper, the class that manages the pipeline for
 *        raw data. 
 *
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved
 */

#include "pipeline_modules.hpp"
#include "network_streamer.hpp"
#include <RawToDepth.h>
#include <FovSegment.h>
#include <atomic>

namespace LidarPipeline {

/**
 * @brief CobraNetPipelineWrapper creates and manages the pipelines that send
 *        point cloud data to the network. It also provides the external API
 *        for the network pipelines. The externally facing functions are:
 *        1. The CobraNetPipelineWrapper constructor, which creates the
 *           pipeline. It is called from the SensorHeadThread constructor
 *        2. The CobraNetPipelineWrapper::HandInCobraDepth() method, which
 *           sends raw to depth data to the pipeline. It is called from the
 *           SensorHeadThread::sendRoi() method
 */
class CobraNetPipelineWrapper
{
    public:
        CobraNetPipelineWrapper(int sensorHeadNum, int maxNetFrames, int basePort);
        void HandInCobraDepth(std::shared_ptr<FovSegment> processedFov);
    protected:
        PipelineDataMM *m_mm;
        NetworkStreamer *m_ns;
        uint64_t m_iteration;
        uint64_t m_submittedFrames;
        uint64_t m_skippedFrames;
        bool m_latchMetaUpdateNeeded;
    private:
        const PipelineOutputType outputType_ = PipelineOutputType::ProcessedData;
};

/**
 * @brief CobraNetPipelineWrapper creates and manages the pipelines that send
 *        point cloud data to the network. It also provides the external API
 *        for the network pipelines. The externally facing functions are:
 *        1. The CobraRawDataNetPipelineWrapper constructor, which creates the
 *           raw ROI network pipeline. It is called from the
 *           SensorHeadThread::receiveNotification() method. Note that
 *           raw ROI streaming is not supported in this release.
 *        2. The CobraRawDataNetPipelineWrapper::HandInCobraROI() method,
 *           which sends raw ROI data to the raw ROI network pipeline. It is
 *           called from the SensorHeadThread::sendRoi() method.
 */
class CobraRawDataNetPipelineWrapper
{
    public:
        CobraRawDataNetPipelineWrapper(int sensorHeadNum, int maxNetFrames, unsigned int numROIsInBuffer);
        bool HandInCobraROI(const char *roi, int roiSize, bool firstRawRoi);
    protected:
        PipelineDataMM *m_mm;
        NetworkStreamer *m_ns;
    private:
        const PipelineOutputType outputType_ = PipelineOutputType::RawData;
};

}

#endif
