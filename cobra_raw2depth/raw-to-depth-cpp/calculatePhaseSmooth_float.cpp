/**
 * @file calculatePhaseSmooth_float.cpp
 * @brief Computes phase from smoothed raw data and performs phase correction using
 * 32-bit floating point operations on the CPU.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "RawToDepthDsp.h"
#include <cassert>

#define MAX_PHASE_ERROR 0.5F

void RawToDepthDsp::calculatePhaseSmooth(std::vector<float_t> &frameSmoothed,
					 std::vector<float_t> &phaseSmoothedFrame,
					 std::vector<float_t> &phaseFrame,
					 std::vector<float_t> &correctedPhaseFrame,
           uint32_t frqIdx) {
  assert(3*phaseSmoothedFrame.size() == frameSmoothed.size());
  assert(3*phaseFrame.size() == frameSmoothed.size());
  assert(3*correctedPhaseFrame.size() == frameSmoothed.size());

  for (uint32_t idx = 0; idx < phaseSmoothedFrame.size(); idx++) {
    int aIdx = int(3 * idx);

    assert(aIdx+2 < frameSmoothed.size());
    auto rawA = frameSmoothed[aIdx];
    auto rawB = frameSmoothed[aIdx + 1];
    auto rawC = frameSmoothed[aIdx + 2];
    
    const float_t oneThird =1.0F/3.0F;
    const float_t twoThirds=2.0F/3.0F;;
    
    float_t frac = 0.0F;

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

    float_t iPhaseSmoothed = 0;
    float_t iPhase = 0;
    auto signal = rawA + rawB - 2 * rawC;
    if (signal > 0 )
    {
      float_t part1 = rawB-rawC; 
      iPhaseSmoothed = oneThird*(part1 / signal) + frac;
      iPhase = phaseFrame[idx];
    }

    assert(idx < phaseSmoothedFrame.size());
    phaseSmoothedFrame[idx] = iPhaseSmoothed;
      
    // This last step is for phase correction
    correctedPhaseFrame[idx] = iPhase;

    float_t phaseErr = iPhase - iPhaseSmoothed;
    if (phaseErr > MAX_PHASE_ERROR) 
    {
      correctedPhaseFrame[idx] -= 1.0F;
    }
    if (phaseErr < -MAX_PHASE_ERROR) 
    {
      correctedPhaseFrame[idx] += 1.0F;
    }
  }
}

