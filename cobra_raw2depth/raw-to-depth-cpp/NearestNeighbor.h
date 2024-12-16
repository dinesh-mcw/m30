/**
 * @file NearestNeighbor.h
 * @brief Implements the nearest-neighbor filter on range values using
 * 32-bit floating-point operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <array>


class NearestNeighbor {

 private:
  const static std::vector<uint16_t> _lutWindowSize;
  const static std::vector<float_t>  _flutRangeToleranceFrac;
  const static std::vector<uint16_t> _lutNeighborCountTolerance;
  
 public:
  static void removeOutliers(std::vector<float_t> &ffilteredRanges, uint16_t filterLevel, std::array<uint32_t,2> &size);
  
private:

};
