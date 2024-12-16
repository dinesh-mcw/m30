/**
 * @file RawToDepthDsp.h
 * @brief The algorithms for performing digital signal processing to
 * generate point clouds from raw iTOF data.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once
#include "FloatVectorPool.h"
#include "RtdVec.h"
#include <cstdint>
#include <vector>
#include <cmath>
#include <map>
#include <tuple>

#define SNR_SCALING_FACTOR float_t(8.0F)
#define RAW_SCALING_FACTOR (8.0F) // shift all input data to fill 15 bits of dynamic range.
const uint16_t DEFAULT_RAW_MASK = 0xfff0;

class RawToDepthDsp
{
private:
	static inline float_t median(std::vector<float_t> &points);
	static inline bool outOfRange(const std::vector<float_t> &frame, uint32_t idx, const std::vector<int32_t> &offsets, float_t thresh);

public:
	static const std::vector<std::vector<float_t>> _fKernels;
	static const std::vector<std::vector<float_t>> _fKernelsNoCenter;
	static const std::vector<float_t> _gaussian6;
	static const float_t              _gaussian6NumberOfSums;
	static const std::vector<float_t> _gaussian8;
	static const float_t              _gaussian8NumberOfSums;
	static const std::vector<float_t> _rect6;
	static const float_t              _rect6NumberOfSums;
	static const std::vector<float_t> _rect8;
	static const float_t              _rect8NumberOfSums;

	static void computeWholeFrameRange(RtdVec &fSmoothedPhases0,
																		 RtdVec &fSmoothedPhases1,
																		 RtdVec &fCorrectedPhases0,
																		 RtdVec &fCorrectedPhases1,
																		 RtdVec &fRanges,
																		 std::array<float_t, 2> freqs, std::vector<float_t> fsInt,
																		 float_t cMps,
																		 RtdVec &mFrame); 

	static void calculatePhaseSmooth(RtdVec &frameSmoothed,
									 RtdVec &phaseSmoothedFrame,
									 RtdVec &phaseFrame,
									 RtdVec &correctedPhaseFrame,
									 uint32_t frqIdx);

	static void smoothSummedData(const RtdVec &roiSummed, RtdVec &roiSmoothed, std::array<uint32_t,2> size,
								 uint32_t _rowKernelIdx, uint32_t _columnKernelIdx, bool doAcceleratedVersion=true);
	static void smoothRaw(const std::vector<float_t> &roiSummed, std::vector<float_t> &roiSmoothed, std::array<uint32_t,2> size,
                   uint32_t _rowKernelIdx, uint32_t _columnKernelIdx);
	static void smoothRaw3x5(const RtdVec &roi, RtdVec &smoothedRoi, std::array<uint32_t,2> size);
	static void smoothRaw5x7(const RtdVec &roi, RtdVec &smoothedRoi, std::array<uint32_t,2> size);
	static void smoothRaw7x15(const RtdVec &roi, RtdVec &smoothedRoi, std::array<uint32_t,2> size);

	static void calculatePhase(const RtdVec &rawRoi,
														 RtdVec &phaseRoi,
														 RtdVec &signalRoi,
														 RtdVec &snrRoi,
														 RtdVec &backgroundRoi,
														 float_t numberOfSummedValues); //num Binning
	
	static void computeSnrSquaredWeights(const std::vector<float_t> &rawRoi0, const std::vector<float_t> &rawRoi1, 
	                                           std::vector<float_t> &snrWeights, float_t &snrWeightsNumberOfSums, 
																						 uint32_t roiHeight, uint32_t roiWidth, uint32_t rowOffset=0);
	static inline float computeSnrSquared(const std::vector<float_t> &rawRoi, uint32_t idx);
	static void snrVoteV2(const std::vector<float_t> &roi0, const std::vector<float_t> &roi1, std::vector<std::vector<float_t>> &rawFov, std::vector<float_t> &snrSquaredFov, uint32_t fovOffset);
	static void transposeRaw(const std::vector<float_t> &roi, std::vector<float_t> &roi_t, std::array<uint32_t,2> size);
	static void sh2f(const uint16_t *src, std::vector<float_t> &dst, uint32_t numElements, uint32_t shiftr = 0, uint16_t rawMask = DEFAULT_RAW_MASK);
	static void tapRotation(const std::vector<float_t> &roiVector, std::vector<float_t> &frame, uint32_t freqIdx, std::vector<uint32_t> roiSize, uint32_t numGpixelPhases, bool doTapRotation);

	static std::vector<int> getMedianOffsets(std::array<uint32_t,2> frameSize, std::vector<uint32_t> kernelIndices);
	static void medianFilterPlus(RtdVec &inFrame, RtdVec &outFrame, std::vector<uint32_t> kernelIndices, std::array<uint32_t,2> frameSize, bool performGhostMedian);
	static void median1d(const RtdVec &range, RtdVec &medianFilteredRange, uint32_t binning);
	static inline float_t binMedian(std::vector<float_t> &points);
	static inline bool outOfRangeIntra(const std::vector<float_t> &frame, const std::vector<float_t> &minMaxMask, uint32_t idx, const std::vector<int32_t> &offsets, float_t thresh);
	static void minMaxIntra(const std::vector<float_t> &frame, std::vector<float_t> &minMaxMask, std::vector<uint32_t> filterSize, std::array<uint32_t,2> frameSize, float_t minMaxThresh);

	static void minMaxRecursive(const std::vector<float_t> &frame, std::vector<float_t> &minMaxMask, std::vector<uint32_t> filterSize, std::array<uint32_t,2> frameSize, float_t minMaxThresh);
	static void minMax(const RtdVec &mFrame, RtdVec &minMaxMask, std::vector<uint32_t> filterSize, std::vector<uint32_t> frameSize, float_t minMaxThresh);
	static void fillMissingRows(const std::vector<float_t> &frame, std::vector<float_t> &outFrame, std::array<uint32_t,2> frameSize, std::vector<bool> &activeRows);
	
	// Reduce the height of the ROI to 1 row by summing along the columns. 
	static void collapseRawRoi(const std::vector<float_t> & rawRoi, std::vector<float_t> &collapsedRoi, const std::vector<float_t> &weights, 
	            const std::array<uint32_t,2> &binning, std::array<uint32_t, 2> roiSize, uint32_t rowOffset=0);
	static void minMax1d(const std::vector<float_t> &rawRoi0, const std::vector<float_t> &rawRoi1, std::vector<float_t> &mask,
	                     std::array<uint32_t, 2> rawRoiSize, uint32_t binning);
};
