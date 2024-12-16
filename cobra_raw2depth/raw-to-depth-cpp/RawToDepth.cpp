/**
 * @file RawToDepth.cpp
 * @brief The parent class for the RawToDepth Module.
 *        Contains data and methods common for multiple RawToDepth specializations.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#include "RawToDepth.h"
#include "RawToDepthDsp.h"
#include "Binning.h"
#include "GPixel.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <cassert>
#include <LumoLogger.h>
#include <fstream>
#include <algorithm>
#include <cfloat>
#include <string>

using std::operator ""s;

constexpr uint16_t PIXEL_MASK_OFF {0xffff};
constexpr uint32_t DEFAULT_FOV_HEIGHT {240};
constexpr uint32_t DEFAULT_FOV_WIDTH {320};
constexpr uint32_t DEFAULT_BINNING {2};
constexpr std::array<uint32_t,2> DEFAULT_MAPPING_TABLE_START {0,0};
constexpr std::array<uint16_t,2> DEFAULT_PIXEL_MASK_START {0,0};
constexpr std::array<uint16_t,2> DEFAULT_PIXEL_MASK_SIZE {MAX_IMAGE_HEIGHT/DEFAULT_BINNING, IMAGE_WIDTH/DEFAULT_BINNING};
constexpr std::array<uint16_t,2> DEFAULT_PIXEL_MASK_STEP {DEFAULT_BINNING, DEFAULT_BINNING};
constexpr uint32_t defaultF0Idx { 7 };
constexpr uint32_t defaultF1Idx { 8 };

RawToDepth::RawToDepth(uint32_t fovIdx, uint32_t headerNum)
    : _fovIdx(fovIdx),
      _pixelMask(std::make_shared<std::vector<uint16_t>>(IMAGE_WIDTH * MAX_IMAGE_HEIGHT, PIXEL_MASK_OFF)),
      _timers(std::make_shared<LumoTimers>("RawToDepth")),
      _binning({DEFAULT_BINNING,DEFAULT_BINNING}),
      _size({DEFAULT_FOV_HEIGHT,DEFAULT_FOV_WIDTH}),
      _mappingTableStart(DEFAULT_MAPPING_TABLE_START),
      _mappingTableStep({DEFAULT_BINNING,DEFAULT_BINNING}),
      _sensorFovStart(DEFAULT_PIXEL_MASK_START),
      _sensorFovSize(DEFAULT_PIXEL_MASK_SIZE),
      _sensorFovStep(DEFAULT_PIXEL_MASK_STEP),
      _gcf((float_t)GPixel::getGcf(defaultF0Idx,defaultF1Idx)),
      _fs({GPixel::IDX_TO_FRQ_LUT[defaultF0Idx], GPixel::IDX_TO_FRQ_LUT[defaultF1Idx]}),
      _fsInt({roundf(_fs[0] / _gcf), roundf(_fs[1] / _gcf)}),
      _headerNum(headerNum)
{
  realloc((uint16_t *)RtdMetadata::DEFAULT_METADATA.data(),
          RtdMetadata::DEFAULT_METADATA.size() * sizeof(uint16_t));
}

RawToDepth::~RawToDepth()
{
  LLogDebug("RawToDepth dtor");
}

/// Initialization and verification methods.
void RawToDepth::reset(const uint16_t *mdPtr, uint32_t mdBytes)
{
  _timers->stop("RawToDepth_Frame_Loop");
  _timers->report();
  _timers->start("RawToDepth_Frame_Loop", TIMERS_UPDATE_EVERY);

  _prevRoiWasLast = false;

  realloc(mdPtr, mdBytes);
}

/**
 * @brief Checks the consistency of the system state.
 * A false in cond indicates that this ROI is skipped
 * If a message is supplied, then the error is logged.
 * If an ROI is skipped, then the _incompleteFov variable is set to 
 * true indicating the whole-frame processing (and data output) are
 * to be skipped.
 */
#define RETURN_CONDITION(cond, msg) { \
  if ((cond)) \
  { \
    std::ostringstream omsg; \
    omsg << msg;  /* NOLINT(bugprone-macro-parentheses) impossible to enclose stream expression in parens */ \
    if (!omsg.str().empty()) LLogErr(msg); \
    _incompleteFov = true; \
    return false; \
  } }


