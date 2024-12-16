/**
 * @file processWholeFrame_float.cpp
 * @brief Grid Mode Processing.
 * Once the ROIs have been gathered into a full-frame (VGA-width) raw buffer,
 * this routine is called to spin off a separate thread localProcessFrame() to
 * finalize the output FOV and pass the data to the getters for transmission over
 * the network.
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 */

#include "RawToDepthV2_float.h"
#include "RawToDepthDsp.h"
#include "Binning.h"
#include "RtdMetadata.h"
#include "LumoUtil.h"
#include "FloatVectorPool.h"
#include "FovSegment.h"
#include "RawToDepthCommon.h"
#include <cmath>
#include <LumoTimers.h>
#include <cassert>
#include <NearestNeighbor.h>
#include <iostream>
#include <fstream>
#include <future>
#include "LumoAffinity.h"

// #define DISABLE_ASYNC_PROCESS_WHOLE_FRAME

/**
 * @brief processWholeFrame is called following the reception of the last ROI in an fov, as
 * indicated by the RtdMetadata::getFrameCompleted() == true.
 *
 * On the first call of processWholeFrame(), a separate thread is started. The separate thread waits
 * for whole-frame data to be available, then performs the operations and passes the result to the consumer
 * via the setFovSegment() function.
 *
 * All of the parameters necessary for performing whole-FOV processing are captured by (copied into)
 * the localProcessFrameInfo struct and passed to the static routine for processing in a separate
 * thread.
 *
 * The setFovSegment function is used as a callback to send the final results back to RawToFovs to present
 * to the consumer.
 *
 * This routine is called sequentially (in the same thread as) processRoi(). Therefore, the management of the
 * ping-pong variable (_rawPingOrPong) is handled here.
 *
 * It is assumed that the separate thread localProcessWholeFrame() method has completed execution of the previous
 * FOV prior to the entry to this routine -- otherwise there will be frame drops on the input. This routine calls wait()
 * on the condition variable to wait for the completion of the previous thread as the first step to
 * guarantee that condition.
 *
 * @param setFovSegment The callback function passed in from the caller to allow the localProcessWholeFrame() method to
 * pass out the FovSegment object once computation is complete.
 */
