/**
 * @file MappingTable.cpp
 * @brief A utility for loading the angle-to-angle calibration mapping table.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "MappingTable.h"
#include "LumoLogger.h"
#include <fstream>
#include <sstream>
#include <cassert>

#define chk(a) if ((a).empty()) { fail = true; break; }

MappingTable::MappingTable(std::string mappingTableFilename) {

  LLogDebug("Attempting to load " << mappingTableFilename << " into the mapping table.");
  std::ifstream file(mappingTableFilename);
  
  if (!file.is_open()) 
  {

    LLogErr("Unable to open input file " << mappingTableFilename << ". Mapping table undefined.");
    _calibrationX = nullptr;
    _calibrationY = nullptr;
    _calibrationTheta = nullptr;
    _calibrationPhi = nullptr;
    return;
  }

  _calibrationX     = std::make_shared<std::vector<int32_t>>(MAPPING_TABLE_LENGTH);
  _calibrationY     = std::make_shared<std::vector<int32_t>>(MAPPING_TABLE_LENGTH);
  _calibrationTheta = std::make_shared<std::vector<int32_t>>(MAPPING_TABLE_LENGTH);
  _calibrationPhi   = std::make_shared<std::vector<int32_t>>(MAPPING_TABLE_LENGTH);


  if ( std::equal(mappingTableFilename.end()-4, mappingTableFilename.end(), ".bin") )
  {
    loadBinTable(file);
    return;
  }
  loadCsvTable(file);
}

void MappingTable::loadBinTable(std::ifstream &file)
{
  int idx=0;
  while (file.peek() != EOF &&
         idx < MAPPING_TABLE_LENGTH)
  {
    assert(idx < MAPPING_TABLE_LENGTH);
    int32_t aval; 
    int32_t bval;
    int32_t cval; 
    int32_t dval;
    file.read(reinterpret_cast<char*>(&aval), sizeof(aval));
    _calibrationX->at(idx) = aval;

    file.read(reinterpret_cast<char*>(&bval), sizeof(bval));
    _calibrationY->at(idx) = bval;

    file.read(reinterpret_cast<char*>(&cval), sizeof(cval));
    _calibrationTheta->at(idx) = cval;

    file.read(reinterpret_cast<char*>(&dval), sizeof(dval));
    _calibrationPhi->at(idx) = dval;
    idx++;
  }
}

void MappingTable::loadCsvTable(std::ifstream &file)
{

  bool fail = false;
  std::string line;
  uint32_t idx=0;
  
  while (std::getline(file, line)) {
    std::stringstream lineStream(line);
    std::string       cell;
    
    assert(idx < MAPPING_TABLE_LENGTH);
    
    std::getline(lineStream, cell, ',');
    chk(cell);
    _calibrationX->at(idx) = std::stoi(cell);
    
    std::getline(lineStream, cell, ',');
    chk(cell);
    _calibrationY->at(idx) = std::stoi(cell);
    
    std::getline(lineStream, cell, ',');
    chk(cell);
    _calibrationTheta->at(idx) = std::stoi(cell);
    
    std::getline(lineStream, cell, ',');
    chk(cell);
    _calibrationPhi->at(idx) = std::stoi(cell);
    idx++;
    
  }
    
  file.close();
  assert(idx == MAPPING_TABLE_LENGTH);
  
  if (fail) {
    _calibrationX = nullptr;
    _calibrationY = nullptr;
    _calibrationTheta = nullptr;
    _calibrationPhi = nullptr;
    LLogErr("Invalid input file format when creating MappingTable.");
    return;
  }
  

  LLogDebug("MappingTable loading succeeded.");
  
}

