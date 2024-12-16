/**
 * @file RawToDepth.h
 * @brief The parent class for the RawToDepth Module.
 *        Contains data and methods common for multiple RawToDepth specializations.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include "RtdMetadata.h"
#include "hdr.h"
#include "TemperatureCalibration.h"
#include "FovSegment.h"
#include "GPixel.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <iostream>
#include <LumoUtil.h>
#include <LumoTimers.h>
#include <tuple>
#include <functional>
#include <limits>


#define TIMERS_UPDATE_EVERY 100 ///< Output timer updates every 100 frames. About once every 10 seconds at full frame rate.
#define STRIPE_TIMERS_UPDATE_EVERY 1000

// The mapping table converts from sensor indices to angle-angle
#define MAPPING_TABLE_FILE_ROOT "/home/root/cobra/mapping_table_" //<ABCD>.bin
// The pixel mask defines the region over which the sensor is illuminated.
#define PIXEL_MASK_FILE_ROOT "/run/lumotive/pixel_mask_" // <ABCD.bin

using std::operator ""s;

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
 * @brief The parent class for RawToDepth processing
 *        One RawToDepth object is created for each output FOV.
 * 
 */
class RawToDepth
{
public :

protected:
  uint32_t _fovIdx; ///< Which output FOV does this RTD object belong to.

  std::shared_ptr<
    std::vector<uint16_t>> _pixelMask;
  std::vector<uint32_t> _minMaxFilterSize; ///< Either 1: a 2D vector containing {v,h} filter size, or 2: empty, indicating that the min-max filter is disabled.
  std::vector<uint64_t> _timestamps; ///< Holds one timestamp for each ROI (original 64-bit format)
  std::vector<
    std::vector<uint32_t>> _timestampsVec; ///< Holds one 3-element timestamp for each ROI (new 3-uint format)
  std::vector<int32_t> _roiStartRows; ///< Collect the start rows for all the rois.
  uint32_t             _fovStartRow { 0 };

  hdr _hdr; ///< High dynamic range processing
  bool _disableRtd = false; ///< Causes RawToDepth to ignore input data.
  
  std::shared_ptr<LumoTimers> _timers;
  TemperatureCalibration _temperatureCalibration;
  
  std::array<uint32_t,2> _binning;
  std::array<uint32_t,2> _size;     ///< FOV size after binning.
  std::array<uint32_t,2> _mappingTableStart; ///< top left point of the FOV in the mapping table.
  std::array<uint32_t,2> _mappingTableStep;  ///< step in each dimension between samples in the mapping table.
  std::array<uint16_t,2> _sensorFovStart; ///< In the pixel mask LUT (full sensor matrix), the start of the FOV
  std::array<uint16_t,2> _sensorFovSize;  ///< ... the size of the FOV, and
  std::array<uint16_t,2> _sensorFovStep;  ///< ... the step size for each pixel in the FOV.
  uint32_t _roiNumRows { 0 };      ///< The number of rows in each ROI.
  uint16_t _expectedNumRois {0};   ///< The getFovNumRois() from the metadata on the first ROI in an FOV.
  uint16_t _expectedScanTableTag {0}; ///< The scan table tag from the first ROI in this FOV.
  uint16_t _expectedFovTag {0};    ///< The FOV tag from the first ROI in this FOV.
  
  float_t                 _gcf; ///< Greatest Common Frequency between the modulation pairs.
  std::array<float_t, 2>  _fs;  ///< The two modulation frequencies, in Hz

  std::vector<float_t> _fsInt; ///< Integers describing the ratio of modulation frequency to the GCF
  static constexpr float_t _c_mps {299792498.0F};
  static constexpr float_t _halfc {0.5F*299792498.0F};
  float_t _rangeLimit {std::numeric_limits<float_t>::max()};
  
  // State variable. Read by external callers.
  bool     _prevRoiWasLast = false;     ///< Indicates that the previous roi returned true for RtdMetadata::getFrameCompleted()

