/**
 * @file hdr_float.cpp
 * @brief Implements the High Dynamic Range operations for iTOF raw
 * data using 32-bit floating point operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "hdr.h"
#include "LumoTimers.h"


std::vector<float> hdr::readoutRoi() { return _rois[_nextRoiIdx]; }

bool hdr::reallocBuffers(uint32_t roiShorts)
{
  bool changed = false;
  MAKE_VECTOR2(_rois, float_t, roiShorts);
  return changed;
}

void hdr::copyBuffer(const uint16_t *src, std::vector<float> &dst, uint32_t numElements, uint32_t shiftr, uint16_t mask)
{
  RawToDepthDsp::sh2f(src, dst, numElements, shiftr, mask);
}


void hdr::hdrSum(uint16_t saturationLevel, const uint16_t *roi, uint32_t roiShorts, uint32_t shiftr, uint16_t rawMask)
{
  // HDR Processing.
  
  for (auto idx = 0; idx < roiShorts; idx += 3)
  {
    auto a_prev = _rois[_previousRoiIdx][idx + 0];
    auto b_prev = _rois[_previousRoiIdx][idx + 1];
    auto c_prev = _rois[_previousRoiIdx][idx + 2];

    auto prev_max = a_prev;
    if (b_prev > prev_max)
    {
      prev_max = b_prev;
    }
    if (c_prev > prev_max)
    {
      prev_max = c_prev;
    }

    // If a pixel is saturated, replace it with the re-acquired one.
    if (prev_max >= float_t(saturationLevel>>shiftr)) // input 16-bit data has been right-shifted by one bit.
    {
      _rois[_nextRoiIdx][idx + 0] = float_t(uint32_t(roi[idx + 0] & rawMask) >> shiftr);
      _rois[_nextRoiIdx][idx + 1] = float_t(uint32_t(roi[idx + 1] & rawMask) >> shiftr);
      _rois[_nextRoiIdx][idx + 2] = float_t(uint32_t(roi[idx + 2] & rawMask) >> shiftr);
    }
    else
    {
      _rois[_nextRoiIdx][idx + 0] = a_prev;
      _rois[_nextRoiIdx][idx + 1] = b_prev;
      _rois[_nextRoiIdx][idx + 2] = c_prev;
    }
  }

}