/**
 * @brief Performs a variety of checks on the state of the metadata.
 *        Called unconditionally on reception of each new ROI.
 *        Maintains a list of ROI start rows, and a list of timestamps (one per ROI)
 *        Passes the metadata to the TemperatureCalibration object to capture the ADC values.
 *  - Skip this ROI if the disableRtd bit is set
 *  - Skip this ROI if the buffer sizes changed since the last "first_roi" bit
 *    This indicates that a first_roi bit was missed due to error in data transmission.
 *  - Skip this ROI if more ROIs have arrived that expected since the last first_roi bit.
 *    This indicates that a first_roi bit was missed due to error in data transmission.
 *  - Appends the current timestamp to the end of the list of timestamps.
 *    So that when the final FOV is output, all timestamps are available.
 *  - Skip this ROI if the indicated output FOV size or location is out of bounds of the 
 *    previous allocated buffers. This indicates that there is an error in the metadata.
 *  - Skip this ROI if the starting row of the FOV is different than expected. This indicates
 *    an error in the metadata.
 *  - Skip this ROI if the starting row of the ROI is unexpected. This indicates an error
 *    in the metadata.
 * 
 * @param mdat The metadata from this ROI
 * @return false If this ROI is to be skipped.
 * @return true If this ROI is to be processed. 
 */
bool RawToDepth::saveTimestamp(RtdMetadata &mdat)
{
  assert(!mdat.wasPreviousRoiSaturated());
  _temperatureCalibration.setAdcValues(mdat, _fovIdx);
  _prevRoiWasLast = mdat.getFrameCompleted(_fovIdx);
  RETURN_CONDITION(mdat.getDisableRtd(_fovIdx), ""s);

  _currentRoiIdx++;

  RETURN_CONDITION(_currentRoiIdx >= _timestamps.size(),
           "Skipping ROI. Likely missed the first_roi. Metadata specifies " << 
            mdat.getFovNumRois(_fovIdx) << " ROIs in this FOV, so far " << 
            1+_currentRoiIdx << " have been received. Current fov idx is " << _fovIdx);

  // _currentRoiIdx is legal, store the current timestamp.
  if (_currentRoiIdx >= 0 && _currentRoiIdx < _timestamps.size())
  {
    _timestamps[_currentRoiIdx] = mdat.getTimestamp();
    _timestampsVec[_currentRoiIdx] = mdat.getTimestamps();
  }

  return true;
}


/**
 * @brief Error check. If the input data specifies a change in roi or fov size, then the
 * current buffer sizes are improperly sized for the data. Skip all the incoming ROIs 
 * until the next first_roi shows up (and the problem is self-repairing)
 * 
 * @param mdat The metadata for this ROI
 * @return true if mdat indicates that some buffer sizes have changed
 * @return false if mdat does not indicate a change in buffer sizes.
 */
bool RawToDepth::bufferSizesChanged(RtdMetadata &mdat)
{
  auto imsize = mdat.getFullImageHeight(_fovIdx) * mdat.getFullImageWidth(_fovIdx);
  return 
    ((mdat.getFullImageHeight(_fovIdx) != _size[0]) ||
     (mdat.getFullImageWidth(_fovIdx) != _size[1]) ||
     (mdat.getBinningY(_fovIdx) != _binning[0]) ||
     (mdat.getBinningX(_fovIdx) != _binning[1]) ||
     (mdat.getFovStartRow(_fovIdx) != _sensorFovStart[0]) ||
     (RtdMetadata::getFovStartColumn(_fovIdx) != _sensorFovStart[1]) ||
     (mdat.getFovNumRows(_fovIdx) != _sensorFovSize[0]) ||
     (RtdMetadata::getFovNumColumns(_fovIdx) != _sensorFovSize[1]) ||
     (mdat.getFovNumRois(_fovIdx) != _timestamps.size()) ||
     (_fs[0] != GPixel::IDX_TO_FRQ_LUT[mdat.getF0ModulationIndex()]) ||
     (_fs[1] != GPixel::IDX_TO_FRQ_LUT[mdat.getF1ModulationIndex()]) ||
     (_gcf != (float_t)GPixel::getGcf(mdat.getF0ModulationIndex(), mdat.getF1ModulationIndex())) ||
     (_fovStartRow != mdat.getFovStartRow(_fovIdx)));
}

