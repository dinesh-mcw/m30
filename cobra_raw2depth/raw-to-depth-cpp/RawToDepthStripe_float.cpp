/**
 * @file RawToDepthStripe_float.cpp
 * @brief Stripe Mode Processing:
 * Performs per-ROI processing for stripe mode.
 * Work in progress.
 *
 * @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.
 *
 */
#include "RawToDepthStripe_float.h"
#include "RawToDepthCommon.h"
#include "LumoUtil.h"

RawToDepthStripe_float::RawToDepthStripe_float(uint32_t fovIdx, uint32_t headerNum) : 
  RawToDepth(fovIdx, headerNum)
{
}

void RawToDepthStripe_float::reset(const uint16_t *mdPtr, uint32_t mdBytes)
{
  RawToDepth::reset(mdPtr, mdBytes);
  auto mdat = RtdMetadata(mdPtr, mdBytes);

  realloc(mdPtr, mdBytes);
}

void RawToDepthStripe_float::realloc(const uint16_t *mdPtr, uint32_t mdBytes)
{
  bool changed=false;
  RtdMetadata mdat(mdPtr, mdBytes);
  MAKE_VECTOR(_signal, float32_t, mdat.getRoiNumColumns() / _binning[1]); //binned size
  MAKE_VECTOR(_snr, float32_t, mdat.getRoiNumColumns() / _binning[1]);
  MAKE_VECTOR(_background, float32_t, mdat.getRoiNumColumns() / _binning[1]);
  MAKE_VECTOR(_ranges, float32_t, mdat.getRoiNumColumns() / _binning[1]);
  MAKE_VECTOR(_oneDMinMaxMask, float32_t, mdat.getRoiNumColumns() / _binning[1]);
  MAKE_VECTOR(_snrWeights, float32_t, NUM_GPIXEL_PHASES*mdat.getRoiNumRows()*RtdMetadata::getRoiNumColumns());
}

bool RawToDepthStripe_float::saveTimestamp(RtdMetadata &mdat)
{
  if (!RawToDepth::saveTimestamp(mdat))
  {
    return false;
  }
  RETURN_CONDITION(1 != _expectedNumRois, "Skipping ROI. Number of ROIs != 1 in stripe mode. Error in metadata.");
  return true;
}

std::pair<const std::vector<float_t>&, float_t> RawToDepthStripe_float::windowFactory(RtdMetadata &mdat, 
                                                                                      const std::vector<float_t> &rawRoi0, 
                                                                                      const std::vector<float_t> &rawRoi1, 
                                                                                      uint32_t rowOffset)
{
  if (mdat.getStripeModeRectSum(_fovIdx) && (RawToDepthDsp::_rect6.size() == (size_t)mdat.getRoiNumRows()) )
  {
    return std::make_pair(ref(RawToDepthDsp::_rect6), RawToDepthDsp::_rect6NumberOfSums);
  }
  if (mdat.getStripeModeRectSum(_fovIdx) && (RawToDepthDsp::_rect8.size() == (size_t)mdat.getRoiNumRows()))
  {
    return std::make_pair(ref(RawToDepthDsp::_rect8), RawToDepthDsp::_rect8NumberOfSums);
  }
  if (mdat.getStripeModeSnrWeightedSum(_fovIdx))
  {
    uint32_t roiHeight = mdat.getRoiNumRows();
    uint32_t roiWidth = RtdMetadata::getRoiNumColumns();
    RawToDepthDsp::computeSnrSquaredWeights( rawRoi0,  rawRoi1, _snrWeights, _snrWeightsNumberOfSums,
                                             roiHeight, roiWidth, rowOffset);
    return std::make_pair(ref(_snrWeights), _snrWeightsNumberOfSums);
  }
  if (RawToDepthDsp::_gaussian6.size() == (size_t)mdat.getRoiNumRows())
  {
    return std::make_pair(ref(RawToDepthDsp::_gaussian6), RawToDepthDsp::_gaussian6NumberOfSums);
  }
  assert(RawToDepthDsp::_gaussian8.size() == (size_t)mdat.getRoiNumRows());
  return std::make_pair(ref(RawToDepthDsp::_gaussian8), RawToDepthDsp::_gaussian8NumberOfSums);
}

