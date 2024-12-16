/**
 * @file binning_float.h
 * @brief Performs binning on 2D raw data using 32-bit floating-point
 * operations on the CPU.
 * 
 * Also include 1xN binning for use in Stripe Mode.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#pragma once

#include "RtdVec.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <array>

class Binning {
 public:
  // The output buffer binnedFrame contains an exact copy of the input data.
  static void bin1x1(const RtdVec &frame, RtdVec &binnedFrame, std::array<uint32_t,2> roiSize);
  // The output buffer binnedFrame has been binned 2x2, with the height of the buffer div of the original height and the binning.
  static void bin2x2(const RtdVec &frame, RtdVec &binnedFrame, std::array<uint32_t,2> roiSize, int32_t shift=0);
  // The output buffer binnedFrame has been binned 4x4 by calling the 2x2 binning routine twice.
  static void bin4x4(const RtdVec &frame, RtdVec &binnedFrame, std::array<uint32_t,2> roiSize);

  // The common input that calls 1x1, 2x2, or 4x4 binning respectively. Only these binning rates are supported.
  static void binMxN(const RtdVec &frame, RtdVec &binnedFrame, std::array<uint32_t,2> roiSize, std::array<uint32_t,2> binning);
  
  // The common input that calls th 1x1, 1x2, or 1x4 binning routines as needed. Only these binning rates are supported.
  static void bin1xN(const RtdVec &rawRoi, RtdVec &binnedRawRoi, uint32_t roiWidth, uint32_t binX);
  // The output buffer binnedRawRoi is identical to the input rawRoi buffer.
  static void bin1x1(const RtdVec &rawRoi, RtdVec &binnedRawRoi, uint32_t roiWidth);
  // The output buffer has been binned by 2, with the size of the buffer being the integer division of the input buffer width and 2.
  static void bin1x2(const RtdVec &rawRoi, RtdVec &binnedRawRoi, uint32_t roiWidth);
  // The output buffer binnedRawRoi has been binned by 4, with the width of the buffer being the integer division of the input buffer size and 4.
  static void bin1x4(const RtdVec &rawRoi, RtdVec &binnedRawRoi, uint32_t roiWidth);

};
