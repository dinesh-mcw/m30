#include "RawToDepthCommon.h"
#include "RtdMetadata.h"
#include <cassert>

/**
 * @brief Converts the input range values into 16-bit 1024.0 meters/step
 * for transmission on the network.
 * 
 * The routine also performs masking operations. A pixel is marked as invalid (precisely 0m range) if :
 * 1. It falls outside of the _pixelMask.
 * 2. It is masked by the min-max filter via _fMinMaxMask
 * 3. The pixel has an SNR that falls below the value in _snrThresh as acquired from metadata.
 * 4. The range is farther than the range limit as defined by RANGE_LIMIT_FRACTION as defined in
 * M20Metadata.h
 * 5. Any range computed as less than zero (rather than aliasing down from the max unambiguous range).
 * 
 * Note that the metadata value M20Metadata::getDisableRangeMasking()==true disables all range masking
 * and passes the range data as computed by the RawToDepth algorithms.
 * 
 * @param _fRanges The FOV buffer containing the ranges as computed in processWholeFrame()
 * @param _fMinMaxMask The mask created by RawToDepthDsp::minMaxRecursive() during processWholeFrame()
 * @param _pixelMask The data structure created at calibration time defining the valid region of the sensor
 * that is illuminate by the laser.
 * @param _fSnr The FOV buffer containing the sum of the SNR from both frequencies as computed in processWholeFrame()
 * @param _sensorFovStart The coordinates with which to index the _pixelMask
 * @param _sensorFovStep 
 * @param pixelMaskStride 
 * @param _size The size of the FOV buffers in two dimensions
 * @param _disableRangeMasking Turns off all masking in this routine
 * @param _snrThresh Any pixel with a computed SNR below this value is marked as invalid.
 * @param rangeOffsetTemperature A fixed offset as computed from temperature values measured in the sensor head.
 * @param rangeLimit The range above which any pixel is declared as invalid
 * @return std::shared_ptr<std::vector<uint16_t>> The range FOV formatted as 16-bit values for transmission over the network.
 */
std::shared_ptr<std::vector<uint16_t>> RawToDepthCommon::getRange(const std::vector<float_t> &_fRanges,
                                                                    const std::vector<float_t> &_fMinMaxMask,
                                                                    std::shared_ptr<std::vector<uint16_t>> _pixelMask,
                                                                    const std::vector<float> &_fSnr,
                                                                    std::array<uint16_t,2> _sensorFovStart,
                                                                    std::array<uint16_t,2> _sensorFovStep,
                                                                    uint16_t pixelMaskStride,
                                                                    std::array<uint32_t,2> _size,
                                                                    bool _disableRangeMasking,
                                                                    float _snrThresh,
                                                                    float_t rangeOffsetTemperature,
                                                                    float_t rangeLimit,
                                                                    float_t maxUnambiguousRange)
{
  auto ranges = std::make_shared<std::vector<uint16_t>>(_fRanges.size());
  float_t iSnrThresh = _snrThresh;

  uint16_t pixelMaskStartY = _sensorFovStart[0];
  uint16_t pixelMaskStartX = _sensorFovStart[1];
  uint16_t pixelMaskStepY  = _sensorFovStep[0];
  uint16_t pixelMaskStepX  = _sensorFovStep[1];

  for (uint32_t idx = 0; idx < _fRanges.size(); idx++)
  {
    uint32_t rangeX = idx%_size[1];
    uint32_t rangeY = idx/_size[1];
    uint32_t pixelMaskX = pixelMaskStartX + rangeX*pixelMaskStepX;
    uint32_t pixelMaskY = pixelMaskStartY + rangeY*pixelMaskStepY;
    uint32_t pixelMaskIdx = pixelMaskX + pixelMaskStride*pixelMaskY;

    assert(idx < _fMinMaxMask.size());
    const float_t minMaxThresh = 0.5F;
    bool minMaxMask = _fMinMaxMask[idx] > minMaxThresh;

    bool pixelMask = false;
    if (pixelMaskIdx < _pixelMask->size())
    {
      pixelMask = _pixelMask->at(pixelMaskIdx) == 0;
    }
    
    assert(idx < _fSnr.size());
    float_t snr = (_fSnr[idx]);

    assert(idx < _fRanges.size());
    auto iRange = _fRanges[idx];
        
    if (!_disableRangeMasking) {
      if (minMaxMask ||
	        snr < 2*iSnrThresh ||
	        pixelMask || 
          iRange > rangeLimit
	        ) 
      {
	      iRange = 0;
      }
    }

    assert(idx < ranges->size());
    (*ranges)[idx] = (uint16_t) roundf(RANGE_NETWORK_SCALE*iRange);
  }

  return ranges;
}


/**
 * @brief Returns a 16-bit FOV buffer containing the average signal from both frequencies.
 * 
 * @param _fSignals The floating point array containing the sum of signals from both frequencies.
 * @return std::shared_ptr<std::vector<uint16_t>> The output FOV buffer containing the signal as 16-bit values
 * to be passed to the network.
 */
std::shared_ptr<std::vector<uint16_t>> RawToDepthCommon::getSignal(const std::vector<float_t> &_fSignals)
{
  auto signal = std::make_shared<std::vector<uint16_t>>(_fSignals.size());
  for (unsigned int idx = 0; idx < _fSignals.size(); idx++)
  {
    const auto avgSignal = roundf(0.5F*_fSignals[idx]); // _fSignals contains the sum of both frequencies.

    assert(idx < (*signal).size());
    const float_t sigClip = 65535.0F;
    (*signal)[idx] = avgSignal > sigClip ? uint16_t(sigClip) : uint16_t(avgSignal);
  }
  return signal;
}

/**
 * @brief Returns a 16-bit FOV buffer containing the average background from both frequencies.
 * Note that one divide-by-two operation was omitted from the intermediate processing, resulting in 
 * this buffer containing the average background from the two frequencies.
 * 
 * @param _fBackground The floating point array containing the average background
 * @return std::shared_ptr<std::vector<uint16_t>> An FOV buffer containing 16-bit values of background
 */
std::shared_ptr<std::vector<uint16_t>> RawToDepthCommon::getBackground(const std::vector<float_t> &_fBackground)
{
  assert(!_fBackground.empty());
  auto background = std::make_shared<std::vector<uint16_t>>(_fBackground.size());
  for (unsigned int idx = 0; idx < _fBackground.size(); idx++)
  {
    auto avgBg = _fBackground[idx]; // 2C has been excluded from the computation. _fBackground contains sum of two frequencies.

    assert(idx < (*background).size());
    const float_t bgClip = 65535.0F;
    (*background)[idx] = avgBg > bgClip ? uint16_t(bgClip) : uint16_t(roundf(avgBg));
  }

  return background;
}

/**
 * @brief Returns a 16-bit FOV buffer containing the average SNR from both frequencies for each pixel.
 * 
 * @param _fSnr The floating point array containing the sum of SNR from both frequencies
 * @return std::shared_ptr<std::vector<uint16_t>> The FOV buffer containing the average SNR in 16-bit format.
 */
std::shared_ptr<std::vector<uint16_t>> RawToDepthCommon::getSnr(const std::vector<float_t> &_fSnr)
{

  uint32_t numSnrs = _fSnr.size();
  
  auto snrs = std::make_shared<std::vector<uint16_t>>(numSnrs);
  for (uint32_t idx = 0; idx < numSnrs; idx++) 
  {
    constexpr float_t factor {0.5F};
    (*snrs)[idx] = (uint16_t)roundf(factor * _fSnr[idx]); // Note: snr is summed in the algorithm.
  }
  return snrs;
}