/**
 * @brief Called whenever a new FOV is begun (RtdMetadata::getFirstRoi() == true)
 * 
 * @param mdPtr The metadata for this ROI
 * @param mdBytes The number of bytes in the metadata buffer
 */
void RawToDepth::realloc(const uint16_t *mdPtr, uint32_t mdBytes)
{
  bool changed = false; // Set by the MAKE_VECTOR macros if the buffer sizes have changed since the last FOV.

  chkBufMetadata(mdBytes);
  auto mdat = RtdMetadata(mdPtr, mdBytes); // Can't do constructor initialization list because of this local variable.
  auto roiBytes = sizeof(uint16_t) * mdat.getRoiNumRows() *
                  ROI_NUM_COLUMNS * mdat.getNumModulationFrequencies() *
                  NUM_GPIXEL_PHASES * mdat.getNumPermutations();

  assert(2 == mdat.getNumModulationFrequencies()); // assert to blow up during testing if the input assumptions are incorrect.

  _disableRtd = mdat.getDisableRtd(_fovIdx);
  _incompleteFov = false;

  _gcf = (float_t)GPixel::getGcf(mdat.getF0ModulationIndex(), mdat.getF1ModulationIndex());
  if (0 == _gcf) // Error, invalid modulation indices.
  {
    return;
  }

  _size[0] = mdat.getFullImageHeight(_fovIdx);
  _size[1] = mdat.getFullImageWidth(_fovIdx);

  _binning[0] = mdat.getBinningY(_fovIdx);
  _binning[1] = mdat.getBinningX(_fovIdx);

  _sensorID = mdat.getSensorId();
  _userTag = mdat.getUserTag(_fovIdx);

  // These two points are indices into the calibration data lookup table.
  _mappingTableStart = {uint32_t(2 * mdat.getFovStartRow(_fovIdx) + mdat.getBinningY(_fovIdx) - 1),
            uint32_t(2 * RtdMetadata::getFovStartColumn(_fovIdx) + mdat.getBinningX(_fovIdx) - 1)};
  _mappingTableStep = {uint32_t(2 * mdat.getBinningY(_fovIdx)),
           uint32_t(2 * mdat.getBinningX(_fovIdx))};

  // These index into the pixel mask
  _sensorFovStart = {mdat.getFovStartRow(_fovIdx), RtdMetadata::getFovStartColumn(_fovIdx)};
  _sensorFovSize = {mdat.getFovNumRows(_fovIdx), RtdMetadata::getFovNumColumns(_fovIdx)};
  _sensorFovStep = {mdat.getBinningY(_fovIdx), mdat.getBinningX(_fovIdx)};
  _roiNumRows = mdat.getRoiNumRows();
  _expectedNumRois = mdat.getFovNumRois(_fovIdx);
  _expectedScanTableTag = mdat.getScanTableTag();
  _expectedFovTag = mdat.getRandomFovTag(_fovIdx);

  _fs[0] = GPixel::IDX_TO_FRQ_LUT[mdat.getF0ModulationIndex()];
  _fs[1] = GPixel::IDX_TO_FRQ_LUT[mdat.getF1ModulationIndex()];

  if (_fsInt.empty()) 
  {
    _fsInt = std::vector<float_t>(2);
  }
  _fsInt[0] =roundf(_fs[0] / _gcf);
  _fsInt[1] =roundf(_fs[1] / _gcf);
  
  _snrThresh = mdat.getSnrThresh(_fovIdx);
  _disableStreaming = mdat.getDisableStreaming();
  _disableRangeMasking = mdat.getDisableRangeMasking(_fovIdx);

  _nearestNeighborFilterLevel = mdat.getNearestNeighborFilterLevel(_fovIdx);
  _performSumRotations = mdat.getDoTapAccumulation();
  _timestamp = mdat.getTimestamp();
  _fovStartRow = mdat.getFovStartRow(_fovIdx);

  auto imsize = _size[0] * _size[1];

  _currentRoiIdx = -1;
  MAKE_VECTOR(_timestamps, uint64_t, mdat.getFovNumRois(_fovIdx));
  MAKE_VECTOR(_timestampsVec, std::vector<uint32_t>, mdat.getFovNumRois(_fovIdx));

  // Each new fov, begin collecting roi start row indices.
  _roiStartRows.clear();
  _roiStartRows.reserve(mdat.getFovNumRois(_fovIdx));

  _rangeLimit = FLT_MAX;
  if (mdat.getEnableMaxRangeLimit(_fovIdx)) 
  {
    _rangeLimit = RANGE_LIMIT_FRACTION * float_t(getMaxUnambiguousRange());
  }

  std::ostringstream logId;
  logId << std::setw(4) << std::setfill('0') << "RawToDepth_" << _headerNum;
  LumoLogger::setId(logId.str());
}

