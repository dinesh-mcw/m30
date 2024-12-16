/**
 * @file binning_float.cpp
 * @brief Performs binning on 2D raw data using 32-bit floating point operations
 * on the CPU.
 * 
 * Also includes 1D binning for use in Stripe Mode.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "Binning.h"
#include "FloatVectorPool.h"
#include "GPixel.h"
#include "RtdMetadata.h"
#include "RawToDepthDsp.h"
#include <cassert>

/**
 * @brief The common routine called for 1-dimensional binning
 * 
 * @param rawRoi Raw data NUM_GPIXEL_PHASES (3) elements of length roiWidth
 * @param binnedRawRoi Raw data reduced by binning of size NUM_GPIXEL_PHASES * roiWidth/binX
 * @param roiWidth The width of the rawRoi input buffer in raw triplets.
 * @param binX The binning rate, either 1, 2, or 4
 */
void Binning::bin1xN(const std::vector<float_t> &rawRoi, std::vector<float_t> &binnedRawRoi, uint32_t roiWidth, uint32_t binX)
{
  if (binX <= 1)
  {
    bin1x1(rawRoi, binnedRawRoi, roiWidth);
    return;
  }
  if (binX == 2)
  {
    bin1x2(rawRoi, binnedRawRoi, roiWidth);
    return;
  }
  if (binX == 4)
  {
    bin1x4(rawRoi, binnedRawRoi, roiWidth);
    return;
  }
  LLogErr("Only binning of 1,2, or 4 are allowed.")
  assert(false);
}

/**
 * @brief Returns an identical copy of the input buffer.
 * 
 * @param rawRoi A NUM_GPIXEL_PHASES*roiWidth buffer of floats containing 3-element raw data.
 * @param binnedRawRoi An exact copy of the input. Must be the same size as the input. 
 * @param roiWidth The length of the input buffer in 3-word elements.
 */
void Binning::bin1x1(const std::vector<float_t> &rawRoi, std::vector<float_t> &binnedRawRoi, uint32_t roiWidth)
{
  assert(rawRoi.size() == binnedRawRoi.size());
  assert(rawRoi.size() == size_t(NUM_GPIXEL_PHASES*roiWidth));
  std::copy(rawRoi.begin(), rawRoi.end(), binnedRawRoi.begin());
}

/**
 * @brief Bins the 1D input data by reducing it in size by 2.
 * Computes the average of neighboring pixels.
 * 
 * @param rawRoi A NUM_GPIXEL_PHASES*roiWidth buffer of floats containing 3-element raw data.
 * @param binnedRawRoi A NUM_GPIXEL_PHASES * roiWidth/2 buffer of floats containing the binned data.
 * @param roiWidth The length of rawRoi in raw triplets.
 */
void Binning::bin1x2(const std::vector<float_t> &rawRoi, std::vector<float_t> &binnedRawRoi, uint32_t roiWidth)
{
  constexpr uint32_t binning=2;
  constexpr float_t scale = 1.0F;
  uint32_t binnedWidth = roiWidth / binning;

  assert(rawRoi.size() == size_t(NUM_GPIXEL_PHASES*roiWidth));
  assert(binnedRawRoi.size() == size_t(NUM_GPIXEL_PHASES * (roiWidth / binning)));

  std::fill(binnedRawRoi.begin(), binnedRawRoi.end(), 0.0F);
  for (uint32_t idx=0; idx<binnedWidth*NUM_GPIXEL_PHASES; idx+=NUM_GPIXEL_PHASES)
  {
    uint32_t binnedAIdx = idx;
    uint32_t unbinnedAIdx = binning*idx;

    assert(binnedAIdx+2U < binnedRawRoi.size());
    assert(unbinnedAIdx+NUM_GPIXEL_PHASES+2U < rawRoi.size());
    binnedRawRoi[binnedAIdx+0U] = (rawRoi[unbinnedAIdx+0U] + rawRoi[unbinnedAIdx+NUM_GPIXEL_PHASES+0U]) / scale;
    binnedRawRoi[binnedAIdx+1U] = (rawRoi[unbinnedAIdx+1U] + rawRoi[unbinnedAIdx+NUM_GPIXEL_PHASES+1U]) / scale;
    binnedRawRoi[binnedAIdx+2U] = (rawRoi[unbinnedAIdx+2U] + rawRoi[unbinnedAIdx+NUM_GPIXEL_PHASES+2U]) / scale;
  }
}

