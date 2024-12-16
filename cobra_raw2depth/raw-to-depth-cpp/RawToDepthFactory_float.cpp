/**
 * @file RawToDepthFactory_float.cpp
 * @brief A factory class to create specializations for RawToDepth for
 * different processing scenarios.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "RawToDepthFactory.h"
#include "RawToDepthV2_float.h"
#include "RawToDepthStripe_float.h"

void RawToDepthFactory::create(std::vector<std::unique_ptr<RawToDepth>> &rtds, RtdMetadata &mdat, uint32_t fovIdx, uint32_t headerNum) 
{
   assert(fovIdx < rtds.size());

   if (mdat.getStripeModeEnabled(fovIdx))
   {
      if (nullptr == rtds[fovIdx] || dynamic_cast<RawToDepthStripe_float*>((rtds.at(fovIdx)).get()) == nullptr)
      {
         if (nullptr != rtds[fovIdx])
         {
            rtds[fovIdx]->shutdown();
         }
         rtds[fovIdx] = std::make_unique<RawToDepthStripe_float>(fovIdx, headerNum);
      }
   }
   else if (mdat.getGridModeEnabled(fovIdx))
   {
      if (nullptr == rtds[fovIdx] || dynamic_cast<RawToDepthV2_float*>((rtds.at(fovIdx)).get()) == nullptr)
      {
         rtds[fovIdx] = std::make_unique<RawToDepthV2_float>(fovIdx, headerNum);
      }
   }
}
