/**
 * @file processRoi_float.cpp
 * @brief The ROI processing routine for grid (camera) mode.
 * processRoi() is called once for each ROI received by the frontend from 
 * the driver. Each ROI is processed into a full-frame (VGA-width) buffer and passed
 * to processWholeFrame() for final computation.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthV2_float.h"
#include "RawToDepthDsp.h"
#include "Binning.h"
#include "RtdMetadata.h"
#include "LumoUtil.h"
#include "FloatVectorPool.h"
#include <cmath>
#include <LumoLogger.h>
#include <LumoTimers.h>
#include <cassert>
#include <NearestNeighbor.h>
#include <iostream>
#include <fstream>

/**
 * @brief A wrapper function so that we can do threading and input buffer 
 *        management at another time.
 * 
 * @param roi The raw input ROI buffer received from the frontend.
 * @param numBytes The size of the buffer.
 */
void RawToDepthV2_float::processRoi(const uint16_t *roi, uint32_t numBytes)
{
  auto localTimer = LumoTimers::ScopedTimer(*_timers, "RawToDepthV2_float::processRoi()", TIMERS_UPDATE_EVERY);
  processOneRoi(this, roi, numBytes);
}

/**
 * @brief Performs per-roi processing on the input raw roi data for grid (camera) mode.
 * This static method can be executed in a separate thread as long as
 * the integrity of shared resources is properly managed.
 * 
 * Definition: a "raw pixel" is a three-component value read from the sensor of the iTOF camera. That three-component
 * value is used to compute phase, signal, background, snr, etc.
 * 
 * This routine processes the high-rate raw input data and writes it into 
 * full-frame (VGA-width) buffers (_fRawFrames) in preparation for whole-frame processing. 
 * The input ROIs are strips of data acquired horizontally (in the 640-direction of the sensor) 
 * corresponding to the transmitted laser stripe. The individual ROIs are processed, then snr-voted into
 * a full-frame sized buffer. After the output full-frame buffer is filled, processWholeFrame() 
 * is called to finalize the FOV and make the data available to the network consumer 
 * 
 * Binning happens in processWholeFrame() following the generation of the full-frame (VGA-width) buffer,
 * therefore the outputs from processRoi() are the size of the unbinned FOV. They can be as large as 
 * VGA-sized (480 high, 640 wide), but the FOV can be steered in the vertical (max 480) direction,
 * so the number and spacing of the ROIs define the height of the output buffer 
 * 
 * The operations follow the following pattern:
 * 1. Ignore this ROI if the data is inconsistent or invalid.
 * 2. High Dynamic Range processing: An original ROI (roi) is  stored into the internal
 *    buffers for HDR. If the next ROI is marked as "retake," that indicates that 
 *    the original ROI was saturated, therefore the original and retake ROIs are combined into
 *    the variables roi0/roi1, one for each frequency 
 *    Otherwise, the original ROI is passed unchanged into roi0/roi1 with one ROI-time of latency.
 * 
 *    HDR is disabled by setting RtdMetadata::saturationThreshold to 4095. This removes the one-
 *    ROI latency normally required for HDR operation 
 * 3. The metadata from the very first ROI ever acquired is logged.
 * 4. If this ROI represents the first ROI in an FOV (mdat.getFirstRoi() == true), then the
 *    reset() method is called to resize buffers if necessary 
 * 5. The insufficiently named saveTimestamp() method does validation on system state and returns
 *    false if this ROI needs to be skipped 
 * 6. The original raw data received by the sensor is taken three times, rotating the phase by 1/3
 *    on each re-acquisition to compensate for manufacturing inconsistencies from one pixel
 *    to the next. These three permutations need to be added together in a process referred to as 
 *    "tap rotation. 
 * 
 *    The FPGA for M20/M25/M30 can perform tap rotation. So, for some scanning scenarios, the 
 *    RtdMetadata::getDoTapAccumulation() is false, indicating that tap rotation was performed prior
 *    to this software receiving the data. For some other scenarios, this routine is required to perform
 *    the operation 
 * 7. The variables _fRawFrames, _activeRows, and _roiIndexFrames all contain data that is passed to 
 *    processWholeFrame(), which is running in a separate thread. That means that the buffers need to be ping-ponged
 *    so that processOneRoi() can place its outputs into one set of buffers while processWholeFrame() reads from
 *    the other set 
 * 8. The strip of data from this ROI gets written into the raw output buffers (_fRawFrames, one for each frequency). There
 *    can be overlap between neighboring ROI strips from the input sensor. Therefore the SNR is computed for each 
 *    pixel, and the and higher snr from the new pixel and the previously acquired pixel is written to the output 
 *    buffer. This is referred to as "snr-voting."
 * 9. _activeRows contains a full-frame height vector of bool. Each value indicates whether this row has been 
 *    filled with input data. This is necessary because there may be gaps in the output buffer due to some input
 *    input ROIs not overlapping. processWholeFrame() uses _activeRows to perform row-filling for some of 
 *    the missing rows.
 *  10. _roiIndexFrame  contains an index value for each pixel. The index is simply an integer that increments
 *    with each input ROI. This allows the output software to assign a timestamp to each pixel in the image 
 *    independently 
 *          
 * 
 * @param inst The RawToDepthV2_float instance
 * @param roi  The buffer containing the raw roi with metadata as the first row.
 * @param numBytes The size of the input buffer for error checking.
 */
