/**
 * @file RawToDepthFactory_float.h
 * @brief A factory class to create specializations for RawToDepth for
 * different processing scenarios.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#pragma once

#include <memory>
#include "RawToDepth.h"

class RawToDepthFactory 
{
public:
  static void create(std::vector<std::unique_ptr<RawToDepth>> &rtds, RtdMetadata &mdat, uint32_t fovIdx=0, uint32_t headerNum=0);
};
