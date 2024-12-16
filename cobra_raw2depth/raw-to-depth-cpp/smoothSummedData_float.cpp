/**
 * @file smoothSummedData_float.cpp
 * @brief The top-level entry point for the various smoothing routines.
 * Implements (slow) general-purpose smoothing using 32-bit floating point
 * operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthDsp.h"
#include "LumoUtil.h"
#include "FloatVectorPool.h"
#include "LumoTimers.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>

#define SmoothRaw5x7HKernelSize 5
#define SmoothRaw5x7VKernelSize 7
#define SmoothRaw7x15HKernelSize 7
#define SmoothRaw7x15VKernelSize 15

void RawToDepthDsp::smoothRaw(const std::vector<float_t> &roiSummed, std::vector<float_t> &roiSmoothed, std::array<uint32_t,2> size,
                              uint32_t _rowKernelIdx, uint32_t _columnKernelIdx)
{
  const auto numRows = size[0];
  const auto numCols = size[1];
  const auto numPixels = numRows * numCols;
  assert(roiSmoothed.size() == roiSummed.size());
  std::fill(roiSmoothed.begin(), roiSmoothed.end(), 0.0F);

  const auto &rowKernel_s = _fKernels[_rowKernelIdx];
  const auto &columnKernel_s = _fKernels[_columnKernelIdx];

  const auto rowKernelSize = int32_t(rowKernel_s.size());
  const auto columnKernelSize = int32_t(columnKernel_s.size());


  SCOPED_VEC_F(roiSmoothedTemp_s, roiSummed.size());
  std::fill(roiSmoothedTemp_s.begin(), roiSmoothedTemp_s.end(), 0.0F);

  const int32_t rowKernelEvenComp = fmod(rowKernelSize, 2) == 0 ? -1 : 0;
  const auto rowKernelHalfSize = fmod(rowKernelSize, 2) == 0 ? int32_t(rowKernelSize / 2) + 1 : int32_t((rowKernelSize - 1) / 2);

  const int32_t columnKernelEvenComp = fmod(columnKernelSize, 2) == 0 ? -1 : 0;
  const auto columnKernelHalfSize = fmod(columnKernelSize, 2) == 0 ? int32_t(columnKernelSize / 2) : int32_t((columnKernelSize - 1) / 2);

  // Convolve each column and stock result (axis = 0)
  uint32_t currentrowIdx = 0U;
  for (auto pixel_index = 0U; pixel_index < (int)numPixels; pixel_index++)
  {
    float_t prod0 = 0;
    float_t prod1 = 0;
    float_t prod2 = 0;
    if (currentrowIdx < columnKernelHalfSize || currentrowIdx >= numRows - columnKernelHalfSize)
    {
      prod0 = roiSummed[3 * pixel_index + 0];
      prod1 = roiSummed[3 * pixel_index + 1];
      prod2 = roiSummed[3 * pixel_index + 2];
    }
    else
    {
      for (auto kernel_index = -1 * columnKernelHalfSize; kernel_index <= columnKernelHalfSize + columnKernelEvenComp; kernel_index++)
      {
          prod0 += columnKernel_s[kernel_index + columnKernelHalfSize] * roiSummed[3 * pixel_index + 0 + 3 * kernel_index * numCols];     // a
          prod1 += columnKernel_s[kernel_index + columnKernelHalfSize] * roiSummed[3 * pixel_index + 1 + 3 * kernel_index * numCols]; // b
          prod2 += columnKernel_s[kernel_index + columnKernelHalfSize] * roiSummed[3 * pixel_index + 2 + 3 * kernel_index * numCols]; // c

      }
    }
    assert(3U * pixel_index + 2 < roiSmoothedTemp_s.size());
    roiSmoothedTemp_s[3 * pixel_index + 0] = prod0; 
    roiSmoothedTemp_s[3 * pixel_index + 1] = prod1;
    roiSmoothedTemp_s[3 * pixel_index + 2] = prod2;

    if (uint32_t(fmod(pixel_index, numCols)) >= numCols - 1)
    {
      currentrowIdx++;
    }
  }

  // Convolve each row and stock result (axis = 1)
  for (auto pixel_index = 0U; pixel_index < numPixels; pixel_index++)
  {
    auto realPixelIndexInRow = int32_t(fmod(pixel_index, numCols));
    float_t prod0_s = 0.0F;
    float_t prod1_s = 0.0F;
    float_t prod2_s = 0.0F;

    if (realPixelIndexInRow < rowKernelHalfSize || realPixelIndexInRow >= numCols - rowKernelHalfSize)
    {
      prod0_s = roiSmoothedTemp_s[3U * pixel_index + 0];
      prod1_s = roiSmoothedTemp_s[3U * pixel_index + 1];
      prod2_s = roiSmoothedTemp_s[3U * pixel_index + 2];
    }
    else
    {
      for (int kernel_index = -1 * rowKernelHalfSize; kernel_index <= rowKernelHalfSize + rowKernelEvenComp; kernel_index++)
      {
          prod0_s += rowKernel_s[kernel_index + rowKernelHalfSize] * roiSmoothedTemp_s[3 * pixel_index + 0 + 3 * kernel_index];     // a
          prod1_s += rowKernel_s[kernel_index + rowKernelHalfSize] * roiSmoothedTemp_s[3 * pixel_index + 1 + 3 * kernel_index]; // b
          prod2_s += rowKernel_s[kernel_index + rowKernelHalfSize] * roiSmoothedTemp_s[3 * pixel_index + 2 + 3 * kernel_index]; // c
      }
    }

    assert(3 * pixel_index + 2 < roiSmoothed.size());
    roiSmoothed[3 * pixel_index + 0] = prod0_s; 
    roiSmoothed[3 * pixel_index + 1] = prod1_s;
    roiSmoothed[3 * pixel_index + 2] = prod2_s;
  }

}

void RawToDepthDsp::smoothSummedData(const std::vector<float_t> &roiSummed, std::vector<float_t> &roiSmoothed, std::array<uint32_t,2> size,
                                     uint32_t _rowKernelIdx, uint32_t _columnKernelIdx, bool doAcceleratedVersion)
{
  const auto numRows = size[0];
  const auto numCols = size[1];
  const auto numPixels = numRows * numCols;
  assert(roiSmoothed.size() == roiSummed.size());
  std::fill(roiSmoothed.begin(), roiSmoothed.end(), 0.0F);

  const auto &rowKernel_s = _fKernels[_rowKernelIdx];
  const auto &columnKernel_s = _fKernels[_columnKernelIdx];

  const auto rowKernelSize = int32_t(rowKernel_s.size());
  const auto columnKernelSize = int32_t(columnKernel_s.size());

  // Shortcut the convolution if the filters are disabled, or the image is too small to filter
  if ( (_rowKernelIdx == 0 && _columnKernelIdx == 0) ||
       (rowKernelSize > size[1] || columnKernelSize > size[0]))
  {
    std::copy(roiSummed.begin(), roiSummed.end(), roiSmoothed.begin());
    return;
  }

  if (rowKernelSize == SmoothRaw5x7HKernelSize && 
      columnKernelSize == SmoothRaw5x7VKernelSize && 
      doAcceleratedVersion)
  {
    smoothRaw5x7(roiSummed, roiSmoothed, size);
    return;
  }

  if (rowKernelSize == SmoothRaw7x15HKernelSize && 
      columnKernelSize == SmoothRaw7x15VKernelSize && 
      doAcceleratedVersion)
  {
    smoothRaw7x15(roiSummed, roiSmoothed, size);
    return;
  }

  smoothRaw(roiSummed, roiSmoothed, size, _rowKernelIdx, _columnKernelIdx);
 }
