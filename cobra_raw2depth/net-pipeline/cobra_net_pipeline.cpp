/**
 * @file cobra_net_pipeline.cpp
 * @brief This file contains the implementations of the CobraNetPipelineWrapper
 *        and the CobraRawNetPipelineWrapper classes. These classes
 *        provide the external API for the network pipelines.
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved
 */
#include "cobra_net_pipeline.hpp"
#include "network_streamer.hpp"
#include "LumoLogger.h"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <vector>
#include <algorithm> 

using namespace LidarPipeline;

// Network Parameters
#define NUM_LOOK_ANGLES 240L
#define NUM_VIRT_CPI_PER_LOOK_ANGLE 10
#define NUM_VIRT_CHANNELS_PER_CPI 64
#define NUM_TOTAL_CHANNELS_PER_LOOK_ANGLE (NUM_VIRT_CHANNELS_PER_CPI * NUM_VIRT_CPI_PER_LOOK_ANGLE)

#define NUM_FRAME_BUFFERS 5
#define NUM_CPI_BUFFERS (NUM_LOOK_ANGLES * NUM_VIRT_CPI_PER_LOOK_ANGLE * NUM_FRAME_BUFFERS)

#define FAKED_FRAME_TIME_US 100000 

#define NET_OUTPUT_ADDRESS              "255.255.255.255"
#define NET_OUTPUT_BASE_PORT            12566
#define NET_OUTPUT_RAWDATA_BASE_PORT    12345

#define NET_OUTPUT_MAX_PORTS 32

// Shot in the dark. Is going to be scan pattern and application specific. So for now, a frame has
// (946 bytes / UDP Payloads) * (10 UDP Payloads / Look Angle) * (480 Angles / Frame). Want a maximum
// of 5 frames outstanding on the network.
#define NET_FRAME_SIZE          (946L * 10L * NUM_LOOK_ANGLES)
#define NET_SEND_BUFFER         (NET_FRAME_SIZE * 20L)
#define NET_BUFFER_SOFT_LIMIT   (NET_FRAME_SIZE * 1L)
#define NET_BUFFER_HARD_LIMIT   (NET_FRAME_SIZE * 2L)

#define NET_RAWDATA_FRAME_SIZE          ROI_SIZE
#define NET_RAWDATA_SEND_BUFFER         (NET_RAWDATA_FRAME_SIZE * 4L)
#define NET_RAWDATA_BUFFER_SOFT_LIMIT   (NET_RAWDATA_FRAME_SIZE * 1L)
#define NET_RAWDATA_BUFFER_HARD_LIMIT   (NET_RAWDATA_FRAME_SIZE * 2L)

constexpr unsigned int PTP_TIMESTAMP_COARSE_SIZE    { 6 };
constexpr unsigned int PTP_TIMESTAMP_FINE_SIZE      { 4 };

CobraNetPipelineWrapper::CobraNetPipelineWrapper(int sensorHeadNum, int maxNetFrames, int basePort)
{

  m_mm = new PipelineDataMM(NUM_FRAME_BUFFERS, NUM_CPI_BUFFERS, this->outputType_);

  if((sensorHeadNum >= NET_OUTPUT_MAX_PORTS) || (sensorHeadNum < 0)) {
      LLogErr("CobraNetPipelineWrapper maximum TCP port count exceeded");
      exit(1);
  }

  if (basePort <= 0) {
    basePort = NET_OUTPUT_BASE_PORT;
  }

  m_ns = new TCPWrappedStreamer(1, 1, basePort + sensorHeadNum, NET_SEND_BUFFER, this->outputType_);

  m_iteration = 0;
  m_submittedFrames = 0;
  m_skippedFrames = 0;

  m_latchMetaUpdateNeeded = false;

  m_ns->set_dbg_MaxFrames(maxNetFrames);

  if(!m_ns->SetCircularBufferSize(NUM_FRAME_BUFFERS+1))
  {
    LLogErr("CobraNetPipelineWrapper set circular buffer size failed");
    exit(1);
  }

  m_ns->SetMemMgr(m_mm);
  m_ns->StartModule();
}

constexpr uint64_t SHIFT32                  { 32 };
constexpr uint64_t PTP_COARSE_MASK          { 0x0000FFFFFFFFFFFF };
constexpr uint64_t PTP_FINE_MASK            { 0x3FFFFFFF };
constexpr uint64_t ARB_TIME_FILTER          { 0x40000000 }; // coarse timestamps before this time (Y2004) are considered ARB and not UTC
constexpr uint32_t FPGA_TIMESTAMP_UNITS     { 10U };        // FPGA fine timestamps are in 10ns units

