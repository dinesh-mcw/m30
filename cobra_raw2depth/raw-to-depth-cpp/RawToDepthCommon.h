#pragma once

#include <memory>
#include <cstdint>
#include <math.h>
#include <vector>

class RawToDepthCommon {
public:
  static std::shared_ptr<std::vector<uint16_t>> getRange(const std::vector<float_t> &_fRanges,
                                                          const std::vector<float_t> &_fMinMaxMask,
                                                          std::shared_ptr<std::vector<uint16_t>> _pixelMask,
                                                          const std::vector<float> &_fSnr,
                                                          std::array<uint16_t,2> _sensorFovStart,
                                                          std::array<uint16_t,2> _sensorFovStep,
                                                          uint16_t pixelMaskStride,
                                                          std::array<uint32_t,2> _size,
                                                          bool _disableRangeMasking,
                                                          float_t _snrThresh,
                                                          float_t rangeOffsetTemperature, 
                                                          float_t rangeLimit,
                                                          float_t maxUnambiguousRange);

  static std::shared_ptr<std::vector<uint16_t>> getSnr(const std::vector<float_t> &_fSnr);
  static std::shared_ptr<std::vector<uint16_t>> getBackground(const std::vector<float_t> &_fBackground);
  static std::shared_ptr<std::vector<uint16_t>> getSignal(const std::vector<float_t> &_fSignals);

};