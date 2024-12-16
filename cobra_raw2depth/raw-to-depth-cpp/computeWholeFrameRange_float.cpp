
/**
 * @file computeWholeFrameRange_float.cpp
 * @brief Computes the range and the min-max mask given the smoothed and
 * corrected iTOF phase data using 32-bit floating point operations in the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthDsp.h"
#include <limits>
inline bool RawToDepthDsp::outOfRange(const std::vector<float_t> &frame, uint32_t idx, const std::vector<int32_t> &offsets, float_t thresh) {

  // Find the min and max values in the window.
  float_t minVal=std::numeric_limits<float_t>::max();
  float_t maxVal=std::numeric_limits<float_t>::lowest();
  for (auto offset : offsets) {
    assert(idx + offset < frame.size());
    assert(int(idx) + int(offset) >= 0);
    auto val = frame[idx + offset];
    if (val < minVal) 
    {
      minVal = val;
    }
    if (val > maxVal) 
    {
      maxVal = val;
    }
  }
  

  // Return true of the total range of values in the sample exceeds the threshold.
  return (maxVal-minVal > thresh) ;

}

void RawToDepthDsp::minMax(const RtdVec &frame, RtdVec &minMaxMask, std::vector<uint32_t> filterSize, std::vector<uint32_t> frameSize, float_t minMaxThresh) {

  std::fill(minMaxMask.begin(), minMaxMask.end(), 0.0F);
  if (2 != filterSize.size()) {
    return;
  }

  int vFilterSize = int(filterSize[0]|1U); // guarantee filter size is odd.
  int hFilterSize = int(filterSize[1]|1U);

  int rowStart = vFilterSize/2;
  int colStart = hFilterSize/2;
  int rowPitch = (int)frameSize[1];

  // lookup table for offsets to filters within the window.
  auto offsets = std::vector<int32_t>(std::size_t(vFilterSize*hFilterSize));
  auto offsetIdx = 0;
  for (auto rowIdx=0; rowIdx<vFilterSize; rowIdx++) 
  {
    for (auto colIdx=0; colIdx<hFilterSize; colIdx++) 
    {
      assert(offsetIdx >= 0);
      assert(offsetIdx < offsets.size());
      offsets[offsetIdx] = rowPitch*(rowIdx-rowStart) + (colIdx-colStart);
      offsetIdx++;
    }
  }

  auto numRows    = frameSize[0] - vFilterSize + 1;
  auto numColumns = frameSize[1] - hFilterSize + 1;

  uint32_t idxStart = rowStart*rowPitch + colStart;
  for (auto rowIdx=0; rowIdx<numRows; rowIdx++) 
  {
    auto idx = idxStart;
    for (auto colIdx=0; colIdx<numColumns; colIdx++) 
    {
      if (outOfRange(frame, idx, offsets, minMaxThresh)) 
      {
        minMaxMask[idx] = 1.0F;
      }
      idx++;
    }
    idxStart += rowPitch;
  }
  
}


void RawToDepthDsp::computeWholeFrameRange(std::vector<float_t> &fSmoothedPhases0,
					   std::vector<float_t> &fSmoothedPhases1,
					   std::vector<float_t> &fCorrectedPhases0,
					   std::vector<float_t> &fCorrectedPhases1,
					   std::vector<float_t> &fRanges,
					   std::array<float_t, 2> freqs, std::vector<float_t> fsInt,
					   float_t cMps,
					   std::vector<float_t> &mFrame
					   ) { // _c is the speed of light.

  auto &phase0Frame = fCorrectedPhases0;
  auto &phase1Frame = fCorrectedPhases1;

  auto freq0 = float(freqs[0]);
  auto freq1 = float(freqs[1]);
  auto iFInt0 = float_t(fsInt[0]);
  auto iFInt1 = float_t(fsInt[1]);

  const float a_float = 0.5F * cMps / (2.0F * freq1); // f is 100e6, _c is 300e6
  const float c_float = 0.5F * cMps / (2.0F * freq0); // c a local algorithmic constant
  
  assert(phase0Frame.size() == phase1Frame.size());
  for (uint32_t idx = 0; idx < phase0Frame.size(); idx++)
  {
    auto phaseSmoothed0 = fSmoothedPhases0[idx];
    auto phaseSmoothed1 = fSmoothedPhases1[idx];

    float_t maskNegatives = phaseSmoothed1 < phaseSmoothed0 ? 1.0F : 0.0F;
    float_t mRaw1 = iFInt0 * phaseSmoothed1;
    float_t mRaw2 = iFInt1 * phaseSmoothed0;
    float_t mRaw3 = iFInt0 * maskNegatives;
    float_t mRaw_tmp = mRaw1 - mRaw2 + mRaw3;
    float_t mRaw_float = roundf(mRaw_tmp); 
    auto mRaw = int32_t(mRaw_float);

    mFrame[idx] = mRaw_float + mRaw_float + maskNegatives;

    float_t phase0 = phase0Frame[idx]; 
    float_t phase1 = phase1Frame[idx]; 
    float_t b_float = mRaw_float + phase1 + maskNegatives;
    float_t d_float = mRaw_float + phase0;

    float_t range = a_float*b_float + c_float*d_float; 
    if (range < 0) 
    {
      range = 0;
    }

    assert(idx < fRanges.size());

    fRanges[idx] = range; 

  }
  
}