/**
 * @brief Returns a copy of the timestamps array. Original 64-bit format.
 * 
 * @return std::shared_ptr<std::vector<uint64_t>> A copy of the timestamps
 */
std::shared_ptr<std::vector<uint64_t>> RawToDepth::getTimestamps()
{
  return std::make_shared<std::vector<uint64_t>>(_timestamps.begin(), _timestamps.end());
}

/**
 * @brief Returns a copy of the timestamps vector. New triple-uint format.
 * 
 * @return std::shared_ptr<std::vector<std::vector<uint32_t>>> A copy of the timestamps
 */
std::shared_ptr<std::vector<std::vector<uint32_t>>> RawToDepth::getTimestampsVec()
{
  return std::make_shared<std::vector<std::vector<uint32_t>>>(_timestampsVec.begin(), _timestampsVec.end());
}

#if defined(__unix__)
#define OUTPUT_RAW_DIR std::string("/run")
#else
#define OUTPUT_RAW_DIR std::string("/")
#endif

/**
 * @brief Utility function. Dumps the given raw ROI to a specific file path.
 * 
 */
void RawToDepth::dumpetyDump(const uint16_t *roi, uint32_t numBytes, RtdMetadata &mdat, uint32_t fovIdx)
{
  if (!mdat.getDumpRawRoi(fovIdx))
  {
    return;
  }

  std::string filename = OUTPUT_RAW_DIR + "/cobra_accumulated_raw_rois_0000.bin";
  LLogDebug("Saving raw data file to " << filename);
  auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
  outf.write((char *)(roi), numBytes);
  outf.close();
}

/**
 * @brief Loads the pixel mask from its location in the file system.
 *        Stores the data into the _pixelMask field of this class.
 * 
 * @param pixelMaskFilepath A fully-qualified path to the pixel mask file.
 */
void RawToDepth::loadPixelMask(std::string pixelMaskFilepath)
{

  if (pixelMaskFilepath.empty())
  {
    return;
  }

  LLogDebug("Reading pixel mask from " << pixelMaskFilepath);
  auto inf = std::ifstream(pixelMaskFilepath, std::ios::in | std::ios::binary);
  if (!inf.is_open())
  {
    LLogDebug("Unable to open input file " << pixelMaskFilepath << " for pixel mask. Default to passthrough.");
    _pixelMask = std::make_shared<std::vector<uint16_t>>(IMAGE_WIDTH * MAX_IMAGE_HEIGHT, PIXEL_MASK_OFF); // Default to passthrough.
    return;
  }

  inf.read((char *)(_pixelMask->data()), sizeof(uint16_t) * IMAGE_WIDTH * MAX_IMAGE_HEIGHT);
  inf.close();
}

/**
 * @brief Error check. Returns false if the given buffer size is smaller than is 
 *        indicated by the metadata. This means that this ROI buffer is malformed. 
 * 
 * @param mdat Metadata from this ROI
 * @param numBytesImagesAndHeader The total buffer size given for this ROI by the driver.
 * @return true If the buffer is big enough to hold the metadata and the raw ROI.
 * @return false otherwise.
 */
bool RawToDepth::chkBufDataWithHeader(RtdMetadata &mdat, uint32_t numBytesImagesAndHeader)
{

  uint32_t expectedBytes = sizeof(uint16_t) * ROI_NUM_COLUMNS * mdat.getNumPermutations() + // data line for header.
                           sizeof(uint16_t) *
                               mdat.getRoiNumRows() *                  // image height.1
                               ROI_NUM_COLUMNS * NUM_GPIXEL_PHASES * // image width
                               mdat.getNumModulationFrequencies() *    // Frequencies are interspersed between permutations.
                               mdat.getNumPermutations();

  if (expectedBytes > numBytesImagesAndHeader)
  {
    LLogErr("Input buffer too small for image data. Expected number of bytes for image and header is %" << expectedBytes << ". Buffer is declared to be of size " << numBytesImagesAndHeader);
    return false;
  }
  return true;
}

