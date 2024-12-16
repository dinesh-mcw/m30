/**
 * @file medianFilterPlus_float.cpp
 * @brief An implementation of an mxn median filter using a plus-shaped pattern
 * using 32-bit floating point operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthDsp.h"
#include <algorithm>

inline float_t RawToDepthDsp::median(std::vector<float_t> &points) {
  assert(points.size() & 1U);
  std::sort(points.begin(), points.end());
  return points[points.size() / 2];
}


// No. Large outliers dominate.
#include <limits>
inline float_t RawToDepthDsp::binMedian(std::vector<float_t> &points)
{
  static float_t lowest_float = std::numeric_limits<float_t>::lowest();
  static float_t max_float = std::numeric_limits<float_t>::max();
  float_t max = lowest_float;
  float_t min = max_float;

  for (auto point : points)
  {
    if (max < point) 
    {
      max = point;
    }
    if (min > point) 
    {
      min = point;
    }
  }

  if (max-min == 0) 
  {
    return points[0]; // all points have same value.
  }

  constexpr uint32_t histogramSize=5;
  std::array<int, histogramSize> hist {0,0,0,0,0};
  for (auto point : points)
  {
    const int idx = int(roundf(4.0F * (point - min)/(max-min)));
    assert(idx <= 5);
    assert(idx >= 0);
    hist[idx]++;
  }

  int maxCount = 0;
  int maxIdx = 0;
  for (auto idx=0; idx<histogramSize; idx++)
  {
    auto val = hist[idx];
    if (val > maxCount) 
    {
      maxCount = val;
      maxIdx = idx;
    }
  }

  return min + float_t(maxIdx) * (max-min);
}

void RawToDepthDsp::medianFilterPlus(std::vector<float_t> &inFrame, std::vector<float_t> &outFrame,
                                     std::vector<uint32_t> kernelIndices,
                                     std::array<uint32_t,2> frameSize, bool performGhostMedian) {
  assert(frameSize[0] * frameSize[1] == (uint32_t)outFrame.size());

  std::copy(inFrame.begin(), inFrame.end(), outFrame.begin());
  if (!performGhostMedian)
  {
    return;
  }

  int hFilterSize = int(_fKernels[kernelIndices[0]].size() | 1U); // guarantee filter size is odd.
  int vFilterSize = int(_fKernels[kernelIndices[1]].size() | 1U);

  auto pointOffsets = getMedianOffsets(frameSize, kernelIndices);

  int rowStart = vFilterSize / 2;
  int colStart = hFilterSize / 2;
  auto rowPitch = (int)frameSize[1];

  SCOPED_VEC_F(points, vFilterSize + hFilterSize - 1);

  // "int" guarantees that the output is signed for the following check.
  int numRows = (int)frameSize[0] - (int)vFilterSize + 1;
  int numColumns = (int)frameSize[1] - (int)hFilterSize + 1;

  if (numRows <= 0 || numColumns <= 0)
  {
    return; // image unmodified.
  }

  auto idxStart = rowStart * rowPitch + colStart; // center of the x-shaped median kernel

  for (auto rowIdx = 0; rowIdx < numRows; rowIdx++) {
    auto idx = idxStart;
    for (auto colIdx = 0; colIdx < numColumns; colIdx++) {

      for (auto pointIdx = 0; pointIdx < pointOffsets.size(); pointIdx++) {
        assert(pointIdx >= 0);
        assert(pointIdx < points.size());
        assert(idx + pointOffsets[pointIdx] >= 0);
        assert(idx + pointOffsets[pointIdx] < inFrame.size());
        points[pointIdx] = inFrame[idx + pointOffsets[pointIdx]];
      }

      assert(idx < outFrame.size());
      assert(idx >= 0);
      auto val = median(points);
      outFrame[idx] = val; // points is sorted after this call.
      idx++;
    }
    idxStart += rowPitch;
  }

}