void RawToDepthStripe_float::processRoi(const uint16_t *roi, uint32_t numBytes)
{
  auto localTimer = LumoTimers::ScopedTimer(*_timers, "RawToDepthStripe_float::processRoi()", STRIPE_TIMERS_UPDATE_EVERY);
  if (nullptr == roi || numBytes == 0)
  {
      return;
  }  

  if (!validateMetadata(roi, numBytes))
  {
      return;
  }  

  _hdr.submit(roi, numBytes/sizeof(uint16_t), _fovIdx, !_veryFirstRoiReceived, INPUT_RAW_SHIFT);
  auto mdat = _hdr.getMetadata();
  auto rawRoi = _hdr.getRoi();

  if (!_veryFirstRoiReceived && !_hdr.skip() && _fovIdx == 0)
  {
      mdat.logMetadata();
  }
  _veryFirstRoiReceived = true;  

  // Call unconditionally. In stripe mode, every ROI is the first (and last) roi in an FOV.
  reset(roi, numBytes);  
  if (_hdr.skip())
  {
    return; // Skip this ROI because HDR needs to hold it to see if there's a retake on the next ROI.
  }
  if (!saveTimestamp(mdat)) // Perform a number of validation checks on the input ROI
  {
    return;
  }

  auto binX = _binning[1];
  _roiStartRow = mdat.getRoiStartRow();
  _binnedRoiWidth = RtdMetadata::getRoiNumColumns() / binX;

  uint32_t numRawRoiElements = NUM_GPIXEL_PHASES*mdat.getNumModulationFrequencies()*mdat.getRoiNumRows()*RtdMetadata::getRoiNumColumns()*mdat.getNumPermutations();

  SCOPED_VEC_F(rawRoi0Rotated, NUM_GPIXEL_PHASES*mdat.getRoiNumRows()*RtdMetadata::getRoiNumColumns());
  SCOPED_VEC_F(rawRoi1Rotated, NUM_GPIXEL_PHASES*mdat.getRoiNumRows()*RtdMetadata::getRoiNumColumns());

  RawToDepthDsp::tapRotation(rawRoi, rawRoi0Rotated, 0, {mdat.getRoiNumRows(), RtdMetadata::getRoiNumColumns()}, NUM_GPIXEL_PHASES, mdat.getDoTapAccumulation());
  RawToDepthDsp::tapRotation(rawRoi, rawRoi1Rotated, 1, {mdat.getRoiNumRows(), RtdMetadata::getRoiNumColumns()}, NUM_GPIXEL_PHASES, mdat.getDoTapAccumulation());

  uint32_t numRawRoiColumns = RtdMetadata::getRoiNumColumns() * NUM_GPIXEL_PHASES;
  uint32_t numBinnedRawRoiColumns = NUM_GPIXEL_PHASES * (RtdMetadata::getRoiNumColumns() / binX);
  SCOPED_VEC_F(roi0Collapsed, numBinnedRawRoiColumns);
  SCOPED_VEC_F(roi1Collapsed, numBinnedRawRoiColumns);

  constexpr uint32_t rowOffset {0};
  auto [window, windowNumberOfSums] = windowFactory(mdat, rawRoi0Rotated, rawRoi1Rotated, rowOffset); // Note: "structured binding"

  RawToDepthDsp::collapseRawRoi(rawRoi0Rotated, roi0Collapsed, window, _binning, {mdat.getRoiNumRows(), RtdMetadata::getRoiNumColumns()}, rowOffset);
  RawToDepthDsp::collapseRawRoi(rawRoi1Rotated, roi1Collapsed, window, _binning, {mdat.getRoiNumRows(), RtdMetadata::getRoiNumColumns()}, rowOffset);

  SCOPED_VEC_F(phaseRoi0, _binnedRoiWidth);
  SCOPED_VEC_F(phaseRoi1, _binnedRoiWidth);

  // Initialize these to zero, since both frequencies are summed in the calculatePhase routine.
  std::fill(_signal.begin(), _signal.end(), 0.0F);
  std::fill(_snr.begin(), _snr.end(), 0.0F);
  std::fill(_background.begin(), _background.end(), 0.0F);
  RawToDepthDsp::calculatePhase(roi0Collapsed, phaseRoi0, _signal, _snr, _background, windowNumberOfSums*(float_t)_binning[1]);
  RawToDepthDsp::calculatePhase(roi1Collapsed, phaseRoi1, _signal, _snr, _background, windowNumberOfSums*(float_t)_binning[1]);

  SCOPED_VEC_F(mFrame, _binnedRoiWidth);
  SCOPED_VEC_F(rangeStripe, _binnedRoiWidth);
  RawToDepthDsp::computeWholeFrameRange(phaseRoi0, phaseRoi1, phaseRoi0, phaseRoi1, rangeStripe, _fs, _fsInt, C_MPS, mFrame);
  
  RawToDepthDsp::minMax1d(rawRoi0Rotated, rawRoi1Rotated, _oneDMinMaxMask, 
                          {mdat.getRoiNumRows(), NUM_GPIXEL_PHASES*RtdMetadata::getRoiNumColumns()}, _binning[1]);


  // Apply temperature correction, clip and modulo the range values.
  const auto maxUnambiguousRange = (float_t)getMaxUnambiguousRange();
  const auto rangeOffsetTemperature = _temperatureCalibration.getRangeOffsetTemperature();
  std::for_each(rangeStripe.begin(), rangeStripe.end(),
    [maxUnambiguousRange, rangeOffsetTemperature] (float_t &range)
    {
      range -= rangeOffsetTemperature;
      if (range<0.0F) { range = 0.0F; }
      range = fmod(range, maxUnambiguousRange);
    });

#if 0 // Check input metadata bit definitions to enable/disable
  RawToDepthDsp::median1d(rangeStripe, _ranges, _binning[1]);
#else
  std::copy(rangeStripe.begin(), rangeStripe.end(), _ranges.begin());
#endif
}

