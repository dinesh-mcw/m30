/**
 * @file RawToFovs.cpp
 * @brief The high-level entry point for passing data to, and retrieving results from
 * the RawToDepth library.
 * 
 * This code creates a unique RawToDepth object for each expected output FOV,
 * directs the data to each as needed, and receives results from the 
 * processWholeFrame thread.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToFovs.h"
#include "RawToDepthFactory.h"
#include "RtdMetadata.h"
#include "FovSegment.h"

RawToFovs::RawToFovs(uint32_t headerNum) : _headerNum(headerNum),
                                           _newMappingTableAvailable(std::vector<bool>(MAX_ACTIVE_FOVS, false)),
                                           _newMappingTableAvailableForRawStream(false),
                                           _newPixelMaskAvailable(std::vector<bool>(MAX_ACTIVE_FOVS, false))
{
  for (auto idx = 0; idx < MAX_ACTIVE_FOVS; idx++)
  {
    _rtds.push_back(nullptr);
    _fovAvailable[idx] = false;
  }
}

/**
 * @brief Once fovsAvailable() returns an fovIdx with valid data, then 
 * calling this routine grabs the available data.
 * 
 * @param fovIdx Which FOV to retrieve data from
 * @return std::shared_ptr<FovSegment> A copy of the available data.
 */
std::shared_ptr<FovSegment> RawToFovs::getData(uint32_t fovIdx)
{
  if (!_fovAvailable[fovIdx])
  {
    return nullptr;
  }

  bool newMappingTable = _newMappingTableAvailable[fovIdx];
  _newMappingTableAvailable[fovIdx] = false;
  
  std::scoped_lock mutexLock(_mutex);
  if (nullptr == _rtds[fovIdx]) 
  {
    return nullptr;
  }

  auto pointCloudData = _availableData[fovIdx];
  pointCloudData->setMappingTable(_mappingTable);
  pointCloudData->setNewMappingTable(newMappingTable);
  _fovAvailable[fovIdx] = false;
  _availableData[fovIdx] = nullptr;
  return pointCloudData;
}

/**
 * @brief Returns a list of all FOVs with data currently available.
 * Always called synchronously right before getData() from the per-roi thread.

 * @return std::vector<uint32_t> a list of output FOVs that have completed processWholeFrame()
 */
std::vector<uint32_t> RawToFovs::fovsAvailable()
{
  std::vector<uint32_t> availableFovs;
  for (uint32_t idx = 0; idx < MAX_ACTIVE_FOVS; idx++)
  {
    std::scoped_lock mutexLock(_mutex);
    if (nullptr != _rtds[idx])
    {
      if (_fovAvailable[idx])
      {
        availableFovs.push_back(idx);
      }
    }
  }
  return availableFovs;
}

/**
 * @brief The entry point for the RawToDepth algorithms.
 * When a new ROI is received from the driver, the user calls this
 * routine to run the per-ROI algorithms and (if it's the last ROI
 * in this FOV) also run RawToDepth::processWholeFrame() to generate
 * the output FOV
 * 
 * @param roi The raw data as received from the sensor, containing a row of metadata followed
 * by raw sensor data.
 * @param numBytes The size of the buffer containing the data.
 */
void RawToFovs::processRoi(const uint16_t *roi, uint32_t numBytes)
{

  RtdMetadata mdat(roi, numBytes);

  for (auto idx : mdat.getActiveFovs())
  {
    {
      std::scoped_lock mutexLock(_mutex);
      RawToDepthFactory::create(_rtds, mdat, idx, _headerNum);
    }

    if (_newPixelMaskAvailable[idx])
    {
      _newPixelMaskAvailable[idx] = false;
      std::scoped_lock mutexLock(_mutex);
      _rtds[idx]->loadPixelMask(_pixelMaskFilepath);
    }

    _rtds[idx]->processRoi(roi, numBytes);

    if (_rtds[idx]->lastRoiReceived())
    {
      _rtds[idx]->processWholeFrame([this, idx](std::shared_ptr<FovSegment> pointCloudData) 
                                                { std::scoped_lock mutexLock(this->_mutex);
                                                  this->_fovAvailable[idx] = true; 
                                                  this->_availableData[idx] = pointCloudData; 
                                                }); // returns immediately, async call
      }
  }
}

/**
 * @brief Call shutdown on each of the RawToDepth objects.
 *        This should be called just before destruction.
 *        It tells the running thread in RawToDepthV2_float to 
 *        quitNow, and waits for it to complete.
 * 
 */
void RawToFovs::shutdown()
{
  for (auto idx = 0; idx<_rtds.size(); idx++)
  {
    if (_rtds[idx] != nullptr)
    {
      _rtds[idx]->shutdown();
    }
  }
}