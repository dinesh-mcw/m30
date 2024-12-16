/**
 * @file RawToDepthDsp.cpp
 * @brief The algorithms for performing digital signal processing to
 * generate point clouds from raw iTOF data.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#include "RawToDepthDsp.h"
#include "LumoLogger.h"
#include "LumoUtil.h"
#include "FloatVectorPool.h"
#include "RtdMetadata.h"
#include "Binning.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>

const std::vector<std::vector<float_t>> RawToDepthDsp::_fKernels =
{
  { 0.0000000e+00, 1.0000000e+00, 0.0000000e+00, },
  { 1.9684139e-01, 6.0631722e-01, 1.9684139e-01, },
  { 6.646033000e-03, 1.942255544e-01, 5.982568252e-01, 1.942255544e-01, 6.646033000e-03 },
  { 4.433048175e-03, 5.400558262e-02, 2.420362294e-01, 3.990502797e-01, 2.420362294e-01, 5.400558262e-02, 4.433048175e-03 },
  { 3.325727091e-03, 2.381792204e-02, 9.719199228e-02, 2.259781525e-01, 2.993724122e-01, 2.259781525e-01, 9.719199228e-02, 2.381792204e-02, 3.325727091e-03 },
  { 2.661264666e-03, 1.344761071e-02, 4.740849576e-02, 1.166060837e-01, 2.000968398e-01, 2.395594109e-01, 2.000968398e-01, 1.166060837e-01, 4.740849576e-02, 1.344761071e-02, 2.661264666e-03 },
  { 1.901645579e-03, 6.275148539e-03, 1.723257069e-02, 3.938290545e-02, 7.490262923e-02, 1.185544877e-01, 1.561602730e-01, 1.711806797e-01, 1.561602730e-01, 1.185544877e-01, 7.490262923e-02, 3.938290545e-02, 1.723257069e-02, 6.275148539e-03, 1.901645579e-03 },
};

const std::vector<std::vector<float_t>> RawToDepthDsp::_fKernelsNoCenter =
{
  { 0.0000000e+00,   1.0000000e+00,   0.0000000e+00, },
  { 5.000000000e-01, 0.000000000e+00, 5.000000000e-01 },
  { 1.654298919e-02, 4.834570108e-01, 0.000000000e+00, 4.834570108e-01, 1.654298919e-02 },
  { 7.376737230e-03, 8.986705675e-02, 4.027562060e-01, 0.000000000e+00, 4.027562060e-01, 8.986705675e-02, 7.376737230e-03 },
  { 4.746782954e-03, 3.399512445e-02, 1.387213321e-01, 3.225367605e-01, 0.000000000e+00, 3.225367605e-01, 1.387213321e-01, 3.399512445e-02, 4.746782954e-03},
  { 3.499635217e-03, 1.768397282e-02, 6.234345778e-02, 1.533401627e-01, 2.631327715e-01, 0.000000000e+00, 2.631327715e-01, 1.533401627e-01, 6.234345778e-02, 1.768397282e-02, 3.499635217e-03 },
  { 2.294403053e-03, 7.571189988e-03, 2.079170969e-02, 4.751687670e-02, 9.037268726e-02, 1.430402077e-01, 1.884129256e-01, 0.000000000e+00, 1.884129256e-01, 1.430402077e-01, 9.037268726e-02, 4.751687670e-02, 2.079170969e-02, 7.571189988e-03, 2.294403053e-03 }
};

const std::vector<float_t> RawToDepthDsp::_gaussian6 { 0.4578333497F, 0.7548395991F, 0.9692332149F, 0.9692332149F, 0.7548395991F, 0.4578333497F };
const float_t RawToDepthDsp::_gaussian6NumberOfSums = 4.363812447F;

const std::vector<float_t> RawToDepthDsp::_gaussian8 { 0.2162651718F, 0.4578333497F, 0.7548395991F, 0.9692332149F, 0.9692332149F, 0.7548395991F, 0.4578333497F, 0.2162651718F };
const float_t RawToDepthDsp::_gaussian8NumberOfSums = 4.79634285F;

const std::vector<float_t> RawToDepthDsp::_rect8 { 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F };
const float_t RawToDepthDsp::_rect8NumberOfSums = 8.0F;

const std::vector<float_t> RawToDepthDsp::_rect6 { 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F };
const float_t RawToDepthDsp::_rect6NumberOfSums = 6.0F;

void RawToDepthDsp::median1d(const std::vector<float_t> &range, std::vector<float_t> &medianFilteredRange, uint32_t binning)
{
  std::copy(range.begin(), range.end(), medianFilteredRange.begin());
  assert(range.size() == medianFilteredRange.size());
  const std::array<uint32_t, 5> medianSizeLUT {5, 5, 5, 3, 3};
  assert(binning < medianSizeLUT.size());
  auto medFilterLength = medianSizeLUT[binning];
  auto vecLen = range.size();

  std::vector<float_t> points(medFilterLength);
  for (auto idx=medFilterLength/2; idx<vecLen-medFilterLength/2-1; idx++)
  {
    for (auto pointIdx=0; pointIdx<medFilterLength; pointIdx++)
    {
      assert(pointIdx < points.size());
      assert(idx + pointIdx - medFilterLength/2 < range.size());
      points[pointIdx] = range[idx + pointIdx - medFilterLength/2];
    }
    std::nth_element(points.begin(), points.begin() + medFilterLength/2, points.end());
    medianFilteredRange[idx] = points[medFilterLength/2];
  }
}

/**
 * @brief Collapses a raw row vertically,then bins horizontally to generated
 * a 1D raw output ROI.
 * 
 * Supports a "rowOffset," which means that the output ROI can be generated from a subset 
 * of the input rows. This allows the caller to vertically collapse only a portion of the 
 * input height.
 * 
 * @param rawRoi A 2D raw input array, containing a single ROI usually 6 or 8 rows high.
 * @param collapsedRoi A 1D output array, shorter than the width of the input array by a factor of binning.
 * @param weights Either a column-sized array of weights to applied to each column of the rawRoi, OR a rawRoi-sized array of snr-weights to be applied across the entire ROI.
 * @param binning The binning specified in the input metadata, which is only applied horizontally to the raw input array.
 * @param roiSize 2D (height, width in raw triplets) size of the input rawRoi used for processing. This is shorter than the input ROI if rowOffset>0.
 * @param rowOffset The number of rows to skip when performing the input processing.
 */
