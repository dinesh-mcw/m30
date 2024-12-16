/**
 * @file RawToDepthGetters_float.cpp
 * @brief The routines to convert local floating-point buffers into 16-bit data 
 * for transmission over the network.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "RawToDepthV2_float.h"
#include <cmath>


/**
 * @brief Each pixel in the output FOV is assigned an integer index that corresponds to which input
 * ROI was used to generate this pixel. This index is used to look into the timestamps vector and 
 * retrieve a precise timestamp for each pixel in the output buffer.
 * 
 * @param roiIndices a full-VGA-sized buffer containing an index that indicates which timestamp was used when
 * that pixel was acquired. This routine indexes into that buffer using the same logic as the pixelMask
 * seen below
 * @param fovStart These values indicate indices into the roiIndices buffer
 * @param fovStep 
 * @param fovSize 
 * @param size The size of the output roiIndicesFov
 * @return std::shared_ptr<std::vector<uint16_t>> An FOV-sized buffer containing indices to which ROI
 * was used to generate each pixel.
 */
std::shared_ptr<std::vector<uint16_t>> RawToDepthV2_float::getRoiIndices(std::vector<int32_t> &roiIndices, 
                                                                         std::array<uint16_t,2> fovStart, 
                                                                         std::array<uint16_t,2> fovStep, 
                                                                         std::array<uint16_t,2> fovSize, 
                                                                         std::array<uint32_t,2> size)
{
  auto roiIndicesFov = std::make_shared<std::vector<uint16_t>>(size[0]*size[1], 0);
  uint16_t pixelMaskStartY = fovStart[0];
  uint16_t pixelMaskStartX = fovStart[1];
  uint16_t pixelMaskStepY  = fovStep[0];
  uint16_t pixelMaskStepX  = fovStep[1];
  uint16_t pixelMaskStride = fovSize[1];

  // The roiIndices (used for indexing the timestamp array)
  // is pre-initialized to -1 to indicate unassigned pixels.
  // If an output pixel contains -1 for its timestamp index, then the 
  // most recent (above and to the left) value is substituted for the time
  // for that point.
  uint16_t lastGood = 0;
  for (uint32_t idx = 0; idx < size[0]*size[1]; idx++)
  {
    uint32_t rangeX = idx%size[1];
    uint32_t rangeY = idx/size[1];
    uint32_t pixelMaskX = pixelMaskStartX + rangeX*pixelMaskStepX;
    uint32_t pixelMaskY = pixelMaskStartY + rangeY*pixelMaskStepY;
    uint32_t pixelMaskIdx = pixelMaskX + pixelMaskStride*pixelMaskY;
    auto roiIndex = roiIndices[pixelMaskIdx];
    if (roiIndex < 0) 
    {
      roiIndex = lastGood;
    }
    else 
    {
      lastGood = roiIndex;
    }
    roiIndicesFov->at(idx) = roiIndex;
  }
  return roiIndicesFov;
}

