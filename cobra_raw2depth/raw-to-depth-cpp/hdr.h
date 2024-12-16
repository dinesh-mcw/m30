/**
 * @file hdr.h
 * @brief High Dynamic Range algorithms
 * 
 * As of Oct, 2023, HDR is non-functional
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once
#include "RtdMetadata.h"
#include "RawToDepthDsp.h"
#include <cstdint>
#include <vector>

#define hdr_copy(src, len, dst) std::copy((src), (src)+(len), (dst).begin())

class hdr {
 private:

  // These ROIs are stored without the header attached.
  int _previousRoiIdx = 0;
  int _nextRoiIdx = 1;
  std::vector<std::vector<float_t>> _rois;
  std::vector<
    std::vector<uint16_t>> _md;
  bool                     _previousRoiWasCorrected = false;
  bool                     _skipThis = false;

 public:

  void submit(const uint16_t *roi, uint32_t roiShorts, uint32_t fovIdx, bool startup, uint32_t shiftr);
  bool skip() const { return _skipThis; }
  std::vector<float_t> &getRoi() { return _rois[_nextRoiIdx]; }
  std::vector<float_t> readoutRoi(); // Used for testing.
  RtdMetadata getMetadata() { return RtdMetadata(_md[_nextRoiIdx]); } //elision, metadata copied into new object.
  std::vector<uint16_t> &getMetadataVector() { return _md[_nextRoiIdx]; }

private:
  void realloc(uint32_t roiShorts); // number of shorts in an roi with the header.
  bool reallocBuffers(uint32_t roiShorts);
  void hdrSum(uint16_t saturationLevel, const uint16_t *roi, uint32_t roiShorts, uint32_t shiftr, uint16_t mask);
  static void copyBuffer(const uint16_t *src, std::vector<float_t> &dst, uint32_t numElements, uint32_t shiftr, uint16_t mask);
  
};