void RawToDepthDsp::collapseRawRoi(const std::vector<float_t> & rawRoi, std::vector<float_t> &collapsedRoi, const std::vector<float_t> &weights, 
                                   const std::array<uint32_t,2> &binning, std::array<uint32_t, 2> roiSize, uint32_t rowOffset)
{
  auto roiWidth = roiSize[1];
  auto roiHeight = roiSize[0];
  auto binX = binning[1];
  assert(collapsedRoi.size() == size_t(NUM_GPIXEL_PHASES * (roiWidth/binX)));
  assert(rawRoi.size() >= size_t(roiHeight * NUM_GPIXEL_PHASES*roiWidth));

  SCOPED_VEC_F(vCollapsedRoi, NUM_GPIXEL_PHASES*roiWidth);
  std::fill(vCollapsedRoi.begin(), vCollapsedRoi.end(), 0.0F);

  for (auto rowIdx=rowOffset; rowIdx<roiHeight; rowIdx++)
  {
    for (auto colIdx=0; colIdx<NUM_GPIXEL_PHASES*roiWidth; colIdx++)
    { 
      auto rawRoiIdx = colIdx + NUM_GPIXEL_PHASES*roiWidth*rowIdx;
      assert(rawRoiIdx < rawRoi.size());

      uint32_t weightsIdx = rowIdx; // for Rect and Gaussian
      if (weights.size() == rawRoi.size()) // for snr-weighted sum
      {
        weightsIdx = rawRoiIdx;
      }

      assert(colIdx < vCollapsedRoi.size());
      assert(rowIdx < weights.size());
      vCollapsedRoi[colIdx] += rawRoi[rawRoiIdx] * weights[weightsIdx];
    }
  }

  Binning::bin1xN(vCollapsedRoi, collapsedRoi, roiWidth, binX);

}

void RawToDepthDsp::minMax1d(const std::vector<float_t> &rawRoi0, const std::vector<float_t> &rawRoi1, std::vector<float_t> &mask,
	                     std::array<uint32_t, 2> rawRoiSize, uint32_t binning)
{

}