  float    _snrThresh {0};              ///< (from metadata) Threshold below which output ranges are invalidated.
  bool     _performSumRotations = true; ///< (from metadata) Indicates that RTD performs tap rotation
  uint16_t _sensorID {0};               ///< (from metadata)
  uint32_t _userTag {0};                ///< (from metadata)
  bool     _disableStreaming {false};   ///< (from metadata)
  bool     _disableRangeMasking {false};///< (from metadata)
  bool     _veryFirstRoiReceived {false};   ///< (from metadata) True if the very first ROI has already been received following startup.
  int32_t  _currentRoiIdx {0};          ///< locally indexed counter that increments for each ROI that is received.
  uint64_t _timestamp {0};              ///< (from metadata) The original 64-bit format of the most recent timestamp that was received.
  bool     _incompleteFov {false};      ///< Set during ROI processing if any of the input ROIs were skipped.

  const uint32_t _headerNum;            ///< Indicates which scanhead the last ROI came from (Jetson) or zero otherwise (NCB)
  uint16_t _nearestNeighborFilterLevel {0}; ///< (from metadata) The nearest neighbor filter index.
  
 public:
  explicit RawToDepth(uint32_t fovIdx, uint32_t headerNum);
  RawToDepth() = delete;
  RawToDepth(RawToDepth &other) = delete;
  RawToDepth(RawToDepth &&other) = delete;
  RawToDepth &operator=(RawToDepth &rhs) = delete;
  RawToDepth &operator=(RawToDepth &&rhs) = delete;
  virtual ~RawToDepth();
  virtual void shutdown() {}

  virtual void processRoi(const uint16_t* roi, uint32_t numBytes)=0;
  virtual void processWholeFrame(std::function<void (std::shared_ptr<FovSegment>)> setFovSegment)=0;

  bool lastRoiReceived() const { return _prevRoiWasLast; }
  uint64_t getTimestamp() const { return _timestamp; }

  virtual void loadPixelMask(std::string pixelMaskFilepath = "");

  uint32_t getHeaderNum() const { return _headerNum; }
  uint16_t &getSensorID() { return _sensorID; }
  uint32_t &getUserTag() { return _userTag; }
  std::array<uint32_t,2> &getImageSize() { return _size; }
  std::array<uint32_t,2> &getImageStep() { return _mappingTableStep; }
  std::array<uint32_t,2> &getImageStart() { return _mappingTableStart; }

  double getModulationFrequency(int frq) const { return double(_fs[frq]); }
  double getGCF() const { return double(_gcf); }
  double getMaxUnambiguousRange() const { return double(_halfc)/double(_gcf); }

  std::shared_ptr<std::vector<uint64_t>> getTimestamps();
  std::shared_ptr<std::vector<std::vector<uint32_t>>> getTimestampsVec();
  std::string getLastTimerReport() { return _timers->getLastReport(); }

protected:
  
  virtual void reset(const uint16_t *mdPtr, uint32_t mdBytes); ///< Called at the first ROI of an FOV, to initialize internal buffers.
  // Returns true if the buffer sizes indicated by the given metadata are different from the current state of the object
  virtual bool bufferSizesChanged(RtdMetadata &mdat);    
  virtual bool saveTimestamp(RtdMetadata &mdat);

  static bool validateMetadataValues(RtdMetadata &mdat);
  bool validateMetadata(const uint16_t *roi, uint32_t numBytes) const;
  static bool chkBufDataWithHeader(RtdMetadata &mdat, uint32_t numBytesImagesAndHeader);
  static bool chkBufMetadata(uint32_t numBytes);  ///< Check to see if the input buffer is large enough to hold metadata
  static bool chkBufDataOnly(RtdMetadata &mdat, uint32_t numBytesImagesOnly); ///< Check to see if the input buffer is large enough to hold the ROI
  static void dumpetyDump(const uint16_t *roi, uint32_t numBytes, RtdMetadata &mdat, uint32_t fovIdx);

private:
  void realloc(const uint16_t *mdPtr, uint32_t mdBytes);

};