void RawToDepthV2_float::processWholeFrame(std::function<void(std::shared_ptr<FovSegment>)> setFovSegment)
{
  auto *rawFrame0 = _fRawFrames.at(_rawPingOrPong).data();
  auto *rawFrame1 = &_fRawFrames.at(_rawPingOrPong)[1];
  auto &activeRows = _activeRows[_rawPingOrPong];
  auto &roiIndexFrame = _roiIndexFrames[_rawPingOrPong];
  _rawPingOrPong = (0x01U ^ _rawPingOrPong); // Prep the next first call to processRoi(), which happens immediately following this thread start.

  auto localTimer = LumoTimers::ScopedTimer(*_timers, "RawToDepthV2_float:: construct localProcessFrameInfo data.", TIMERS_UPDATE_EVERY);

#ifndef DEBUG
  // https://stackoverflow.com/questions/18359864/passing-arguments-to-stdasync-by-reference-fails
  //  std::ref because of perfect forwarding.
  {
    std::unique_lock mutexLock(*_wholeFrameRunningMutex);

    _wholeFrameRunningConditionVariable->wait(mutexLock, [this]
                                              { return this->_wholeFrameRunningData->dataProcessed; });
#endif
    _wholeFrameRunningData->quitNow = false;       // quitNow
    _wholeFrameRunningData->dataReady = true;      // dataReady
    _wholeFrameRunningData->dataProcessed = false; // dataProcessed
    _wholeFrameRunningData->conditionVariable = _wholeFrameRunningConditionVariable;
    _wholeFrameRunningData->mutex = _wholeFrameRunningMutex;
    _wholeFrameRunningData->setFovSegment = setFovSegment;
    _wholeFrameRunningData->fovIdx = _fovIdx;
    _wholeFrameRunningData->binning = _binning;
    _wholeFrameRunningData->size = _size;
    _wholeFrameRunningData->rowKernelIdx = _rowKernelIdx;
    _wholeFrameRunningData->columnKernelIdx = _columnKernelIdx;
    _wholeFrameRunningData->fs = _fs;
    _wholeFrameRunningData->fsInt = _fsInt;
    _wholeFrameRunningData->c = _c_mps;
    _wholeFrameRunningData->minMaxFilterSize = _minMaxFilterSize;
    _wholeFrameRunningData->performGhostMedian = _performGhostMedian;
    _wholeFrameRunningData->nearestNeighborFilterLevel = _nearestNeighborFilterLevel;
    _wholeFrameRunningData->headerNum = getHeaderNum();
    _wholeFrameRunningData->timestamp = getTimestamp();
    _wholeFrameRunningData->sensorId = getSensorID();
    _wholeFrameRunningData->userTag = getUserTag();
    _wholeFrameRunningData->lastRoiReceived = lastRoiReceived();
    _wholeFrameRunningData->incompleteFov = _incompleteFov;
    _wholeFrameRunningData->GCF = getGCF();
    _wholeFrameRunningData->maxUnambiguousRange = getMaxUnambiguousRange();
    _wholeFrameRunningData->imageStart = getImageStart();
    _wholeFrameRunningData->imageStep = getImageStep();
    _wholeFrameRunningData->roiIndexFrame = roiIndexFrame;
    _wholeFrameRunningData->timestamps = *getTimestamps(); // take a copy
    _wholeFrameRunningData->timestampsVec = *getTimestampsVec();
    _wholeFrameRunningData->lastTimerReport = getLastTimerReport();
    _wholeFrameRunningData->pixelMask = _pixelMask;
    _wholeFrameRunningData->fovStart = _sensorFovStart;
    _wholeFrameRunningData->fovStep = _sensorFovStep;
    _wholeFrameRunningData->fovSize = _sensorFovSize;
    _wholeFrameRunningData->fovNumRois = _expectedNumRois;
    _wholeFrameRunningData->lastRoiIdx = _currentRoiIdx;
    _wholeFrameRunningData->disableRangeMasking = _disableRangeMasking;
    _wholeFrameRunningData->snrThresh = _snrThresh;
    _wholeFrameRunningData->rangeOffsetTemperature = _temperatureCalibration.getRangeOffsetTemperature();
    _wholeFrameRunningData->disableRtd = _disableRtd;
    _wholeFrameRunningData->activeRows = activeRows;
    _wholeFrameRunningData->timers = _timers;
    _wholeFrameRunningData->rangeLimit = _rangeLimit;
    _wholeFrameRunningData->rawFrame0 = rawFrame0;
    _wholeFrameRunningData->rawFrame1 = rawFrame1;

#ifdef DEBUG
  localProcessFrame(_wholeFrameRunningData);
#else
    if (!_wholeFrameRunning)
    {
      _wholeFrameRunningFuture = std::async(std::launch::async, &processWholeFrameEventLoop, _wholeFrameRunningData);
      _wholeFrameRunning = true;
    }
  }
  _wholeFrameRunningConditionVariable->notify_one();
#endif
}

/**
 * @brief The method that is the thread that processes whole frames as they become available.
 * 
 * @param infoPtr Contains a copy of the data necessary to perform whole-frame processing.
 */
void RawToDepthV2_float::processWholeFrameEventLoop(std::shared_ptr<LocalProcessFrameInfo> infoPtr)
{
  LLogInfo("processWholeFrameEventLoop starts.");
  while (true)
  {
    {
      std::unique_lock mutexLock(*infoPtr->mutex);
      infoPtr->conditionVariable->wait(mutexLock, [infoPtr]
                                       { return infoPtr->dataReady || infoPtr->quitNow; });

      if (infoPtr->quitNow)
      {
        LLogInfo("processWholeFrameEventLoop quitting now.");
        return;
      }

      infoPtr->dataReady = false;
      localProcessFrame(infoPtr);
    }
    infoPtr->dataProcessed = true;
    infoPtr->conditionVariable->notify_one();
  }
}