void RawToDepthDsp::fillMissingRows(const std::vector<float_t> &inFrame, std::vector<float_t> &outFrame, std::array<uint32_t,2> frameSize, std::vector<bool> &activeRows)
{

  if (frameSize[0] < 3)
  {
    std::copy(inFrame.begin(), inFrame.end(), outFrame.begin());
    return;
  }

  assert(std::size_t(NUM_GPIXEL_PHASES*frameSize[0]*frameSize[1]) <= outFrame.size());
  assert(inFrame.size() >= std::size_t(frameSize[0]*frameSize[1]*NUM_GPIXEL_PHASES)); // > can happen if inFrame size % binning != 0
  
  int idxBottomRow = int((frameSize[0]-1) * frameSize[1] * NUM_GPIXEL_PHASES);
  for (auto col=0; col<frameSize[1]*NUM_GPIXEL_PHASES; col++)
  {
    outFrame[col] = inFrame[col]; // top row
    outFrame[idxBottomRow + col] = inFrame[idxBottomRow + col];
  }

  for (auto row=1; row<frameSize[0]-1; row++)
  {
    bool thisRowActive = activeRows[row];
    bool upRowActive = activeRows[row-1];
    bool downRowActive = activeRows[row+1];
    for (auto col=0; col<frameSize[1]*NUM_GPIXEL_PHASES; col++)
    {
      auto idx = col + row*(frameSize[1]*NUM_GPIXEL_PHASES);
      auto upidx = idx - frameSize[1]*NUM_GPIXEL_PHASES;
      auto downidx = idx + frameSize[1]*NUM_GPIXEL_PHASES;

      auto downval = inFrame[downidx];
      auto upval = inFrame[upidx];
      auto val = inFrame[idx];

      if (!thisRowActive && upRowActive && downRowActive)
      {
        const float_t avg = 0.5F*(upval + downval);
        outFrame[idx] = avg;
      }
      else if (!thisRowActive && upRowActive)
      {
        outFrame[idx] = upval;
      }
      else if (!thisRowActive && downRowActive)
      {
        outFrame[idx] = downval;
      }
      else
      {
        outFrame[idx] = val;
      }

    }
  }
}

void RawToDepthDsp::computeSnrSquaredWeights(const std::vector<float_t> &rawRoi0, 
                                             const std::vector<float_t> &rawRoi1, 
                                             std::vector<float_t> &snrWeights, 
                                             float_t &snrWeightsNumberOfSums,
                                             uint32_t roiHeight, uint32_t roiWidth, uint32_t rowOffset)
{
  assert(rawRoi0.size() == rawRoi1.size());
  assert(rawRoi0.size() == snrWeights.size());
  assert(rawRoi0.size() >= size_t(NUM_GPIXEL_PHASES*roiHeight*roiWidth));

  if (roiHeight == _gaussian6.size())
  {
    snrWeightsNumberOfSums = _gaussian6NumberOfSums;
  }
  else
  {
    snrWeightsNumberOfSums = _gaussian8NumberOfSums;
  }

  auto snrWeightsIdx = 0;
  for (auto idx=0; idx<roiHeight*roiWidth; idx++)
  {
    assert(size_t((idx + rowOffset*roiWidth)*NUM_GPIXEL_PHASES) < rawRoi0.size());
    float_t snr  = sqrtf(computeSnrSquared(rawRoi0, idx + rowOffset*roiWidth));
            snr += sqrtf(computeSnrSquared(rawRoi1, idx + rowOffset*roiWidth)); // No need to scale for the average. This value is rescaled to a peak of 1.0F below.
    snrWeights[snrWeightsIdx++] = snr;
    snrWeights[snrWeightsIdx++] = snr;
    snrWeights[snrWeightsIdx++] = snr;
  }

  for (auto colIdx=0; colIdx<roiWidth*NUM_GPIXEL_PHASES; colIdx+=NUM_GPIXEL_PHASES)
  {
    float_t columnMax=0.0F;
    for (auto rowIdx=rowOffset; rowIdx<roiHeight; rowIdx++)
    {
      auto val = snrWeights[colIdx + rowIdx*roiWidth*NUM_GPIXEL_PHASES];
      if (val > columnMax)
      {
        columnMax = val;
      }
    }

    for (auto rowIdx=rowOffset; rowIdx<roiHeight; rowIdx++)
    {
      float_t newWeight = snrWeights[colIdx + rowIdx*roiWidth*NUM_GPIXEL_PHASES] / columnMax; // Scale each column to a peak of 1.0F
      snrWeights[colIdx + 0 + rowIdx*roiWidth*NUM_GPIXEL_PHASES] = newWeight;
      snrWeights[colIdx + 1 + rowIdx*roiWidth*NUM_GPIXEL_PHASES] = newWeight;
      snrWeights[colIdx + 2 + rowIdx*roiWidth*NUM_GPIXEL_PHASES] = newWeight;
    }
  }  
}