void RawToDepthV2_float::processOneRoi(RawToDepthV2_float *inst, const uint16_t *roi, uint32_t numBytes)
{
  auto md_ = RtdMetadata(roi, numBytes);
  
  if (nullptr == roi || numBytes == 0) 
  {
    return;
  }
  
  if (!inst->validateMetadata(roi, numBytes)) 
  {
    return;
  }

  inst->_hdr.submit(roi, numBytes/sizeof(uint16_t), inst->_fovIdx, !inst->_veryFirstRoiReceived, INPUT_RAW_SHIFT);
  auto mdat = inst->_hdr.getMetadata();  // metadata needs to be time-delayed to match the roiVector.
  auto roiVector_f = inst->_hdr.getRoi();

  if (!inst->_veryFirstRoiReceived && !inst->_hdr.skip() && inst->_fovIdx ==0) 
  {
    mdat.logMetadata();
  }
  inst->_veryFirstRoiReceived = true;

  if (mdat.getFirstRoi(inst->_fovIdx)) 
  {
    // If necessary resizes the buffers containing intermediate data.
    inst->reset(inst->_hdr.getMetadataVector().data(), ROI_NUM_COLUMNS * NUM_GPIXEL_PHASES * uint32_t(sizeof(uint16_t)));
  }
  if (inst->_hdr.skip()) 
  {
    return; // Skip this ROI because we need to wait one ROI-time of latency to see if the next one is a retake.
  }
  if (!inst->saveTimestamp(mdat)) 
  {
    return;
  }
  
  // auto localTimer0 = LumoTimers::ScopedTimer(inst->_timers, "RawToDepthV2_float::processRoi() - 0", TIMERS_UPDATE_EVERY);
  SCOPED_VEC_F(roi0, NUM_GPIXEL_PHASES*mdat.getRoiNumRows()*ROI_NUM_COLUMNS);
  int freqIdx=0;

  RawToDepthDsp::tapRotation(roiVector_f, roi0, freqIdx, {mdat.getRoiNumRows(), ROI_NUM_COLUMNS}, NUM_GPIXEL_PHASES, mdat.getDoTapAccumulation());

  // auto localTimer1 = LumoTimers::ScopedTimer(inst->_timers, "RawToDepthV2_float::processRoi() - 1", TIMERS_UPDATE_EVERY);
  SCOPED_VEC_F(roi1, NUM_GPIXEL_PHASES*mdat.getRoiNumRows()*ROI_NUM_COLUMNS);
  freqIdx=1;
  RawToDepthDsp::tapRotation(roiVector_f, roi1, freqIdx, {mdat.getRoiNumRows(), ROI_NUM_COLUMNS}, NUM_GPIXEL_PHASES, mdat.getDoTapAccumulation());

  bool changed = false;
  MAKE_VECTOR2(inst->_fRawFrames[inst->_rawPingOrPong], float_t, NUM_GPIXEL_PHASES*mdat.getFovNumRows(inst->_fovIdx)*mdat.getFovNumColumns(inst->_fovIdx));
  MAKE_VECTOR(inst->_activeRows[inst->_rawPingOrPong], bool, mdat.getFovNumRows(inst->_fovIdx));
  if (changed || mdat.getFirstRoi(inst->_fovIdx))
  {
    // The dimensions of _fRawFrames is [ping_or_pong][frequency][raw pixels]. These two fills zero-out the buffers for both frequencies of this 
    // ping-pong state. The transition of the inst->_rawPingOrPong variable happens in this thread just prior to the execution of
    // processWholeFrame().
    std::fill(inst->_fRawFrames[inst->_rawPingOrPong][0].begin(), inst->_fRawFrames[inst->_rawPingOrPong][0].end(), 0.0F);
    std::fill(inst->_fRawFrames[inst->_rawPingOrPong][1].begin(), inst->_fRawFrames[inst->_rawPingOrPong][1].end(), 0.0F);
    std::fill(inst->_activeRows[inst->_rawPingOrPong].begin(), inst->_activeRows[inst->_rawPingOrPong].end(), false);
    // roiIndexFrames is pre-initialized to -1 as a flag to indicate uninitialized pixels. Due to the nature of
    // snr-voting and binning, some rows in the prebinned image might be unassigned.
    std::fill(inst->_roiIndexFrames[inst->_rawPingOrPong].begin(), inst->_roiIndexFrames[inst->_rawPingOrPong].end(), -1);
  }

  auto &rawFrames = inst->_fRawFrames[inst->_rawPingOrPong];
  RawToDepthDsp::snrVoteV2(roi0, roi1, rawFrames, inst->_fovSnrV2, (mdat.getRoiStartRow()-mdat.getFovStartRow(inst->_fovIdx))*ROI_NUM_COLUMNS);

  for (auto rowIdx=0; rowIdx<mdat.getRoiNumRows(); rowIdx++)
  {
    inst->_activeRows[inst->_rawPingOrPong][mdat.getRoiStartRow() - mdat.getFovStartRow(inst->_fovIdx) + rowIdx] = true;
    for (auto colIdx=0; colIdx<RtdMetadata::getRoiNumColumns(); colIdx++)
    {
      inst->_roiIndexFrames[inst->_rawPingOrPong][(mdat.getRoiStartRow() + rowIdx)*IMAGE_WIDTH + 
                                                   RtdMetadata::getFovStartColumn(inst->_fovIdx) + colIdx] = inst->_currentRoiIdx;
    }
  }
}