/**
 * @brief Checks if the given number of bytes is sufficient to hold the metadata for an ROI.
 * 
 * @param numBytes The size of a metadata buffer.
 * @return true If the buffer is large enough to hold the metadata
 * @return false otherwise.
 */
bool RawToDepth::chkBufMetadata(uint32_t numBytes)
{ // Check to see if the input buffer is large enough to hold metadata
  if (numBytes < sizeof(Metadata_t))
  {
    LLogErr("Input buffer too small for metadata. Buffer size is " << numBytes << ". Metadata size is " << sizeof(Metadata_t) << ". Dropping ROI.");
    return false;
  }
  return true;
}

/**
 * @brief Error check. Checks if the given buffer size is large enough to hold the raw ROI.
 * 
 * @param mdat The metadata that was provided with this ROI
 * @param numBytesImagesOnly The size of the given buffer.
 * @return true if the buffer size is large enough to hold the raw ROI data.
 * @return false otherwise.
 */
bool RawToDepth::chkBufDataOnly(RtdMetadata &mdat, uint32_t numBytesImagesOnly)
{ // Check to see if the input buffer is large enough to hold the ROI

  uint32_t expectedBytes =
      sizeof(uint16_t) *
      mdat.getRoiNumRows() *                  // image height.
      ROI_NUM_COLUMNS * NUM_GPIXEL_PHASES * // image width
      mdat.getNumModulationFrequencies() *    // Frequencies are interspersed between permutations.
      mdat.getNumPermutations();

  if (expectedBytes > numBytesImagesOnly)
  {
    LLogErr("Input buffer too small for image data. Expected number of bytes in image data only is " << expectedBytes << ". Buffer is declared to be of size " << numBytesImagesOnly);
    return false;
  }
  return true;
}

/**
 * @brief Performs error checking on the metadata internals.
 * 
 * @param roi The raw ROI data.
 * @param numBytes The size of the buffer containing the metadata and the raw ROI
 * @return true If the metadata is internally consistent.
 * @return false otherwise
 */
bool RawToDepth::validateMetadata(const uint16_t *roi, uint32_t numBytes) const
{
  if (!chkBufMetadata(numBytes))
  {
    return false;
  }

  auto rowShorts = ROI_NUM_COLUMNS * NUM_GPIXEL_PHASES;
  uint32_t roiBytes = numBytes - rowShorts * sizeof(uint16_t);
  auto mdat = RtdMetadata(roi, numBytes);
  // Run-time check that buffer sizes are allocated correctly.
  chkBufDataWithHeader(mdat, numBytes);

  uint32_t roiShorts = roiBytes / sizeof(uint16_t);
  const auto *roiInput = roi + rowShorts; // pointer to the raw roi data
  const auto *mdPtr = roi;

  if (!chkBufDataOnly(mdat, roiBytes))
  {
    return false;
  }

  if (!validateMetadataValues(mdat))
  {
    LLogErr("Invalid Metadata. Ignoring ROI.");
    return false;
  }

  if (!_veryFirstRoiReceived && !mdat.getFirstRoi(_fovIdx))
  {
    LLogDebug("Ignoring ROI: the first_roi has not yet been received for this FOV. ROI counter: " << mdat.getRoiCounter());
    return false;
  }

  if (!_veryFirstRoiReceived && mdat.wasPreviousRoiSaturated())
  {
    LLogErr("Skipping this ROI. First ever ROI is marked as an HDR retake.");
    return false;
  }

  dumpetyDump(roi, numBytes, mdat, _fovIdx);
  return true;
}

/// Metadata error checking.
#define val_ck(a, b)                       \
  if (!(a))                                \
  {                                        \
    std::string message = (b); \
    LLogErr("Invalid metadata: " << message); \
    assert(a);                             \
    return false;                          \
  }

bool RawToDepth::validateMetadataValues(RtdMetadata &mdat)
{
  val_ck(mdat.getSensorMode() == SENSOR_MODE_DMFD, "Only DMFD is supported");
  val_ck(mdat.getNumModulationFrequencies() == 2, "The number of modulation frequencies must be 2.");
  val_ck((int)mdat.getF0ModulationIndex() - (int)mdat.getF1ModulationIndex() == 1, "modulation indices must be adjacent, F0 must be the lower frequency");
  return true;
}