/**
 * @brief Bins the 1D input data by 4.
 * Computes the average of the neighboring 4 pixels.
 * 
 * @param rawRoi A NUM_GPIXEL_PHASES*roiWidth buffer of floats containing 3-element raw data.
 * @param binnedRawRoi A buffer "div-4" smaller than the input buffer with each pixel being the average of four in rawRoi.
 * @param roiWidth The number of raw triplets in the rawRoi buffer.
 */
void Binning::bin1x4(const std::vector<float_t> &rawRoi, std::vector<float_t> &binnedRawRoi, uint32_t roiWidth)
{
  constexpr uint32_t binning=4;
  constexpr float_t scale = 1.0F;
  uint32_t binnedWidth = roiWidth/binning;

  assert(rawRoi.size() == size_t(NUM_GPIXEL_PHASES*roiWidth));
  assert(binnedRawRoi.size() == size_t(NUM_GPIXEL_PHASES * (roiWidth / binning)));

  std::fill(binnedRawRoi.begin(), binnedRawRoi.end(), 0.0F);
  for (uint32_t idx=0; idx<binnedWidth*NUM_GPIXEL_PHASES; idx+=NUM_GPIXEL_PHASES)
  {

    uint32_t binnedAIdx = idx;
    uint32_t unbinnedAIdx = binning*idx;
    assert(binnedAIdx+2U < binnedRawRoi.size());
    assert(unbinnedAIdx+3U*NUM_GPIXEL_PHASES+2U < rawRoi.size());
    binnedRawRoi[binnedAIdx+0U] = (rawRoi[unbinnedAIdx+0U*NUM_GPIXEL_PHASES+0U] + 
                                   rawRoi[unbinnedAIdx+1U*NUM_GPIXEL_PHASES+0U] + 
                                   rawRoi[unbinnedAIdx+2U*NUM_GPIXEL_PHASES+0U] + 
                                   rawRoi[unbinnedAIdx+3U*NUM_GPIXEL_PHASES+0U]) / scale;
    binnedRawRoi[binnedAIdx+1U] = (rawRoi[unbinnedAIdx+0U*NUM_GPIXEL_PHASES+1U] + 
                                   rawRoi[unbinnedAIdx+1U*NUM_GPIXEL_PHASES+1U] + 
                                   rawRoi[unbinnedAIdx+2U*NUM_GPIXEL_PHASES+1U] + 
                                   rawRoi[unbinnedAIdx+3U*NUM_GPIXEL_PHASES+1U]) / scale;
    binnedRawRoi[binnedAIdx+2U] = (rawRoi[unbinnedAIdx+0U*NUM_GPIXEL_PHASES+2U] + 
                                   rawRoi[unbinnedAIdx+1U*NUM_GPIXEL_PHASES+2U] + 
                                   rawRoi[unbinnedAIdx+2U*NUM_GPIXEL_PHASES+2U] + 
                                   rawRoi[unbinnedAIdx+3U*NUM_GPIXEL_PHASES+2U]) / scale;
  }

}