inline float RawToDepthDsp::computeSnrSquared(const std::vector<float_t> &rawRoi, uint32_t idx)
{
    auto aIdx = idx*NUM_GPIXEL_PHASES;
    assert(aIdx+2 < rawRoi.size());
    auto rawA = rawRoi[aIdx];
    auto rawB = rawRoi[aIdx + 1];
    auto rawC = rawRoi[aIdx + 2];

    if (rawA <= rawB && rawA <= rawC)
    {
      auto tmp = rawC;
      rawC = rawA;
      rawA = rawB;
      rawB = tmp;
    }

    else if (rawB <= rawC && rawB < rawA)
    {
      auto tmp = rawA;
      rawA = rawC;
      rawC = rawB;
      rawB = tmp;
    }
    
    //float_t snr = (a + b - 2*c) / sqrtf(2.0F * c);
    const float_t num = rawA + rawB - 2*rawC;
    const float snr_squared = num*num / (2.0F * rawC);
    return snr_squared;

}
void RawToDepthDsp::snrVoteV2(const std::vector<float_t> &roi0, const std::vector<float_t> &roi1, std::vector<std::vector<float_t>> &rawFov, std::vector<float_t> &snrSquaredFov, uint32_t fovOffset)
{
  assert(rawFov.size() == 2);
  assert(roi0.size() == roi1.size());
  assert(roi0.size() % NUM_GPIXEL_PHASES == 0);
  auto &fov0 = rawFov[0];
  auto &fov1 = rawFov[1];
  auto numSnrValues = roi0.size()/NUM_GPIXEL_PHASES;

  // idx counts raw triplets.
  for (uint32_t idx = 0; idx < numSnrValues; idx++)
  {   
    auto snr0 = computeSnrSquared(roi0, idx);
    auto snr1 = computeSnrSquared(roi1, idx);
    auto snr = snr0+snr1;

    assert(idx+fovOffset < snrSquaredFov.size());
    if (snr > snrSquaredFov[idx + fovOffset])
    {
      auto aIdx = idx*NUM_GPIXEL_PHASES;
      auto offset = NUM_GPIXEL_PHASES * fovOffset;
      assert(aIdx+2 < roi0.size());
      fov0[aIdx + offset + 0] = roi0[aIdx + 0]; // raw values in triplets.
      fov0[aIdx + offset + 1] = roi0[aIdx + 1]; // raw values in triplets.
      fov0[aIdx + offset + 2] = roi0[aIdx + 2]; // raw values in triplets.

      assert(aIdx+2 < roi1.size());
      fov1[aIdx + offset + 0] = roi1[aIdx + 0];
      fov1[aIdx + offset + 1] = roi1[aIdx + 1];
      fov1[aIdx + offset + 2] = roi1[aIdx + 2];

      assert(idx + fovOffset < snrSquaredFov.size());
      snrSquaredFov[idx + fovOffset] = snr;
    }
  }
}

void RawToDepthDsp::minMaxRecursive(const std::vector<float_t> &frame, std::vector<float_t> &minMaxMask, std::vector<uint32_t> filterSize, std::array<uint32_t,2> frameSize, float_t minMaxThresh)
{

  std::fill(minMaxMask.begin(), minMaxMask.end(), 0.0F);

  if (2 != filterSize.size())
  {
    return;
  }
  
  if (frameSize[0] < filterSize[0] || frameSize[1] < filterSize[1])
  {
    return;
  }

  minMaxIntra(frame, minMaxMask, filterSize, frameSize, minMaxThresh);

  std::vector<float_t> frameReversed(frame.rbegin(), frame.rend()); // reverse iterator.
  std::vector<float_t> minMaxMaskReversed(frame.size(), 0.0F);

  minMaxIntra(frameReversed, minMaxMaskReversed, filterSize, frameSize, minMaxThresh);

  std::reverse(minMaxMaskReversed.begin(), minMaxMaskReversed.end());
  for (auto idx = 0; idx < minMaxMask.size(); idx++)
  {
    minMaxMask[idx] = minMaxMask[idx] * minMaxMaskReversed[idx];
  }
}

