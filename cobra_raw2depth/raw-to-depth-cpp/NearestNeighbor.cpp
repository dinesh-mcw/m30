/**
 * @file NearestNeighbor.cpp
 * @brief Implements the nearest-neighbor filter on range values using
 * 32-bit floating-point operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "NearestNeighbor.h"
#include "RawToDepthDsp.h"
#include "FloatVectorPool.h"
#include "RtdMetadata.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cmath>

// This count includes the center pixel. So a "3" really means that the filter requires 2 neighbors in the surrounding region.
// 0: disabled
// 1: ignore this value, use a 3-point horizontal median filter.
// 2: ignore this value, use a 5 point median filter with a cross-shaped kernel.
const std::vector<uint16_t> NearestNeighbor::_lutNeighborCountTolerance { 0,      3,      5,      5,      7,     11}; 
const std::vector<uint16_t> NearestNeighbor::_lutWindowSize             { 0,      3,      5,      6,      7,      9};
const std::vector<float_t>  NearestNeighbor::_flutRangeToleranceFrac    { 0,  1.0F/16.0F, 1.0F/16.0F, 1.0F/16.0F, 1.0F/16.0F, 1.0F/16.0F}; //Fixed-point multiplier fraction. Q0.16. 0x0fff is 1/16 (>>4 bits).

inline float_t countNeighbors(float_t val, float_t rangeTol, uint32_t minNeighborCount, std::vector<float_t> &ranges, uint32_t startIdx, uint32_t winSize, uint32_t stride) {
  uint32_t winStart = startIdx;
  uint32_t numNeighbors = 0;

  for (uint32_t rowIdx=0; rowIdx<winSize; rowIdx++) {
    uint32_t winEnd = winStart + winSize;
    
    for (uint32_t winIdx=winStart; winIdx<winEnd; winIdx++) {
      assert(winIdx < ranges.size());
      auto winVal = ranges[winIdx];
      // No branches.
      numNeighbors += uint32_t(rangeTol >= fabs(winVal - val));
    }
    winStart += stride;
  }

  float_t outputValue = (numNeighbors < minNeighborCount) ? 0.0F : val; // still a branch, once per pixel.

  return outputValue;
}

void NearestNeighbor::removeOutliers(std::vector<float_t> &ffilteredRanges, uint16_t filterLevel, std::array<uint32_t,2> &size) 
{

  if (size[0] < _lutNeighborCountTolerance.back() || size[1] < _lutNeighborCountTolerance.back())
  {
    return;
  }

  if (filterLevel == 0) {
    return;
  }

  if (filterLevel > MAX_NEAREST_NEIGHBOR_IDX) 
  {
    filterLevel = MAX_NEAREST_NEIGHBOR_IDX;
  }
  assert(_lutWindowSize.size() > MAX_NEAREST_NEIGHBOR_IDX);
  assert(_flutRangeToleranceFrac.size() > MAX_NEAREST_NEIGHBOR_IDX);
  assert(_lutNeighborCountTolerance.size() > MAX_NEAREST_NEIGHBOR_IDX);

  SCOPED_VEC_F(franges, ffilteredRanges.size());
  std::copy(ffilteredRanges.begin(), ffilteredRanges.end(), franges.begin());
  
  assert(std::size_t(size[0]*size[1]) <= franges.size());
  
  auto winSize = _lutWindowSize[filterLevel];
  auto halfWin = winSize / 2;
  auto rangeTolFrac = _flutRangeToleranceFrac[filterLevel];
  auto minNeighborCount = _lutNeighborCountTolerance[filterLevel];

  
  auto imh = size[0] - 2*halfWin;
  auto imw = size[1] - 2*halfWin;
  
  uint32_t stride = size[1];
  uint32_t colStart = halfWin + stride*halfWin;
  uint32_t winStart = 0;
  
  for (auto yIdx=0; yIdx<imh; yIdx++) {
    auto pixIdx = colStart; // Index to the first pixel in the current row of the input buffer.
    auto winIdx = winStart; // Index to topleft pixel in the window
    
    for (auto xIdx=0; xIdx<imw; xIdx++) {
      assert(pixIdx < franges.size());
      auto val = franges[pixIdx];
      const float_t rangeTol = 1.0F/1024.0F + val * rangeTolFrac;
      ffilteredRanges[pixIdx++] = countNeighbors(val, rangeTol, minNeighborCount, franges, winIdx++, winSize, stride);
    }
    
    // Move to the next row.
    winStart += stride;
    colStart += stride;
  }

}
