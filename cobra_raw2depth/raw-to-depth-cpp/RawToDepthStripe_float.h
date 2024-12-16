/**
 * @file RawToDepthStripe_float.h
 * @brief The derived class that implements the Stripe Mode algorithms.
 * Work in progress.
 * 
 * @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.
 * 
 */
#include "RawToDepth.h"

constexpr float_t SNR_WEIGHTED_WINDOW_DEFAULT_NUMBER_OF_SUMS { 4.0F };

class RawToDepthStripe_float : public RawToDepth
{
private:
    std::vector<float_t> _signal;
    std::vector<float_t> _background;
    std::vector<float_t> _snr;
    std::vector<float_t> _ranges;
    std::vector<float_t> _oneDMinMaxMask;

	std::vector<float_t> _snrWeights;
	float_t              _snrWeightsNumberOfSums { SNR_WEIGHTED_WINDOW_DEFAULT_NUMBER_OF_SUMS };

    uint16_t _roiStartRow=0;
    uint32_t _binnedRoiWidth=IMAGE_WIDTH;
    
protected:
    bool saveTimestamp(RtdMetadata &mdat) override;

public:
    RawToDepthStripe_float(uint32_t fovIdx, uint32_t headerNum);

    // Performs DSP on the input data to generate the point cloud.
    void processRoi(const uint16_t* roi, uint32_t numBytes) override;
    // Simply formats the data for transmission, since per-roi and whole frame processing are the same in Stripe Mode.
    void processWholeFrame(std::function<void (std::shared_ptr<FovSegment>)> setFovSegment) override;
    // Called once per ROI to resize buffers if necessary.
    void reset(const uint16_t *mdPtr, uint32_t mdBytes) override;

private:
    // Called once per ROI to resize buffers if necessary.
    void realloc(const uint16_t *mdPtr, uint32_t mdBytes);
    std::pair<const std::vector<float_t>&, float_t> windowFactory(RtdMetadata &mdat, const std::vector<float_t> &rawRoi0, const std::vector<float_t> &rawRoi1, uint32_t rowOffset=0);

};