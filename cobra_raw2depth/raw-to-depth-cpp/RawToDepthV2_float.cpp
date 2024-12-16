/**
 * @file RawToDepthV2_float.cpp
 * @brief Specialization of the RawToDepth class that implements the float-point
 *        RawToDepth algorithm set.
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

RawToDepthV2_float::RawToDepthV2_float(uint32_t fovIdx, uint32_t headerNum) :
  RawToDepth(fovIdx, headerNum) , 
  _wholeFrameRunning(false),
  _wholeFrameRunningMutex(std::make_shared<std::mutex>()),
  _wholeFrameRunningConditionVariable(std::make_shared<std::condition_variable>()),
  _wholeFrameRunningData(std::make_shared<LocalProcessFrameInfo>())
{
  realloc((uint16_t*)RtdMetadata::DEFAULT_METADATA.data(), uint32_t(RtdMetadata::DEFAULT_METADATA.size()*sizeof(uint16_t)));
  _wholeFrameRunningData->dataProcessed = true; // Initial condition for the threading.

  std::ostringstream logId; logId << std::setw(4) << std::setfill('0') << "RawToDepthV2_float_" << _headerNum;
  LumoLogger::setId(logId.str());
}

RawToDepthV2_float::~RawToDepthV2_float()
{
  _wholeFrameRunningData->quitNow = true;
  LLogDebug("RawToDepthV2_float dtor");
}

void RawToDepthV2_float::shutdown()
{
  if (!_wholeFrameRunningFuture.valid())
  {
    return;
  }
  
  {
    std::unique_lock mutexLock(*_wholeFrameRunningMutex);
    _wholeFrameRunningData->dataReady = true;
    _wholeFrameRunningData->quitNow = true;
  }
  _wholeFrameRunningConditionVariable->notify_one();
  _wholeFrameRunningFuture.wait();
}

void RawToDepthV2_float::reset(const uint16_t *mdPtr, uint32_t mdBytes) {
  RawToDepth::reset(mdPtr, mdBytes);
  auto mdat = RtdMetadata(mdPtr, mdBytes);

  _performGhostMedian = mdat.getPerformGhostMedianFilter(_fovIdx);
  _performGhostMinMax = mdat.getPerformGhostMinMaxFilter(_fovIdx);

  realloc(mdPtr, mdBytes);
}

bool RawToDepthV2_float::bufferSizesChanged(RtdMetadata &mdat) {
  if (RawToDepth::bufferSizesChanged(mdat)) 
  {
    return true;
  }

  if (_activeRows[0].size() != mdat.getFovNumRows(_fovIdx) ||
      _activeRows[1].size() != mdat.getFovNumRows(_fovIdx) ||
      _roiIndexFrames[0].size() != size_t(MAX_IMAGE_HEIGHT) * size_t(IMAGE_WIDTH) ||
      _roiIndexFrames[1].size() != size_t(MAX_IMAGE_HEIGHT) * size_t(IMAGE_WIDTH) ||
      _fovSnrV2.size() != size_t(RtdMetadata::getFovNumColumns(_fovIdx)) * size_t(mdat.getFovNumRows(_fovIdx))
  )
  {
    return true;
  }
  return false;
}

void RawToDepthV2_float::realloc(const uint16_t *mdPtr, uint32_t mdBytes)
{
  auto imsize = _size[0] * _size[1];
  auto mdat = RtdMetadata(mdPtr, mdBytes);

  bool changed = false;

  MAKE_VECTOR2(_fRawFrames[0], float_t, NUM_GPIXEL_PHASES*mdat.getFovNumColumns(_fovIdx)*mdat.getFovNumRows(_fovIdx)); 
  MAKE_VECTOR2(_fRawFrames[1], float_t, NUM_GPIXEL_PHASES*mdat.getFovNumColumns(_fovIdx)*mdat.getFovNumRows(_fovIdx)); 
  MAKE_VECTOR(_activeRows[0], bool, mdat.getFovNumRows(_fovIdx));
  MAKE_VECTOR(_activeRows[1], bool, mdat.getFovNumRows(_fovIdx));
  MAKE_VECTOR(_roiIndexFrames[0], int32_t, MAX_IMAGE_HEIGHT * IMAGE_WIDTH);
  MAKE_VECTOR(_roiIndexFrames[1], int32_t, MAX_IMAGE_HEIGHT * IMAGE_WIDTH);
  
  // unbinned snr the size of the fov.
  MAKE_VECTOR(_fovSnrV2, float_t, mdat.getFovNumColumns(_fovIdx) * mdat.getFovNumRows(_fovIdx)); // prebinned.
  std::fill(_fovSnrV2.begin(), _fovSnrV2.end(), 0.0F);
  
  assert(mdat.getBinningY(_fovIdx) == mdat.getBinningX(_fovIdx));
  if (mdat.getBinningX(_fovIdx) == 2)
  {
    _columnKernelIdx = 3;
    _rowKernelIdx = 2; 
  }


  if (mdat.getBinningX(_fovIdx) == 1)
  {
    _columnKernelIdx = 3;
    _rowKernelIdx = 2;
  }

      
  if (mdat.getBinningX(_fovIdx) == 4)
  {
    _columnKernelIdx = 2; 
    _rowKernelIdx = 1; 
  }
   
  if (mdat.getDisablePhaseSmoothing(_fovIdx)) 
  {
    _rowKernelIdx=0;
    _columnKernelIdx=0;
  }

  _minMaxFilterSize = {};
  if (mdat.getPerformGhostMinMaxFilter(_fovIdx)) 
  {
    uint32_t vMinMaxSize = (RawToDepthDsp::_fKernels[_rowKernelIdx].size()/2) & 0x01U;
    uint32_t hMinMaxSize = (RawToDepthDsp::_fKernels[_columnKernelIdx].size()/2) & 0x01U;
    const uint32_t minSize = 3;
    if (vMinMaxSize < minSize) 
    {
      vMinMaxSize = minSize;
    }
    if (hMinMaxSize < minSize) 
    {
      hMinMaxSize = minSize;
    }
    _minMaxFilterSize = {vMinMaxSize, hMinMaxSize};
  }

  if (changed || bufferSizesChanged(mdat)) 
  {
    FloatVectorPool::clear();
  }

}

static bool contains(const std::vector<int32_t> &roiStartRows, const uint32_t row)
{
  return std::find(roiStartRows.begin(), roiStartRows.end(), row) != roiStartRows.end();
}


bool RawToDepthV2_float::saveTimestamp(RtdMetadata &mdat)
{
  if (!RawToDepth::saveTimestamp(mdat))
  {
    return false;
  }

  RETURN_CONDITION(mdat.getScanTableTag() != _expectedScanTableTag, "Skipping ROI. Scan table tag changed in the middle of an FOV."s);
  RETURN_CONDITION(mdat.getRandomFovTag(_fovIdx) != _expectedFovTag, "Skipping ROI. FOV tag changed in the middle of an FOV."s);

  RETURN_CONDITION(
    (mdat.getFovNumRois(_fovIdx) != _expectedNumRois) ||  
    (_sensorFovStart != std::array<uint16_t,2>({mdat.getFovStartRow(_fovIdx), RtdMetadata::getFovStartColumn(_fovIdx)})) ||  
    (_sensorFovSize !=  std::array<uint16_t,2>({mdat.getFovNumRows(_fovIdx), RtdMetadata::getFovNumColumns(_fovIdx)})) || 
    (_sensorFovStep !=  std::array<uint16_t,2>({mdat.getBinningY(_fovIdx), mdat.getBinningX(_fovIdx)})),
    "Skipping ROI. FOV size changed relative to the first ROI."s);

  RETURN_CONDITION(bufferSizesChanged(mdat), "Buffer sizes changed between first_rois. Dropping roi."s);

  // check for an ROI specified outside the FOV
  RETURN_CONDITION((int)mdat.getRoiStartRow()-(int)mdat.getFovStartRow(_fovIdx) + (int)mdat.getRoiNumRows()  > (int)mdat.getFovNumRows(_fovIdx),
            "An ROI was specified that lies outside of the FOV. roiStartRow " << mdat.getRoiStartRow() << 
            ". roiNumRows " << mdat.getRoiNumRows() << ". fovStartRow " << mdat.getFovStartRow(_fovIdx) << 
            ". fovNumRows " << mdat.getFovNumRows(_fovIdx) << ". This is an error in the metadata.");

  RETURN_CONDITION(_fovStartRow != mdat.getFovStartRow(_fovIdx), "FOV start row changed without first_roi. Dropping ROI."s);

  RETURN_CONDITION(!contains(_roiStartRows, mdat.getRoiStartRow()) && _roiStartRows.size() >= mdat.getFovNumRows(_fovIdx), 
    "An unexpected ROI start row was found in the metadata. An ROI has likely been missed. Dropping ROI"s);

  if (!contains(_roiStartRows, mdat.getRoiStartRow()))
  {
    _roiStartRows.push_back(mdat.getRoiStartRow());
  }
  
  return true;
}