inline bool RawToDepthDsp::outOfRangeIntra(const std::vector<float_t> &frame, const std::vector<float_t> &minMaxMask, uint32_t idx, const std::vector<int32_t> &offsets, float_t thresh)
{

  // Find the min and max values in the window.
  float_t minVal = std::numeric_limits<float_t>::max();
  float_t maxVal = std::numeric_limits<float_t>::lowest();
  for (auto offset : offsets)
  {
    assert(idx + offset < frame.size());
    assert(int(idx) + int(offset) >= 0);

    auto mask = minMaxMask[idx + offset];
    auto val = frame[idx + offset];
    if (val < minVal && (0.0F == mask))
    {
      minVal = val;
    }
    if (val > maxVal && (0.0F == mask))
    {
      maxVal = val;
    }
  }

  if (minVal == std::numeric_limits<float_t>::max() || maxVal == std::numeric_limits<float_t>::lowest())
  {
    return false; // no valid points in range.
  }

  // Return true of the total range of values in the sample exceeds the threshold.
  return (maxVal - minVal > thresh);
}

void RawToDepthDsp::minMaxIntra(const std::vector<float_t> &frame, std::vector<float_t> &minMaxMask, std::vector<uint32_t> filterSize, std::array<uint32_t,2> frameSize, float_t minMaxThresh)
{
  std::fill(minMaxMask.begin(), minMaxMask.end(), 0.0F);
  if (2 != filterSize.size())
  {
    return;
  }

  int vFilterSize = int(filterSize[0] | 1U); // guarantee filter size is odd.
  int hFilterSize = int(filterSize[1] | 1U);

  int rowStart = vFilterSize / 2;
  int colStart = hFilterSize / 2;
  int rowPitch = (int)frameSize[1];

  // lookup table for offsets to filters within the window.
  auto offsets = std::vector<int32_t>(std::size_t(vFilterSize * hFilterSize));
  auto offsetIdx = 0;
  for (auto rowIdx = 0; rowIdx < vFilterSize; rowIdx++)
  {
    for (auto colIdx = 0; colIdx < hFilterSize; colIdx++)
    {
      assert(offsetIdx >= 0);
      assert(offsetIdx < offsets.size());
      offsets[offsetIdx] = rowPitch * (rowIdx - rowStart) + (colIdx - colStart);
      offsetIdx++;
    }
  }

  auto numRows = frameSize[0] - vFilterSize + 1;
  auto numColumns = frameSize[1] - hFilterSize + 1;

  uint32_t idxStart = rowStart * rowPitch + colStart;
  for (auto rowIdx = 0; rowIdx < numRows; rowIdx++)
  {
    auto idx = idxStart;
    for (auto colIdx = 0; colIdx < numColumns; colIdx++)
    {
      if (outOfRangeIntra(frame, minMaxMask, idx, offsets, minMaxThresh))
      {
        minMaxMask[idx] = 1.0F;
      }
      idx++;
    }
    idxStart += rowPitch;
  }
}

std::vector<int> RawToDepthDsp::getMedianOffsets(std::array<uint32_t,2> frameSize, std::vector<uint32_t> kernelIndices)
{
  int hFilterSize = int(_fKernels[kernelIndices[0]].size() | 1U); // guarantee filter size is odd.
  int vFilterSize = int(_fKernels[kernelIndices[1]].size() | 1U);

  int rowStart = vFilterSize / 2;
  int colStart = hFilterSize / 2;
  int rowPitch = (int)frameSize[1];

  // generate a lookup table for the pixels to filter.
  std::vector<int> pointOffsets(vFilterSize + hFilterSize - 1);
  int pointIdx = 0;
  for (int idx = -hFilterSize / 2; idx <= hFilterSize / 2; idx++)
  {
    pointOffsets[pointIdx++] = idx;
  }
  for (int idx = -vFilterSize / 2; idx < 0; idx++)
  {
    pointOffsets[pointIdx++] = idx * rowPitch;
  }
  for (int idx = 1; idx <= vFilterSize / 2; idx++)
  {
    pointOffsets[pointIdx++] = idx * rowPitch;
  }

  assert(pointIdx == pointOffsets.size());

  return pointOffsets;
}