// If using chunky timestamps instead of frame-based timestamps,
// pick/generate a "nominal" by TBD policy, then overwrite standard/frame-based
// timestamp with that for this packet/chunk. If no points were "valid" in this
// chunk don't overwrite the old style frame timestamp.
static inline void fillInTimestamp(CPIReturn *cpir,
                                   std::vector<uint16_t> &seenRoiIndices,
                                   std::vector<std::vector<uint32_t>> &fullTimestampByIdxVector)
{
    std::vector<uint32_t>& medianTimestamp = fullTimestampByIdxVector.at(0);
    if (!seenRoiIndices.empty())
    {
        size_t count = seenRoiIndices.size();
                
        // Data/scene dependent selections
        std::sort(seenRoiIndices.begin(), seenRoiIndices.end());
        uint16_t medianRoiIdx = seenRoiIndices[count / 2];

        medianTimestamp = fullTimestampByIdxVector.at(medianRoiIdx);
    }

    uint64_t coarse = ((uint64_t)medianTimestamp[2] << SHIFT32) + medianTimestamp[1];
    uint64_t ptp_sec = htobe64((coarse) & PTP_COARSE_MASK);
    uint32_t ptp_nsec = htobe32((medianTimestamp[0] & PTP_FINE_MASK) * FPGA_TIMESTAMP_UNITS);

    memcpy(cpir->timestamp.data(), ((uint8_t *)&ptp_sec) + 2, PTP_TIMESTAMP_COARSE_SIZE);
    memcpy(&(cpir->timestamp.at(PTP_TIMESTAMP_COARSE_SIZE)), ((uint8_t *)&ptp_nsec), PTP_TIMESTAMP_FINE_SIZE);
    cpir->tscale = coarse < ARB_TIME_FILTER ? TimestampScale::ARB : TimestampScale::UTC;
}