/**
 * @brief
 *
 * @param infoPtr The captured variables from the RawToDepthV2_float object required for performing full-FOV processing
 *
 * Definition: a "raw pixel" is a three-component value read from the sensor of the iTOF camera. That three-component
 * value is used to compute phase, signal, background, snr, etc.
 *
 * Description:
 * 1. The input data is passed in via rawFrame0/1 inside the info struct.
 *    The input frames are full-VGA width, and the height is units of pixels from the input sensor. The height
 *    of the input raw data varies based on vertical scan angle.
 * 2. The scan can be chosen in such a way as to leave some rows of rawFrame0/1 uninitialized. In this case,
 *    those rows are filled using fillMissingRows() and placed into f0/f1RowFilled. The algorithm for replacing
 *    missing rows is as follows:
 *    a) If there is data on both the row above and below this one, then this row is filled with the average of the
 *       above and below row data.
 *    b) If there is only one neighboring row (above or below), then that row is replicated into this one.
 *    c) If there are no neighboring rows, the the raw data remains as zeros, which are then subject to binning.
 *  3. The two f0/f1RowFilled buffers are then individually binned into f0/f1RawFovBinned.
 *     Binning is simply summing neighborhoods of 1x1, 2x2, or 4x4 raw pixels.
 *  4. calculatePhase() takes both binned raw buffers (f0/f1RawFovBinned) and performed the first stage of
 *     iTOF processing. The outputs from the two calls to calculatePhase() are:
 *     a) Two frequencies of phase: f0/f1PhaseFov
 *     b) The sum of signal from each frequency: fSignals
 *     c) The sum of SNR from each frequency: fSnr
 *     d) The average of background from each frequency: fBackground
 *  5. (Ghost Mitigation: smoothed raw) Raw data smoothing.
 *     The binned raw data (f0/f1RawFovbinned) buffers are independently smoothed using smoothSummedData() and
 *     written to fF0/fF1SummedSmoothed.
 *
 *     The smoothed data is then used to recompute the phase and perform a phase correction by calling calculatePhaseSmooth()
 *     and putting the results into fSmoothedPhases0/1 and fCorrectedPhases0/1.
 *
 *     This phase correction will adjust the phase by 0 or +/-1 (full phase rotations) based on the phase of the
 *     neighborhood computed from the smoothed raw data.
 *
 *     This correction helps to remove ghosting at sharp edges and reduces the frequency of errors at phase transition boundaries.
 *  6. The smoothed and corrected phases for each frequency (fSmoothedPhases0/1 and fCorrectedPhases0/1) are then used to compute
 *     range values for each output pixel in the binned image.
 *  7. (Phase error reduction: min-max). The values in mFrame are an intermediate value computed during the range processing. It represents
 *     the integer number of phase transitions for one of the frequencies. A min-max filter is run against this data and an output
 *     mask (fMinMaxMask) is generated.
 *
 *     The min-max filter examines a rectangular neighborhood around a given M value. If the neighborhood has a difference of more than
 *     1 phase transition from the current value, then this value is marked as invalid.
 * 8. (Ghost Mitigation: median filter) At sharp depth transitions in a point cloud, the averaging and smoothing of near vs far raw data
 *    causes erroneous range values to be generated. These error are mitigated by running a 2D median filter across the data.
 * 9. (Noise Reduction: Nearest Neighbor filter) To reduce the variance of range values in the output point cloud, each range value is
 *    compared with a rectangular neighborhood. If this range value is sufficiently different from the local neighborhood, then the
 *    range value is marked as invalid. This operation is performed by NearestNeighbor::removeOutliers().
 * 10. Output.
 *    A data structure of type FovSegment is created to pass to the downstream network consumer.
 *    Static getter methods (getRange(), getSnr(), getBackground(), getSignal()) are called to convert the local float-point variables into
 *    the 16-bit formats required by the network stream.
 *    Other values needed by the FovSegment are pulled from the localProcessFrameInfo struct that contains copies of the variables from the
 *    RawToDepthV2_float class.
 */
