/**
 * @file GPixel.h
 * @brief Contains some convenience getters and constants related to the 
 * GPixel iTOF sensor.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include "LumoLogger.h"
#include <cassert>

class GPixel {
 public: 
  static const std::vector<float> IDX_TO_FRQ_LUT;
  static const std::vector<std::vector<float>> IDX_TO_GCF_LUT;
  static uint32_t getGcf(uint32_t f0ModulationIndex, uint32_t f1ModulationIndex);
};