void RawToDepthStripe_float::processWholeFrame(std::function<void(std::shared_ptr<FovSegment>)> setFovSegment)
{
  auto localTimer = LumoTimers::ScopedTimer(*_timers, "RawToDepthStripe_float::processWholeFrame()", STRIPE_TIMERS_UPDATE_EVERY);

  if (_disableRtd) 
  {
    return;
  }

  if (_incompleteFov)
  {
    LLogErr("Skipping whole-frame processing. Incomplete FOV received.");
    return;
  }

  if (1 != _expectedNumRois ||
      0 != _currentRoiIdx ||
      !lastRoiReceived())
  {
    LLogErr("Skipping whole-frame processing. expected num ROIs:" << _expectedNumRois << " actual num ROIs:" << _currentRoiIdx+1);
    return;
  }

  auto roiIndices = std::make_shared<std::vector<uint16_t>>(_binnedRoiWidth, 0); // All samples in an ROI have the same timestamp.

  SCOPED_VEC_F(fMinMaxMask, _binnedRoiWidth);
  std::fill(fMinMaxMask.begin(), fMinMaxMask.end(), 0.0F);

  // Index into the pixel mask
  std::array<uint16_t,2> maskStartIdx = {_roiStartRow, RtdMetadata::getRoiStartColumn()};
  std::array<uint16_t,2> maskStep = {(uint16_t)_binning[0], (uint16_t)_binning[1]};
  uint16_t pixelMaskStride = RtdMetadata::getRoiNumColumns(); // Width of the pixel mask
  std::array<uint32_t,2> roiSize {1, _binnedRoiWidth};
  auto rangeRoi = RawToDepthCommon::getRange(_ranges, 
                    fMinMaxMask, _pixelMask, _snr,
                    maskStartIdx, // pre-binned sensor location
                    maskStep, // stepping across the mask table.
                    pixelMaskStride, 
                    roiSize, // The size of the output FOV, 1x640/binning
                    _disableRangeMasking, _snrThresh,
                    _temperatureCalibration.getRangeOffsetTemperature(), 
                    _rangeLimit,
                    (float_t)getMaxUnambiguousRange());


  // For stripe mode: recompute the position of this ROI in the mapping table.
  // "(2*_roiNumRows)/2". The first "2" is for the mapping table step. The second is an offset to half the vertical size of the ROI,
  const std::array<uint32_t,2> mappingTableStart = {uint32_t(2 * _roiStartRow + (2*_roiNumRows)/2 - 1),
                                                  uint32_t(2 * RtdMetadata::getRoiStartColumn() + _binning[1] - 1)};
  std::array<uint32_t,2> mappingTableStep = {uint32_t(2 * _binning[0]),
                                            uint32_t(2 * _binning[1])};
  const std::array<uint32_t,2> fovStart = { (_roiStartRow + _roiNumRows/2)/_binning[0], RtdMetadata::getRoiStartColumn()/_binning[1] };
  const std::array<uint32_t,2> fovStep = _binning;

  setFovSegment(std::make_shared<FovSegment>(
    _fovIdx,
    _headerNum,
    _timestamp,
    _sensorID,
    _userTag,
    lastRoiReceived(),
    getGCF(),
    getMaxUnambiguousRange(),
    roiSize,
    rangeRoi,
    mappingTableStart,
    mappingTableStep,
    fovStart,
    fovStep,
    RawToDepthCommon::getSnr(_snr),
    RawToDepthCommon::getSignal(_signal),
    RawToDepthCommon::getBackground(_background),
    roiIndices,
    getTimestamps(),
    getTimestampsVec(),
    getLastTimerReport()
  ));
}