void RawToDepthV2_float::localProcessFrame(std::shared_ptr<LocalProcessFrameInfo> infoPtr)
{
  LocalProcessFrameInfo &info = *infoPtr;

  if (info.disableRtd)
  {
    return;
  }

  if (info.incompleteFov)
  {
    LLogErr("Skipping whole-frame processing. Incomplete FOV received.");
    return;
  }

  if (info.fovNumRois != info.lastRoiIdx + 1 ||
      !info.lastRoiReceived)
  {
    return;
  }

  auto localTimer = LumoTimers::ScopedTimer(*info.timers, "RawToDepthV2_float::processWholeFrame()", TIMERS_UPDATE_EVERY);

  // Note: Sometimes image height % binning != 0, so rawFrame0/1 can be a few rows longer than prebinnedSize
  // Run this thread on the first A72
  LumoAffinity::setAffinity(LumoAffinity::A72_0);

  auto size = info.size[0] * info.size[1];
  std::array<uint32_t, 2> prebinnedSize = {info.size[0] * info.binning[0], info.size[1] * info.binning[1]}; // lose a few rows at the bottom if rawFrame0.size() % binning != 0

  SCOPED_VEC_F(f0RawFovBinned, NUM_GPIXEL_PHASES * size);
  SCOPED_VEC_F(f1RawFovBinned, NUM_GPIXEL_PHASES * size);

  {
    // auto fillAndBinTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() -- fill and bin", TIMERS_UPDATE_EVERY);
    SCOPED_VEC_F(f0RawFilled, info.rawFrame0->size());
    SCOPED_VEC_F(f1RawFilled, info.rawFrame1->size());

    RawToDepthDsp::fillMissingRows(*info.rawFrame0, f0RawFilled, prebinnedSize, info.activeRows);
    RawToDepthDsp::fillMissingRows(*info.rawFrame1, f1RawFilled, prebinnedSize, info.activeRows);

    Binning::binMxN(f0RawFilled, f0RawFovBinned, prebinnedSize, info.binning);
    Binning::binMxN(f1RawFilled, f1RawFovBinned, prebinnedSize, info.binning);
  }

  SCOPED_VEC_F(f0PhaseFov, size);
  SCOPED_VEC_F(f1PhaseFov, size);
  SCOPED_VEC_F(fSignals, size);
  SCOPED_VEC_F(fSnr, size);
  SCOPED_VEC_F(fBackground, size);
  {
    // auto calcPhaseTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() -- calc phase", TIMERS_UPDATE_EVERY);
    // prefill signals, snr, background with zeros. calculatePhase now sums into the buffers.
    // _fSignals, _fSnr, _fBackground are only accessed in this method.
    std::fill(fSignals.begin(), fSignals.end(), 0.0F);
    std::fill(fSnr.begin(), fSnr.end(), 0.0F);
    std::fill(fBackground.begin(), fBackground.end(), 0.0F);

    RawToDepthDsp::calculatePhase(f0RawFovBinned, f0PhaseFov, fSignals, fSnr, fBackground, float_t(info.binning[0] * info.binning[1]));
    RawToDepthDsp::calculatePhase(f1RawFovBinned, f1PhaseFov, fSignals, fSnr, fBackground, float_t(info.binning[0] * info.binning[1]));
  }

  SCOPED_VEC_F(fF0SummedSmoothed, f0RawFovBinned.size()); // Allocates a std::vector<float_t>
  SCOPED_VEC_F(fF1SummedSmoothed, f1RawFovBinned.size());
  {
    // auto smoothingTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() -- smooth", TIMERS_UPDATE_EVERY);
    RawToDepthDsp::smoothSummedData(f0RawFovBinned, fF0SummedSmoothed, info.size, info.rowKernelIdx, info.columnKernelIdx);
    RawToDepthDsp::smoothSummedData(f1RawFovBinned, fF1SummedSmoothed, info.size, info.rowKernelIdx, info.columnKernelIdx);
  }

  SCOPED_VEC_F(fSmoothedPhases0, size);
  SCOPED_VEC_F(fSmoothedPhases1, size);
  SCOPED_VEC_F(fCorrectedPhases0, size);
  SCOPED_VEC_F(fCorrectedPhases1, size);
  SCOPED_VEC_F(mFrame, size);
  SCOPED_VEC_F(ranges, size);
  SCOPED_VEC_F(fRanges, size);
  SCOPED_VEC_F(fMinMaxMask, size);

  {
    // auto phaseSmoothRangeTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() -- calc phase smooth, range", TIMERS_UPDATE_EVERY);
    RawToDepthDsp::calculatePhaseSmooth(fF0SummedSmoothed, fSmoothedPhases0, f0PhaseFov, fCorrectedPhases0, 0);
    RawToDepthDsp::calculatePhaseSmooth(fF1SummedSmoothed, fSmoothedPhases1, f1PhaseFov, fCorrectedPhases1, 1);
    RawToDepthDsp::computeWholeFrameRange(fSmoothedPhases0, fSmoothedPhases1, fCorrectedPhases0, fCorrectedPhases1, ranges, info.fs, info.fsInt, info.c, mFrame);
  }

  {
    // auto minmaxTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() -- minmax", TIMERS_UPDATE_EVERY);
    RawToDepthDsp::minMaxRecursive(mFrame, fMinMaxMask, info.minMaxFilterSize, info.size, 1);
  }

  {
    // auto medianTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() - median filter", TIMERS_UPDATE_EVERY);
    RawToDepthDsp::medianFilterPlus(ranges, fRanges, {info.rowKernelIdx, info.columnKernelIdx}, info.size, info.performGhostMedian);
  }

  {
    // auto nnTimer = LumoTimers::ScopedTimer(info.timers, "RawToDepthV2_float::processWholeFrame() - nearest neighbor", TIMERS_UPDATE_EVERY);
    NearestNeighbor::removeOutliers(fRanges, info.nearestNeighborFilterLevel, info.size);
  }

  const auto maxUnambiguousRange = (float_t)info.maxUnambiguousRange;
  const auto rangeOffsetTemperature = info.rangeOffsetTemperature;
  std::for_each(fRanges.begin(), fRanges.end(),
                [maxUnambiguousRange, rangeOffsetTemperature](float_t &range)
                {
                  range -= rangeOffsetTemperature;
                  if (range < 0.0F)
                  {
                    range = 0.0F;
                  }
                  range = fmod(range, maxUnambiguousRange);
                });

  auto rangeFov = RawToDepthCommon::getRange(fRanges,
                                             fMinMaxMask, info.pixelMask, fSnr,
                                             info.fovStart, info.fovStep, info.fovSize[1], info.size,
                                             info.disableRangeMasking, info.snrThresh,
                                             info.rangeOffsetTemperature,
                                             info.rangeLimit,
                                             (float_t)info.maxUnambiguousRange);

  auto roiIndicesFov = getRoiIndices(info.roiIndexFrame, info.fovStart, info.fovStep, info.fovSize, info.size);

  const std::array<uint32_t, 2> fovStart = {info.fovStart[0] / info.binning[0], info.fovStart[1] / info.binning[1]};
  const std::array<uint32_t, 2> fovStep = {info.binning[0], info.binning[1]};

  info.setFovSegment(
      std::make_shared<FovSegment>(info.fovIdx,
                                   info.headerNum,
                                   info.timestamp, // timestamp
                                   info.sensorId,
                                   info.userTag,
                                   info.lastRoiReceived,
                                   info.GCF,
                                   info.maxUnambiguousRange,
                                   info.size,
                                   rangeFov,
                                   info.imageStart,
                                   info.imageStep,
                                   fovStart,
                                   fovStep,
                                   RawToDepthCommon::getSnr(fSnr),
                                   RawToDepthCommon::getSignal(fSignals),
                                   RawToDepthCommon::getBackground(fBackground),
                                   roiIndicesFov,
                                   std::make_shared<std::vector<uint64_t>>(info.timestamps),
                                   std::make_shared<std::vector<std::vector<uint32_t>>>(info.timestampsVec),
                                   info.lastTimerReport));
}
