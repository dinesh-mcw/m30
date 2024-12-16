/**
 * @file calculatePhase_float.cpp
 * @brief Performs iTOF phase computations on CPU using 32-bit floating-point operations
 * on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "RawToDepthDsp.h"

// Assumes signal, snr, background are initialize to zero on first call.
void RawToDepthDsp::calculatePhase(const std::vector<float_t> &rawRoi,
				   std::vector<float_t> &phaseRoi,
				   std::vector<float_t> &signalRoi,
				   std::vector<float_t> &snrRoi,
				   std::vector<float_t> &backgroundRoi,
				   float_t numberOfSummedValues) 
{
  for (uint32_t idx = 0; idx < phaseRoi.size(); idx++)
  {
    int aIdx = int(3 * idx);
    
    assert(aIdx+2 < rawRoi.size());
    auto rawA = rawRoi[aIdx];
    auto rawB = rawRoi[aIdx + 1];
    auto rawC = rawRoi[aIdx + 2];

    const float_t oneThird=1.0F/3.0F;
    const float_t twoThirds=2.0F/3.0F;
    
    float_t frac = 0;

    if (rawA <= rawB && rawA <= rawC)
    {
      auto tmp = rawC;
      rawC = rawA;
      rawA = rawB;
      rawB = tmp;
      frac = oneThird;
    }

    else if (rawB <= rawC && rawB < rawA)
    {
      auto tmp = rawA;
      rawA = rawC;
      rawC = rawB;
      rawB = tmp;
      frac = twoThirds;
    }
    
    float_t phase;
    float_t snr;
    float_t signal = rawA + rawB - 2 * rawC; // signed operation.
    if (signal <= 0)
    {
      signal = 0;
      phase = 0;
      snr = 0;
      rawC = 0;
    }
    else
    {

      float_t part1 = rawB-rawC;
      float_t part2 = oneThird * (part1 / signal) + frac;

      phase = part2;

      const float_t clip = 1.0F/65535.0F;
      if (rawC< clip) 
      {
        rawC = clip; // overflow prevention.
      }

      const float_t twoc = 2.0F * rawC;
      snr = signal / sqrtf(twoc);      
    }

    assert(idx < phaseRoi.size());
    assert(idx < signalRoi.size());
    assert(idx < snrRoi.size());
    assert(idx < backgroundRoi.size());
    phaseRoi[idx]      = phase;
    signalRoi[idx]     += signal / numberOfSummedValues;
    snrRoi[idx]        += snr;
    backgroundRoi[idx] += rawC / numberOfSummedValues;
  }
}
