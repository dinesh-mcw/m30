/**
 * @file smoothRaw5x7_float.cpp
 * @brief A size-specific optimizer-friendly implementatSion of smoothing
 * raw iTOF data with a 5x7 kernel using 32-bit float-point operations on
 * the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthDsp.h"
#include "FloatVectorPool.h"
#include "RtdMetadata.h"
#include <cassert>

void RawToDepthDsp::transposeRaw(const std::vector<float_t> &roi, std::vector<float_t> &roi_t, std::array<uint32_t,2> size) {
  assert(roi.size() == roi_t.size());

  auto inPitch = size[1]*3U;
  auto outPitch = size[0]*3U;
  auto columnStart = 0U;
  auto rowStart = 0U;
  
  for (auto rowIdx=0; rowIdx<size[0]; rowIdx++) {
    auto inIdx = columnStart;
    auto outIdx = rowStart;
    for (auto colIdx=0; colIdx<size[1]; colIdx++) {
      roi_t[outIdx]   = roi[inIdx++];
      roi_t[outIdx+1] = roi[inIdx++];
      roi_t[outIdx+2] = roi[inIdx++];
      outIdx += outPitch;
    }
    columnStart += inPitch;
    rowStart += 3;
  }
}

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
  
#define sum5(sum, roi, idx, k) { \
    assert((idx)+6 < (roi).size());  \
    assert((idx)-6 >= 0);          \
    (sum)  = (roi)[(idx)- 6]*(k)[0];	 \
    (sum) += (roi)[(idx)- 3]*(k)[1];	 \
    (sum) += (roi)[(idx)+ 0]*(k)[2];	 \
    (sum) += (roi)[(idx)+ 3]*(k)[3];	 \
    (sum) += (roi)[(idx)+ 6]*(k)[4];	 \
  }                              \


// unused. Placeholder.
void RawToDepthDsp::smoothRaw3x5(const std::vector<float_t> &roi, std::vector<float_t> &smoothedRoi, std::array<uint32_t,2> size) {}

constexpr uint32_t VKERNEL_IDX { 3 };
constexpr uint32_t VKERNEL_SIZE { 7 };
constexpr uint32_t HKERNEL_IDX { 2 };
constexpr uint32_t HKERNEL_SIZE { 5 };
void RawToDepthDsp::smoothRaw5x7(const std::vector<float_t> &roi, std::vector<float_t> &smoothedRoi, std::array<uint32_t,2> size) {

  assert(roi.size() == smoothedRoi.size());
  assert(size[0] >= VKERNEL_SIZE);
  assert(size[1] >= HKERNEL_SIZE);

  auto paddedSize = size;
  SCOPED_VEC_F(transposedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  SCOPED_VEC_F(vSmoothedTransposedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  SCOPED_VEC_F(vSmoothedRoi, paddedSize[0]*NUM_GPIXEL_PHASES*paddedSize[1]);
  
  transposeRaw(roi, transposedRoi, paddedSize);
  
  const auto &kern7 = _fKernels[VKERNEL_IDX];
  assert(kern7.size() == VKERNEL_SIZE);

  auto rowStart = 0U;
  auto rowPitch = paddedSize[0]*NUM_GPIXEL_PHASES; // transposed input pitch by num rows.

  // Copy out the first three rows, unfiltered, into the buffer
  for (auto colIdx=0; colIdx< paddedSize[1]; colIdx++)
  {
    auto rowIdx = rowStart;
    for (auto rowCount=0; rowCount<VKERNEL_SIZE/2; rowCount++)
    {
      assert(rowIdx+2 < vSmoothedTransposedRoi.size());
      assert(rowIdx+2 < transposedRoi.size());
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
    }
    rowStart += rowPitch;
  }

  rowStart = NUM_GPIXEL_PHASES*(paddedSize[0]-3U);
  // Copy out the last three rows, unfiltered, into the buffer
  for (auto colIdx=0; colIdx< paddedSize[1]; colIdx++)
  {
    auto rowIdx = rowStart;
    for (auto rowCount=0; rowCount<VKERNEL_SIZE/2; rowCount++)
    {
      assert(rowIdx+2 < vSmoothedTransposedRoi.size());
      assert(rowIdx+2 < transposedRoi.size());
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
      vSmoothedTransposedRoi[rowIdx] = transposedRoi[rowIdx]; rowIdx++;
    }
    rowStart += rowPitch;
  }

  rowStart = NUM_GPIXEL_PHASES*3; //skip left 3 elements with 7-element filter.
  const auto numRows = paddedSize[0]-6;  // 

  // filter a->b
  float_t sum;
  for (auto colIdx=0; colIdx<paddedSize[1]; colIdx++) {
    auto rowIdx = rowStart;
    for (auto rowCount=0; rowCount<numRows; rowCount++) {
      sum7(sum, transposedRoi, rowIdx, kern7);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

      sum7(sum, transposedRoi, rowIdx, kern7);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

      sum7(sum, transposedRoi, rowIdx, kern7);
      assert(rowIdx < vSmoothedTransposedRoi.size());
      vSmoothedTransposedRoi[rowIdx++] = sum;

    }
    rowStart += rowPitch;
  }

  transposeRaw(vSmoothedTransposedRoi, vSmoothedRoi, {paddedSize[1], paddedSize[0]});
  auto colPitch = paddedSize[1]*3;
  auto colStart = 0U;
  for (auto rowIdx=0; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0; colCount<2; colCount++) {
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
    }
    colStart += colPitch;
  }

  colStart = 3*(paddedSize[1] - 2);
  for (auto rowIdx=0; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0; colCount<2; colCount++) {
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
      smoothedRoi[colIdx] = vSmoothedRoi[colIdx]; colIdx++;
    }
    colStart += colPitch;
  }


  const auto &kern5 = _fKernels[HKERNEL_IDX];
  assert(kern5.size() == HKERNEL_SIZE);

  colStart = 2*NUM_GPIXEL_PHASES; // skip left 2 elements with 5-element filter.
  auto numCols  = paddedSize[1]-4;

  for (auto rowIdx=0; rowIdx<paddedSize[0]; rowIdx++) {
    auto colIdx = colStart;
    for (auto colCount=0; colCount<numCols; colCount++) {
      sum5(sum, vSmoothedRoi, colIdx, kern5);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

      sum5(sum, vSmoothedRoi, colIdx, kern5);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

      sum5(sum, vSmoothedRoi, colIdx, kern5);
      assert(colIdx < smoothedRoi.size());
      smoothedRoi[colIdx++] = sum;

    }
    colStart += colPitch;
  }

}