// Reformat Cobra frames into Barracuda/Thunderbird-style CPIs
void CobraNetPipelineWrapper::HandInCobraDepth(std::shared_ptr<FovSegment> processedFov)
{
    if (!processedFov)
    {
        return;
    }

    if(processedFov->isNewMappingTableAvailable())
    {
        m_latchMetaUpdateNeeded = true;
    }

    m_submittedFrames++;

    // Will only update if stream is not active
    m_ns->setDeviceID(processedFov->getSensorId());

    auto ranges = processedFov->getRange();
    auto signals = processedFov->getSignal();
    auto snr = processedFov->getSnrSquared();
    auto background = processedFov->getBackground();
    auto roiIdx = processedFov->getRoiIndexFov();
    
    // Makes using operator[] easier
    auto *rangeVector = ranges.get();
    auto *signalVector = signals.get();
    auto *snrVector = snr.get();
    auto *bgVector = background.get();
    auto *roiIdxVector = roiIdx.get();

    // Indexed by ROI Index, *not* image coordinates (one level of indirection not present in other
    // data)
    auto fullTimestampByIdx = processedFov->getTimestampsVec();
    auto *fullTimestampByIdxVector = fullTimestampByIdx.get();

    // TODO: Accept some subset of these vectors -- even more urgent for SWDL
    // MAYBE: If timestamps aren't available, just override policy/configuration (not yet present)
    //        for chunky timestamps
    if (rangeVector == nullptr ||
        signalVector == nullptr ||
        snrVector == nullptr ||
        bgVector == nullptr || 
        roiIdxVector  == nullptr || 
        fullTimestampByIdxVector == nullptr)
    { // valid data not available.
        return;
    }

    // Image Size
    std::vector<uint32_t> sizes = processedFov->getImageSize();
    uint32_t sizeSteerDim = sizes[0];
    uint32_t sizeStareDim = sizes[1];

    // Image Steps
    std::vector<uint32_t> steps = processedFov->getMappingTableStep();
    uint32_t stepSteerDim = steps[0];
    uint32_t stepStareDim = steps[1];

    // Image Top Left
    std::vector<uint32_t> topLeft = processedFov->getMappingTableTopLeft();
    auto tlSteerDimRaw = static_cast<int32_t>(topLeft[0]);
    auto tlStareDimRaw = static_cast<int32_t>(topLeft[1]);
//  tlSteerDimRaw -= stepSteerDim / 2;
//  tlStareDimRaw -= stepStareDim / 2;
//  assert(tlSteerDimRaw >= 0);
//  assert(tlStareDimRaw == 0);
    uint32_t tlSteerDim = tlSteerDimRaw; 
    uint32_t tlStareDim = tlStareDimRaw;


    // Binning
    //size_t bc = processedFov->getBinning();
    //(void) bc; // Nothing for now -- put in metadata someday?

    // Network level dimensions
    size_t steerAngles = sizeSteerDim;
    size_t stareSteps = (sizeStareDim + (NUM_CHANNELS_PER_TYPE2_PACKET - 1))/ NUM_CHANNELS_PER_TYPE2_PACKET;

    // If we're out of returnchunk pool we'll generate a warning for now
    ReturnChunk *returnChunk = m_mm->GetReturnChunk();
    if(returnChunk == nullptr)
    {
        LLogWarning("NetWrapper/TCP: No Return Chunk available. Skipping ROI.");
        return;
    }

    // Allocate and Fill-In Full Scene of Data
    // Note from X10/X20, we implicitly have 64 channels everywhere, and
    // we generate 10 "virtual CPIs" per look angle
    for (unsigned int i = 0; i < steerAngles; i++)
    {
        for (unsigned int j = 0; j < stareSteps; j++)
        {
            uint32_t pktNum = i * stareSteps + j;

            // Allocate
            CPIReturn *cpir = m_mm->GetCPIReturn();

            returnChunk->cpiReturns.at(pktNum) = cpir;
            returnChunk->cpiReturnsUsed++; // TODO: Move me into the index above, kill pktNum

            // Looks like we've passed all the potential drop points -- transfer the
            // latched metadata update signal as a one-shot
            if(m_latchMetaUpdateNeeded)
            {
                cpir->prefixMetaDataUpdate = true;
                m_latchMetaUpdateNeeded = false;
            }

            // Update calibration table pointers regardless of if we're sending a
            // metadata update immediately. May need them later, and should do it
            // coherently from the same thread that may be reading
            cpir->calibrationX = processedFov->getCalibrationX();
            cpir->calibrationY = processedFov->getCalibrationY();
            cpir->calibrationTheta = processedFov->getCalibrationTheta();
            cpir->calibrationPhi = processedFov->getCalibrationPhi();

            // Fill in Header
            cpir->completeSizeSteerDim = sizeSteerDim;
            cpir->completeSizeStareDim = sizeStareDim;

            cpir->startingSteerOrder = i;
            cpir->startingStareOrder = NUM_CHANNELS_PER_TYPE2_PACKET * j;

            // Hack in SWDL variable vertical crop and bin for now
            cpir->bs_SteerOffset = (tlSteerDim + i * stepSteerDim);
            cpir->bs_SteerStep = stepSteerDim;
            cpir->bs_StareOffset = (tlStareDim + cpir->startingStareOrder * stepStareDim);
            cpir->bs_StareStep = stepStareDim;

            // And hack in user tag, too
            cpir->bs_UserTag = processedFov->getUserTag();

            // Are we the last packet of the frame?
            cpir->metaLastCpiInFrame = (i == steerAngles - 1) && (j == stareSteps - 1);

            // Copy out
            std::vector<uint16_t> seenRoiIndices;
            seenRoiIndices.reserve(NUM_CHANNELS_PER_TYPE2_PACKET);

            for (uint32_t channel = 0; channel < NUM_CHANNELS_PER_TYPE2_PACKET; channel++)
            {
                if((channel + cpir->startingStareOrder) >= cpir->completeSizeStareDim)
                {
                    break;
                }

                size_t inIndex = cpir->startingSteerOrder * sizeStareDim + cpir->startingStareOrder + channel;

                if((*rangeVector)[inIndex] != 0)
                {
                    cpir->rangeValid.at(channel) = true;
                    cpir->range.at(channel) = (*rangeVector)[inIndex];

                    // Note that we only select a "chunky" timestamp from/with candidates whose points were deemed 
                    // present/valid
                    seenRoiIndices.push_back(roiIdxVector->at(inIndex));
                }
                cpir->intensityValid.at(channel) = true;
                cpir->intensity.at(channel) = (*signalVector)[inIndex];
                cpir->backgroundValid.at(channel) = true;
                cpir->background.at(channel) = (*bgVector)[inIndex];
                cpir->snrValid.at(channel) = true;
                cpir->snr.at(channel) = (*snrVector)[inIndex];
            }

            fillInTimestamp(cpir, seenRoiIndices, *fullTimestampByIdxVector);
        }
    }

    m_ns->HandChunkIn(returnChunk);
}

