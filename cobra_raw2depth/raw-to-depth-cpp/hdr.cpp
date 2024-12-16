/**
 * @file hdr.cpp
 * @brief High Dynamic Range algorithms
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "hdr.h"
#include "RtdVec.h"
#include <LumoUtil.h>
#include <cassert>

void hdr::realloc(uint32_t roiShorts) {
  bool changed = reallocBuffers(roiShorts);
  MAKE_VECTOR2(_md, uint16_t, ROI_NUM_COLUMNS*NUM_GPIXEL_PHASES);
  if (!changed) 
  {
    return;
  }

  _previousRoiWasCorrected = false;
  _skipThis = true;
}


void hdr::submit(const uint16_t *roiWithHeader, uint32_t roiShortsWithHeader, uint32_t fovIdx, bool startup, uint32_t shiftr) {
  // roi is the raw input with the header attached.
  // skipThis notifies the caller that the ROI they are receiving is ready to be used for a point cloud.

  auto rowShorts = ROI_NUM_COLUMNS * NUM_GPIXEL_PHASES;
  auto roiShorts = roiShortsWithHeader - rowShorts;

  realloc(roiShorts);

  assert(_md[0].size() == rowShorts);
  assert(_md[1].size() == rowShorts);

  RtdMetadata mdat(roiWithHeader, rowShorts*sizeof(uint16_t));
  auto mask = RtdMetadata::getRawPixelMask();

  const auto *roi = roiWithHeader + rowShorts;

  assert(roiShorts == _rois[0].size());
  assert(roiShorts == _rois[1].size());
  assert(roiShorts%3 == 0);
  _skipThis = false;


  // Note: if md.isHdrDisabled() on one ROI, then the next one is marked as "previousRoiSaturated(),"
  // Then unknown data will come out.
  // straight pass through, no pipeline delay. Put this ROI into _rois[_nextRoiIdx], ready to be read out.
  if (mdat.isHdrDisabled()) {
    _skipThis = false;
    _previousRoiWasCorrected = false;

    copyBuffer(roi, _rois[_nextRoiIdx], roiShorts, shiftr, mask);
    hdr_copy(roiWithHeader, rowShorts, _md[_nextRoiIdx]);
    return;
  }


  // First ROI ever, or,
  // First ROI following a re-acquired one, so store this one into the previous buffer and add an roi of latency.
  if (startup ||
      _previousRoiWasCorrected) 
  {
    assert(!mdat.wasPreviousRoiSaturated());
    _skipThis = true;
    _previousRoiWasCorrected = false;
    copyBuffer(roi, _rois[_previousRoiIdx], roiShorts, shiftr, mask);
    hdr_copy(roiWithHeader, rowShorts, _md[_previousRoiIdx]);
    
    copyBuffer(roi, _rois[_nextRoiIdx], roiShorts, shiftr, mask);
    hdr_copy(roiWithHeader, rowShorts, _md[_nextRoiIdx]);

    return;
  }

  // By the time we reach this line, we know that there is one good ROI in _rois[_previousRoiIdx].

  // The current ROI is new (not re-acquired). Store it into the history buffer.
  // Send along the previous ROI since it has not been re-acquired.
  if (!mdat.wasPreviousRoiSaturated()) {
    _skipThis = false;
    _previousRoiWasCorrected = false;

    // ping-pong buffer definitions so that what's in "previous" now become "next"
    _previousRoiIdx = _previousRoiIdx == 0 ? 1 : 0;
    _nextRoiIdx = _nextRoiIdx == 0 ? 1 : 0;
    
    // Now that previous ROI is ready to be read from _rois[_nextRoiIdx],
    // Put this current ROI into the previous buffer to add an roi of delay.
    copyBuffer(roi, _rois[_previousRoiIdx], roiShorts, shiftr, mask);
    hdr_copy(roiWithHeader, rowShorts, _md[_previousRoiIdx]);

    return;
  }

  hdrSum(mdat.getSaturationThreshold(), roi, roiShorts, shiftr, mask);
  // If two ROIs have been merged together, pass out the metadata from the prior acquisition.
  hdr_copy(_md[_previousRoiIdx].data(), rowShorts, _md[_nextRoiIdx]);

  _skipThis = false;
  _previousRoiWasCorrected = true;

  // By the time we reach this code, there is no valid data in _rois[_previousRoiIdx] (the history buffer).
  // And _rois[_nextRoiIdx] contains the roi combined between the last two acquisitions.
}
