/**
 * @file FovSegment.h
 * @brief The data structure that holds the output point cloud data 
 * data for this FOV. 
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once
#include "MappingTable.h"

#include <cstdint>
#include <cassert>

class FovSegment {
private:
  const uint32_t _fovIdx;    ///< The index of this data's FOV
  const uint32_t _headerNum; ///< The index of the sensor that provided the raw data
  const uint64_t _timestamp; ///< A single timestamp that represents the entire FOV.
  const uint16_t _sensorId;  ///< The sensor ID that was provided by the metadata in RtdMetadata::getSensorId()
  const uint32_t _userTag;   ///< The user tag that was provided by the metadata in RtdMetadata::getUserTag()

  std::shared_ptr<MappingTable> _mappingTable = nullptr; ///< Holds the mapping table if a new one has been provided
  bool _newMappingTableAvailable = false; ///< Indicates whether the mapping table has been updated since the last time it was read by the consumer.

  const bool _frameCompleted; ///< True indicates whether whether this is the only, or the last in a series of, FovSegments for this FOV.
  const double _gcf;          ///< The greatest common frequency for this acquisition.
  const double _maxUnambiguousRange; ///< The maximum unambiguous range for this iTOF acquisition
  const std::vector<uint32_t> _imageSize; ///< The size of the output fov (height, width)

  const std::shared_ptr<std::vector<uint16_t>> _ranges; ///< The range data component of the output FOv
  const std::vector<uint32_t> _mappingTableTopLeft; ///< Coordinates for this data within the mapping table
  const std::vector<uint32_t> _mappingTableStep;    ///< Coordinates for this data within the mapping table
  const std::array<uint32_t,2> _fovTopLeft; ///< Coordinates for this fov within a (binned) iTOF sensor.
  const std::array<uint32_t,2> _fovStep; ///< Steps within sensor coordinates for this FOV (e.g. binning).

  const std::shared_ptr<std::vector<uint16_t>> _snr; //The SNR component for this output FOV
  const std::shared_ptr<std::vector<uint16_t>> _signal;  ///< The signal component for this output FOV
  const std::shared_ptr<std::vector<uint16_t>> _background; ///< The background component for this output FOV

  ///< An image the same size as other output components for this FOV. This buffer contains indices that indicate
  ///< which ROI was used to acquire each data point. This index can be used to index into the _timestamps or 
  ///< _timestampsVec vectors to get timestamps for each individual pixel.
  const std::shared_ptr<std::vector<uint16_t>> _roiIndexFov;

  ///< 64-bit timestamp, that is the lower 60 bits of the 7 12-bit metadata values.
  const std::shared_ptr<std::vector<uint64_t>> _timestamps;
  ///< Newer timestamp format, in which all 94 bits are split between 3 32-bit unsigned ints.
  const std::shared_ptr<std::vector<std::vector<uint32_t>>> _timestampsVec;
  ///< A string containing a report of timing during this acquisition.
  const std::string _timerReport;
  
public:
  FovSegment(uint32_t fovIdx,
      uint32_t headerNum,
      uint64_t timestamp, ///< lowest 64-bits of the FPGA timestamp.
      uint16_t sensorId,
      uint32_t userTag,
      bool frameCompleted,
      double gcf,
      double maxUnambiguousRange,
      std::array<uint32_t,2> imageSize,
      std::shared_ptr<std::vector<uint16_t>> ranges,
      std::array<uint32_t,2> mappingTableTopLeft = {0,0},
      std::array<uint32_t,2> mappingTableStep = {2,2},
      std::array<uint32_t,2> fovTopLeft = {0,0},
      std::array<uint32_t,2> fovStep = {2,2},
      std::shared_ptr<std::vector<uint16_t>> snr = nullptr,
      std::shared_ptr<std::vector<uint16_t>> signal = nullptr,
      std::shared_ptr<std::vector<uint16_t>> background = nullptr,
      std::shared_ptr<std::vector<uint16_t>> roiIndices = nullptr,
      std::shared_ptr<std::vector<uint64_t>> timestamps = nullptr,
      std::shared_ptr<std::vector<std::vector<uint32_t>>> timestampsVec = nullptr,
      std::string timerReport = ""
      ) :
    _fovIdx(fovIdx),
    _headerNum(headerNum),
    _timestamp(timestamp),
    _sensorId(sensorId),
    _userTag(userTag),
    _frameCompleted(frameCompleted),
    _gcf(gcf),
    _maxUnambiguousRange(maxUnambiguousRange),
    _imageSize({imageSize[0], imageSize[1]}),
    _ranges(ranges),
    _mappingTableTopLeft({mappingTableTopLeft[0], mappingTableTopLeft[1]}),
    _mappingTableStep({mappingTableStep[0], mappingTableStep[1]}),
    _fovTopLeft(fovTopLeft),
    _fovStep(fovStep),
    _snr(snr),
    _signal(signal),
    _background(background),
    _roiIndexFov(roiIndices),
    _timestamps(timestamps),
    _timestampsVec{timestampsVec},
    _timerReport(timerReport)
  {
    auto imageArea = std::size_t(_imageSize[0]) * std::size_t(_imageSize[1]);
    if (ranges) 
    {     
      assert(ranges->size() == imageArea); 
    }
    if (snr) 
    {       
      assert(snr->size() == imageArea); 
    }
    if (signal) 
    {    
      assert(signal->size() == imageArea); 
    }
    if (background) 
    { 
      assert(background->size() == imageArea); 
    }
    if (roiIndices) 
    { 
      assert(roiIndices->size() == imageArea); 
    }
  }

  void setMappingTable(std::shared_ptr<MappingTable> table) { _mappingTable = table; }
  void setNewMappingTable(bool newTableAvailable) { _newMappingTableAvailable = newTableAvailable; }
  
  const std::vector<uint32_t>    &getImageSize() const { return _imageSize; }
  const std::vector<uint32_t>    &getMappingTableTopLeft() const { return _mappingTableTopLeft; }
  const std::vector<uint32_t>    &getMappingTableStep() const { return _mappingTableStep; }
  std::array<uint32_t,2>   getFovTopLeft() const { return _fovTopLeft; }
  std::array<uint32_t,2>   getFovStep() const {return _fovStep; }
  
  uint16_t getFovIdx()    const { return _fovIdx; }
  uint16_t getHeaderNum() const { return _headerNum; }
  uint64_t getTimestamp() const { return _timestamp; }
  uint16_t getSensorId()  const { return _sensorId; }
  uint16_t getUserTag()   const{ return _userTag; }
  bool getFrameCompleted() const { return _frameCompleted; }
  double getGcf() const { return _gcf; }
  double getMaxUnambiguousRange() const { return _maxUnambiguousRange; }

  std::shared_ptr<std::vector<uint16_t>> getRange() const { return _ranges; }
  std::shared_ptr<std::vector<uint16_t>> getSnrSquared() const { return _snr; }
  std::shared_ptr<std::vector<uint16_t>> getSnr() const { return _snr; }
  std::shared_ptr<std::vector<uint16_t>> getBackground() const { return _background; }
  std::shared_ptr<std::vector<uint16_t>> getSignal() const { return _signal; }
  std::shared_ptr<std::vector<uint16_t>> getRoiIndexFov() const { return _roiIndexFov; }
  std::shared_ptr<std::vector<uint64_t>> getTimestamps() const { return _timestamps; }
  std::shared_ptr<std::vector<std::vector<uint32_t>>> getTimestampsVec() const { return _timestampsVec; }

  std::shared_ptr<std::vector<int32_t>> getCalibrationX()     const { return _mappingTable ? _mappingTable->getCalibrationX() : nullptr; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationY()     const { return _mappingTable ? _mappingTable->getCalibrationY() : nullptr; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationTheta() const { return _mappingTable ? _mappingTable->getCalibrationTheta() : nullptr;}
  std::shared_ptr<std::vector<int32_t>> getCalibrationPhi()   const { return _mappingTable ? _mappingTable->getCalibrationPhi() : nullptr; }

  bool isNewMappingTableAvailable() const { return _newMappingTableAvailable; }

  std::string getTimerReport() const { return _timerReport; }
    
};