// Raw Data
CobraRawDataNetPipelineWrapper::CobraRawDataNetPipelineWrapper(int sensorHeadNum, int maxNetFrames, unsigned int numROIsInBuffer) {

    m_mm = new PipelineDataMM(numROIsInBuffer, 0, this->outputType_);
    m_ns = new TCPWrappedStreamer(1, 1, NET_OUTPUT_RAWDATA_BASE_PORT + sensorHeadNum, NET_RAWDATA_SEND_BUFFER, this->outputType_);

    m_ns->set_dbg_MaxFrames(-1); // always disable max frame count. We could add at some point max ROI count but for now
                                 // we can simply specify the desired number of ROIs on the receiving end

    if(!m_ns->SetCircularBufferSize((size_t) numROIsInBuffer+1)) {
        LLogErr("CobraNetPipelineWrapper set circular buffer size failed");
        exit(1);
    }

    m_ns->SetMemMgr(m_mm);
    m_ns->StartModule();
}

bool CobraRawDataNetPipelineWrapper::HandInCobraROI(const char *roi, int roiSize, bool firstRawRoi)
{

    static bool fovTransmitInProgress = false;
    static uint16_t numROIs = 0;

    if (roiSize != ROI_SIZE) {
        LLogErr("RawData/NetWrapper: roi passed on has the wrong size: " << roiSize <<
                " (actual) vs " << ROI_SIZE << " (expected)");
        return false;
    }

    if (!m_ns->IsPipelineRunning()) {
        // if pipeline isn't running yet on the other end, don't fill it up
        return false;
    }

    if (firstRawRoi) {
        fovTransmitInProgress = false;
        numROIs = 0;
    }

    // Read the ROI start/stop flags from the metadata
    // We will be focusing on transmitting the raw data for FoV 0. All other FoVs are ignored.
    // We will drop entire frames of FoV 0 to make sure that we aren't slowing down other processes.
    RtdMetadata metadata((uint16_t *) roi, roiSize);

    if (!metadata.getIsFovActive(0))
    { // if this ROI does not contain data for FOV 0, skip it
        return false;
    }
    if (metadata.getFovNumRois(0) > m_mm->GetReturnChunkPoolCapacity()) // Need to make sure that the memory pool size is large enough for 1 frame
    {
        LLogErr("RawData/NetWrapper: Attempting to output raw ROIs but the memory pool size ("
                << m_mm->GetReturnChunkPoolCapacity() << ") is smaller than the number of ROIs ("
                << metadata.getFovNumRois(0) << ")");
        return false;
    }

    if (numROIs >= metadata.getFovNumRois(0)) // if we are full already
    {
        if (m_mm->GetNumAvailableReturnChunk() == m_mm->GetReturnChunkPoolCapacity()) // If we've sent out everything, we are ready to transmit again
        {
            numROIs = 0;
            fovTransmitInProgress = false;
        } else { // Wait until buffer is emptied
            return false;
        }
    }
    
    // When a "start ROI" gets in, we will accept up to numROIs ROIs from FoV 0 (which should be the entire frame).
    // Then we will block all others ROIs coming in until the transmission is done (controlled by fovTransmitInProgress)
    // Once fovTransmitInProgress is false again and a "start ROI" gets in, we can restart sending a frame
    if (metadata.getFirstRoi(0)) // if current roi is a "start ROI"
    {
        if (!fovTransmitInProgress) // Not currently transmitting, kick off new frame transmit
        {
            fovTransmitInProgress = true;
        } else { // Already transmitting, return and wait for buffer to be emptied
            return false;
        }
    }

    if (!fovTransmitInProgress) // If the current is not a start ROI and a frame is not already in progress, return
    {
        return false;
    }
        

    // At this point, there are only two possibilities. The ROI will be pushed in because:
    // 1) it is the starting ROI of a new frame and we are ready to transmit
    // or
    // 2) it belongs to the frame currently being pushed out
    numROIs++;

    // Error check: if the current ROI is the last one of the current frame, we can check that we received all of them,
    //              otherwise a problem occurred
    if (metadata.getFrameCompleted(0))
    {
        assert(numROIs == metadata.getFovNumRois(0));
    }

    ReturnChunk *returnChunk = m_mm->GetReturnChunk();
    if(returnChunk != nullptr)
    { // If we're out of returnchunk pool something went terribly wrong, error out (this shouldn't be attainable)
        LLogErr("RawData/NetWrapper: No Return Chunk available. Skipping an ROI is forbidden.");
        return false;
    }

    ROIReturn *roir = m_mm->GetROIReturn();
    returnChunk->roiReturn = roir;

    // Copy input data into the buffer 
    memcpy(roir->roi.data(), roi, (size_t) roiSize);
    
    // Hand to network streamer (other thread)
    m_ns->HandChunkIn(returnChunk);

    return true; // successfully handed in the ROI
}