// shift defaults to "1" allowing for 2bits of numerical growth plus one extra bit.
void Binning::bin2x2(const std::vector<float_t> &frame, std::vector<float_t> &binnedFrame, std::array<uint32_t,2> roiSize, int32_t shift) { 
  
  auto binning=2;
  const float_t factor = powf(2.0F, float_t(shift));
  
  std::vector<uint32_t> binnedSize { roiSize[0]/binning, roiSize[1]/binning }; // odd-height ROIs clip off the bottom row.
  assert((uint32_t)frame.size() >= 3U*binnedSize[0]*binnedSize[1]*2U*2U); // odd-height ROIs might have an extra row.
  assert((uint32_t)binnedFrame.size() == 3U * binnedSize[0]*binnedSize[1]);
  for (uint32_t row=0; row<binnedSize[0]; row++) {
    for (uint32_t col=0; col<binnedSize[1]; col++) {
      float_t
	    aSum  = frame[3*binning*col + 0*3 + 0 + (row*binning + 0)*3*roiSize[1]];
      aSum += frame[3*binning*col + 1*3 + 0 + (row*binning + 0)*3*roiSize[1]];
      aSum += frame[3*binning*col + 0*3 + 0 + (row*binning + 1)*3*roiSize[1]];
      aSum += frame[3*binning*col + 1*3 + 0 + (row*binning + 1)*3*roiSize[1]];
      
      float_t
	    bSum  = frame[3*binning*col + 0*3 + 1 + (row*binning + 0)*3*roiSize[1]];
      bSum += frame[3*binning*col + 1*3 + 1 + (row*binning + 0)*3*roiSize[1]];
      bSum += frame[3*binning*col + 0*3 + 1 + (row*binning + 1)*3*roiSize[1]];
      bSum += frame[3*binning*col + 1*3 + 1 + (row*binning + 1)*3*roiSize[1]];
      
      float_t
	    cSum  = frame[3*binning*col + 0*3 + 2 + (row*binning + 0)*3*roiSize[1]];
      cSum += frame[3*binning*col + 1*3 + 2 + (row*binning + 0)*3*roiSize[1]];
      cSum += frame[3*binning*col + 0*3 + 2 + (row*binning + 1)*3*roiSize[1]];
      cSum += frame[3*binning*col + 1*3 + 2 + (row*binning + 1)*3*roiSize[1]];

      aSum *= factor;
      bSum *= factor;
      cSum *= factor;
      
      binnedFrame[3*col + 0 + 3*binnedSize[1]*row] = float_t(aSum); 
      binnedFrame[3*col + 1 + 3*binnedSize[1]*row] = float_t(bSum);
      binnedFrame[3*col + 2 + 3*binnedSize[1]*row] = float_t(cSum);
    }
  }
}

void Binning::bin4x4(const std::vector<float_t> &frame, std::vector<float_t> &binnedFrame, std::array<uint32_t,2> roiSize) {
  SCOPED_VEC_F(binned2x2, NUM_GPIXEL_PHASES*roiSize[0]*roiSize[1]/4);
  bin2x2(frame, binned2x2, roiSize, 0);

  bin2x2(binned2x2, binnedFrame, { roiSize[0]/2, roiSize[1]/2 }, 0); 
  
}

void Binning::bin1x1(const std::vector<float_t> &frame, std::vector<float_t> &binnedFrame, std::array<uint32_t,2> roiSize) {
  assert((uint32_t)frame.size() == 3*roiSize[0]*roiSize[1]);
  assert(frame.size() == binnedFrame.size());
  
  for (uint32_t idx=0; idx<frame.size(); idx++) {
    binnedFrame[idx] = frame[idx];
  }
}

void Binning::binMxN(const std::vector<float> &frame, std::vector<float> &binnedFrame, std::array<uint32_t,2> roiSize, std::array<uint32_t,2> binning) {
  if (binning[0] == 1 && binning[1] == 1) {
    bin1x1(frame, binnedFrame, roiSize);
    return;
  }
  if (binning[0] == 2 && binning[1] == 2) {
    bin2x2(frame, binnedFrame, roiSize);
    return;
  }
  if (binning[0] == 4 && binning[1] == 4) {
    bin4x4(frame, binnedFrame, roiSize);
    return;
  }
    
  LLogErr("MxN binning is not supported. Binning is set to " << binning[0] << "x" << binning[1]);
}

