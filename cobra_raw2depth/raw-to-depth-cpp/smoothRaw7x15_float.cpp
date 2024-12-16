/**
 * @file smoothRaw7x15_float.cpp
 * @brief A size-specific optimizer-friendly implementation of smoothing
 * raw iTOF data with a 7x15 kernel using 32-bit float-point operations on
 * the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthDsp.h"
#include "FloatVectorPool.h"
#include "RtdMetadata.h"
#include <cassert>

#define sum7(sum, roi, idx, k) {\
    assert((idx)+9 < (roi).size());  \
    assert((idx)-9 >= 0);          \
    (sum)  = (roi)[(idx)- 9]*(k)[0];	 \
    (sum) += (roi)[(idx)- 6]*(k)[1];	 \
    (sum) += (roi)[(idx)- 3]*(k)[2];	 \
    (sum) += (roi)[(idx)+ 0]*(k)[3];	 \
    (sum) += (roi)[(idx)+ 3]*(k)[4];	 \
    (sum) += (roi)[(idx)+ 6]*(k)[5];	 \
    (sum) += (roi)[(idx)+ 9]*(k)[6];	 \
  }                              \
  
#define sum15(sum, roi, idx, k) { \
    assert((k).size() == 15);       \
    assert((idx)+21 < (roi).size());  \
    assert((idx)-21 >= 0);          \
    (sum)  = (roi)[(idx)- 21]*(k)[0];	 \
    (sum) += (roi)[(idx)- 18]*(k)[1];	 \
    (sum) += (roi)[(idx)- 15]*(k)[2];	 \
    (sum) += (roi)[(idx)- 12]*(k)[3];	 \
    (sum) += (roi)[(idx)- 9]* (k)[4];	   \
    (sum) += (roi)[(idx)- 6]* (k)[5];	 \
    (sum) += (roi)[(idx)- 3]* (k)[6];	 \
    (sum) += (roi)[(idx)+ 0]* (k)[7];	 \
    (sum) += (roi)[(idx)+ 3]* (k)[8];	 \
    (sum) += (roi)[(idx)+ 6]* (k)[9];	 \
    (sum) += (roi)[(idx)+ 9]* (k)[10];	 \
    (sum) += (roi)[(idx)+ 12]*(k)[11];	 \
    (sum) += (roi)[(idx)+ 15]*(k)[12];	 \
    (sum) += (roi)[(idx)+ 18]*(k)[13];	 \
    (sum) += (roi)[(idx)+ 21]*(k)[14];	 \
  }                              \

constexpr uint32_t VKERNEL_IDX { 6 };
constexpr uint32_t VKERNEL_SIZE { 15 };
constexpr uint32_t HKERNEL_IDX { 3 };
constexpr uint32_t HKERNEL_SIZE { 7 };
void RawToDepthDsp::smoothRaw7x15(const std::vector<float_t> &roi, std::vector<float_t> &smoothedRoi, std::array<uint32_t,2> size) {

  assert(roi.size() == smoothedRoi.size());
  assert(size[0] >= VKERNEL_SIZE);
  assert(size[1] >= HKERNEL_SIZE);

  auto paddedSize = size;
  SCOPED_VEC_F(transposedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  SCOPED_VEC_F(vSmoothedTransposedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  SCOPED_VEC_F(vSmoothedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  
  transposeRaw(roi, transposedRoi, paddedSize);
  
  auto rowStart = 0U;
  auto rowPitch = paddedSize[0]*NUM_GPIXEL_PHASES; // transposed input pitch by num rows.

  for (auto colIdx=0; colIdx< paddedSize[1]; colIdx++)
  {
    auto rowIdx = rowStart;
    for (auto rowCount=0U; rowCount<HKERNEL_SIZE; rowCount++)
    {
      assert(rowIdx+2 < vSmoothedTransposedRoi.size());
      assert(rowIdx+2 < transposedRoi.size());
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
    }
    rowStart += rowPitch;
  }

  rowStart = NUM_GPIXEL_PHASES*(paddedSize[0]-HKERNEL_SIZE);
  for (auto colIdx=0U; colIdx< paddedSize[1]; colIdx++)
  {
    auto rowIdx = rowStart;
    for (auto rowCount=0U; rowCount<HKERNEL_SIZE; rowCount++)
    {
      assert(rowIdx+2 < vSmoothedTransposedRoi.size());
      assert(rowIdx+2 < transposedRoi.size());
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
    }
    rowStart += rowPitch;
  }

  const auto &k15 = _fKernels[VKERNEL_IDX];
  assert(k15.size() == VKERNEL_SIZE);

  rowStart = NUM_GPIXEL_PHASES*(VKERNEL_SIZE/2); //skip left 7 elements with 15-element filter.
  const auto numRows = paddedSize[0]-VKERNEL_SIZE-1;  // 

  // filter a->b
  float_t sum;
  for (auto colIdx=0U; colIdx<paddedSize[1]; colIdx++) {
    auto rowIdx = rowStart;
    for (auto rowCount=0U; rowCount<numRows; rowCount++) {
      sum15(sum, transposedRoi, rowIdx, k15);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

      sum15(sum, transposedRoi, rowIdx, k15);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

      sum15(sum, transposedRoi, rowIdx, k15);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

    }
    rowStart += rowPitch;
  }

  transposeRaw(vSmoothedTransposedRoi, vSmoothedRoi, {paddedSize[1], paddedSize[0]});
  // pr_(c, paddedSize);

  auto colPitch = paddedSize[1]*NUM_GPIXEL_PHASES;
  auto colStart = 0U;
  for (auto rowIdx=0U; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0U; colCount<3; colCount++) {
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
    }
    colStart += colPitch;
  }

  colStart = NUM_GPIXEL_PHASES*(paddedSize[1] - HKERNEL_SIZE/2);
  for (auto rowIdx=0; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0; colCount<3; colCount++) {
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
    }
    colStart += colPitch;
  }

  const auto &kern7 = _fKernels[HKERNEL_IDX];
  assert(kern7.size() == 7);

  colStart = NUM_GPIXEL_PHASES*(HKERNEL_SIZE/2); // skip left 3 elements with 7-element filter.
  auto numCols  = paddedSize[1]-HKERNEL_SIZE-1;

  for (auto rowIdx=0; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0; colCount<numCols; colCount++) {
      sum7(sum, vSmoothedRoi, colIdx, kern7);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

      sum7(sum, vSmoothedRoi, colIdx, kern7);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

      sum7(sum, vSmoothedRoi, colIdx, kern7);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

    }
    colStart += colPitch;
  }
}
