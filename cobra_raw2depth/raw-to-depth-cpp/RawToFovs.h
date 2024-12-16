/**
 * @file RawToFovs.h
 * @brief The top-level entry point for passing data to, and retrieving results from
 * the RawToDepth library.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#pragma once

#include "FovSegment.h"
#include "MappingTable.h"
#include "RawToDepth.h"
#include <atomic>
#include <cstdint>
#include <vector>

class RawToFovs
{
 private:
  uint32_t _headerNum; ///< Which sensor is this object receiving data from
  std::vector<std::unique_ptr<RawToDepth>> _rtds; ///< The RawToDepth objects, one per output FOV
  std::shared_ptr<MappingTable> _mappingTable;    ///< The calibration mapping table, loaded with a call to reloadCalibrationTable()
  std::vector<bool> _newMappingTableAvailable;    ///< Indicates that a new mapping table for the given fovIdx needs to get transmitted over the network.
  bool _newMappingTableAvailableForRawStream;     ///< Indicates that a new mapping table needs to get transmitted for the raw data output
  std::vector<bool> _newPixelMaskAvailable;       ///< Indicates that a new pixel mask is available for the given fovIdx
  std::string _pixelMaskFilepath;                 ///< Indicates to each RawToDepth object where to load the pixel mask from.
  ///< Each output FOV provides data for the downstream consumer, and it is stored here until overwritten by the next completed FOV
  std::vector<std::shared_ptr<FovSegment>> _availableData = std::vector<std::shared_ptr<FovSegment>>(MAX_ACTIVE_FOVS, nullptr);
  ///< Each entry indicates whether that fovIdx has output data (FovSegment) that has not yet been retrieved.
  ///< Once the FovSegment has been retrieved by the consumer, then its indicator is set to false.
  std::array<std::atomic_bool, MAX_ACTIVE_FOVS> _fovAvailable { false };
  ///< Used to lock access to local variables from multiple threads.
  std::mutex _mutex;
  
 public:
  explicit RawToFovs(uint32_t headerNum=0);
  virtual ~RawToFovs()=default;

  RawToFovs(RawToFovs &other) = delete;
  RawToFovs(RawToFovs &&other) = delete;
  RawToFovs &operator=(RawToFovs &rhs) = delete;
  RawToFovs &operator=(RawToFovs &&rhs) = delete;


  ///< This call is (sometimes) asynchronous. The last ROI in a grid-mode frame starts a separate thread to perform whole-frame processing.
  ///< If this function is called, then RawToFovs::wait() must be called before destruction.
  void processRoi(const uint16_t *roi, uint32_t numBytes);
  std::vector<uint32_t> fovsAvailable();
  std::shared_ptr<FovSegment> getData(uint32_t fovIdx);
  virtual void shutdown();

  std::shared_ptr<std::vector<int32_t>> getCalibrationX()     { return _mappingTable ? _mappingTable->getCalibrationX() : nullptr; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationY()     { return _mappingTable ? _mappingTable->getCalibrationY() : nullptr; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationTheta() { return _mappingTable ? _mappingTable->getCalibrationTheta() : nullptr; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationPhi()   { return _mappingTable ? _mappingTable->getCalibrationPhi() : nullptr; }

  /**
   * @brief Called by the user to indicate that the System Control software has provided a new mapping table
   * 
   * @param mappingTableFilename Path on the local file system to the new mapping table
   * @param pixelMaskFilename Path on the local file system to the new pixel mask.
   */
  void reloadCalibrationData(std::string mappingTableFilename = "", std::string pixelMaskFilename = "") {
    const static auto ABCD = std::string("ABCD");
    std::string mappingTableFilepath = mappingTableFilename;
    
    if (mappingTableFilepath.empty()) {
      mappingTableFilepath = std::string(MAPPING_TABLE_FILE_ROOT) + ABCD[_headerNum] + ".bin";
    }

    _mappingTable = std::make_shared<MappingTable>(mappingTableFilepath);
    _newMappingTableAvailable = std::vector<bool>(MAX_ACTIVE_FOVS, true);
    _newMappingTableAvailableForRawStream = true;

    _pixelMaskFilepath = pixelMaskFilename;
    if (_pixelMaskFilepath.empty()) {
      _pixelMaskFilepath = std::string(PIXEL_MASK_FILE_ROOT) + ABCD[_headerNum] + ".bin";
    }
    _newPixelMaskAvailable = std::vector<bool>(MAX_ACTIVE_FOVS, true);
  }
};