#define BIT_ITERS 9
#define BIT_MASK 0x100
uint16_t int_sqrt16(uint16_t squaredValue) // could return a byte.
{
  auto valueTimes4 = uint32_t(squaredValue << 2U); // Compute sqrt(4x_) = 2sqrt(x_), then round the lsb
  uint32_t res = 0;
  uint32_t add = BIT_MASK;

  int idx;
  for (idx = 0; idx < BIT_ITERS; idx++)
  {
    uint32_t temp = res | add;
    uint32_t estSquared = temp * temp;
    if (valueTimes4 >= estSquared) // TODO: non-branching compare.
    {
      res = temp;
    }
    add >>= 1U;
  }
  return (res + 1) >> 1U;
}

void RawToDepthDsp::sh2f(const uint16_t *src, std::vector<float_t> &dst, uint32_t numElements, uint32_t shiftr, uint16_t rawMask)
{
  assert(dst.size() == numElements);
  for (auto idx = 0; idx < numElements; idx++)
  {
    dst[idx] = float_t(uint32_t(src[idx] & rawMask) >> shiftr);
  }
}

void RawToDepthDsp::tapRotation(const std::vector<float_t> &roiVector, std::vector<float_t> &frame, uint32_t freqIdx, std::vector<uint32_t> roiSize, uint32_t numGpixelPhases, bool doTapRotation)
{
  auto roiHeight = roiSize[0];
  auto roiNumColumns = roiSize[1];

  auto roiRowStride = roiNumColumns * numGpixelPhases;
  auto roiImageStride = roiRowStride * roiHeight;

  if (!doTapRotation)
  {
    // copy the raw data into the frame buffer. No sum taps required.
    assert(frame.size() == std::size_t(numGpixelPhases * roiSize[0] * roiSize[1]));

    uint32_t roiVectorIdx = freqIdx * roiImageStride;

    if (roiVectorIdx + frame.size() > roiVector.size()) 
    {
      return; // buffer mis-sized.
    }

    for (uint32_t idx = 0; idx < frame.size(); idx++)
    {
      assert(idx < roiVector.size());
      assert(idx < frame.size());
      auto val = roiVector[roiVectorIdx++];
      frame[idx] = val; // explicit copy allows the roiVector to be larger than necessary.
    }
  }

  assert(frame.size() == std::size_t(numGpixelPhases * roiSize[0] * roiSize[1]));
  assert(frame.size() <= roiVector.size());

  const auto *abc1 = roiVector.data() + std::size_t(freqIdx * roiImageStride);
  const auto *abc2 = roiVector.data() + std::size_t((2 + freqIdx) * roiImageStride);
  const auto *abc3 = roiVector.data() + std::size_t((4 + freqIdx) * roiImageStride);

  if (roiVector.size() < (4 + freqIdx) * roiImageStride + roiSize[0]*roiSize[1])
  {
    return;
  }

  for (std::size_t idx = 0U; idx < std::size_t(roiSize[0]) * std::size_t(roiSize[1]); idx++)
  {
    assert(3*idx + 2 < roiVector.size());
    auto rawA1 = abc1[3U * idx + 0];
    auto rawB1 = abc1[3U * idx + 1];
    auto rawC1 = abc1[3U * idx + 2];

    assert(3*idx + 2 < roiVector.size());
    auto rawA2 = abc2[3U * idx + 0];
    auto rawB2 = abc2[3U * idx + 1];
    auto rawC2 = abc2[3U * idx + 2];

    assert(3*idx + 2 < roiVector.size());
    auto rawA3 = abc3[3U * idx + 0];
    auto rawB3 = abc3[3U * idx + 1];
    auto rawC3 = abc3[3U * idx + 2];

    float_t aSum = rawA1 + rawB2 + rawC3;
    float_t bSum = rawB1 + rawC2 + rawA3;
    float_t cSum = rawC1 + rawA2 + rawB3;

    assert(3*idx + 2 < frame.size());
    frame[3U * idx + 0] = aSum;
    frame[3U * idx + 1] = bSum;
    frame[3U * idx + 2] = cSum;
  }
}
