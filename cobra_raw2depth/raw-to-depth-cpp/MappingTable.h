/**
 * @file MappingTable.h
 * @brief A utility class for loading and accessing the calibration
 * angle-to-angle mapping table.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#define MAPPING_TABLE_LENGTH 1226561

class MappingTable {
 private:
  std::shared_ptr<std::vector<int32_t>> _calibrationX = nullptr;
  std::shared_ptr<std::vector<int32_t>> _calibrationY = nullptr;
  std::shared_ptr<std::vector<int32_t>> _calibrationTheta = nullptr;
  std::shared_ptr<std::vector<int32_t>> _calibrationPhi = nullptr;

 public:
  MappingTable() = default;
  explicit MappingTable(std::string mappingTableFilename);

  std::shared_ptr<std::vector<int32_t>> getCalibrationX()     { return _calibrationX; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationY()     { return _calibrationY; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationTheta() { return _calibrationTheta; }
  std::shared_ptr<std::vector<int32_t>> getCalibrationPhi()   { return _calibrationPhi; }

private:
  void loadCsvTable(std::ifstream &file);
  void loadBinTable(std::ifstream &file);
};
