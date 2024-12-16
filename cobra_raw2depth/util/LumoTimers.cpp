/**
 * @file LumoTimers.cpp
 * @brief A utility class for capturing timing information during live execution.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "LumoTimers.h"
#include <sstream>
#include <iomanip>
#include <list>
#include <fstream>

LumoTimers::LumoTimers(std::string reportName) :
  _reportName(reportName) {
  report();
}

LumoTimers::Timer &LumoTimers::get(std::string timerName) {
  if (0 == _timers.count(timerName)) // "contains()" not until C++20. https://en.cppreference.com/w/cpp/container/map
  {
    _timers.emplace(timerName, std::make_shared<LumoTimers::Timer>());
  }
  return *_timers[timerName];
}

/**
 * @brief Start the named timer.
 * This sets the _lastStartTime and sets _started to true.
 * Also sets _numberOfIterationsToAccumulate to the given value.
 * Setting _numberOfIterationsToAccumulate to a different value on 
 * subsequent calls results in undefined behavior.
 * 
 * @param timerName The unique name for the timer that's being started.
 * @param reportEvery Generate an output string once every reportEvery-th call to report().
 */
void LumoTimers::start(std::string timerName, uint32_t reportEvery) {
  std::scoped_lock mutexLock(_mutex);
  get(timerName)._lastStartTime = std::chrono::high_resolution_clock::now();
  get(timerName)._started = true;
  get(timerName)._numberOfIterationsToAccumulate = reportEvery;
}

/**
 * @brief Stops accumulating time a particular 
 * 
 * @param timerName 
 */
void LumoTimers::stop(std::string timerName) {
  std::scoped_lock mutexLock(_mutex);
  if (!get(timerName)._started) {
    return;
  }
  
  auto startTime = std::chrono::high_resolution_clock::now();
  
  get(timerName)._totalExecutionTimeMicroseconds +=
    std::chrono::duration_cast<std::chrono::microseconds>(startTime - get(timerName)._lastStartTime).count();
  get(timerName)._oneMeasurement = true;
}

struct BenchJsons
{
  struct BenchJson
  {
    std::string timerName;
    uint32_t numberOfMeasurements;
    uint32_t totalExecutionTimeMicroseconds;
    uint32_t averageExecutionTimeMicroseconds;
  };
  std::string timestamp;
  std::list<BenchJson> benches;

  std::string create()
  {
    std::ostringstream json;
    json << "{" << std::endl;
    json << "\t" << "\"timestamp\": \"" << timestamp << "\"," << std::endl;
    json << "\t" << "\"timers\":" << std::endl;
    json << "\t" << "[" << std::endl;
    for(const auto &bench : benches)
    {
      json << "\t\t" << "{" << std::endl;
      json << "\t\t\t" << "\"timerName\": \"" << bench.timerName << "\"," << std::endl;
      json << "\t\t\t" << "\"numberOfMeasurements\": " << bench.numberOfMeasurements << "," << std::endl;
      json << "\t\t\t" << "\"totalExecutionTimeMicroseconds\": " << bench.totalExecutionTimeMicroseconds << "," << std::endl;
      json << "\t\t\t" << "\"averageExecutionTimeMicroseconds\": " << bench.averageExecutionTimeMicroseconds << "," << std::endl;
      json << "\t\t" << "}" << std::endl;
    }
    json << "\t" << "]" << std::endl;
    json << "}" << std::endl;
    return json.str();
  }

  void write(std::string reportName, uint32_t detailedFileIdx)
  {
#ifdef RTD_DETAILED_BENCHMARKING_DIR
    const std::string filenameBase {"LumoTimers_"};
    std::string dirpath {RTD_DETAILED_BENCHMARKING_DIR};
    if (dirpath.empty())
    {
      dirpath = "/tmp/";
    }
    std::string trailingChar;
    if (dirpath.back() != '/')
    {
      trailingChar = "/";
    }
    std::ostringstream filename;
    filename << dirpath << trailingChar << filenameBase << reportName << "_" << std::setfill('0') << std::setw(2) << detailedFileIdx << ".json";
    LLogInfo("Outputting benchmarking data to " << filename.str());
    auto outf = std::ofstream(filename.str(), std::ios::out);
    if (outf.is_open())
    {
      outf << create();
    }
#endif
  }
};

std::string LumoTimers::report() 
{
  auto nowTime {std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
  std::ostringstream timeStr; timeStr << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %X");

  BenchJsons benchJsons;
  benchJsons.timestamp = timeStr.str();

  if (_timers.empty()) 
  {
    return "";
  }
  
  std::ostringstream totalMessage;

  for (auto& element : _timers) {
    const auto &timerName {element.first};
    auto thisTimer {element.second};

    if (!thisTimer->_oneMeasurement) {
      continue; // Don't report a time for a timer without at least one complete measurement.
    }
    
    if (++thisTimer->_iterationCounter < thisTimer->_numberOfIterationsToAccumulate) {
      continue;
    }

    std::ostringstream message;
    message << "Timing (us) for timer \"" + _reportName + "\" ";
    constexpr int nameWidth {10};
    message << std::setw(nameWidth);
    message << element.first;
    message << " (avg over " + std::to_string(thisTimer->_iterationCounter) + "): ";
    constexpr int microsecondsWidth {8};
    message << std::setw(microsecondsWidth);
    message << std::to_string(thisTimer->_totalExecutionTimeMicroseconds/thisTimer->_iterationCounter);

    totalMessage << "\n";
    totalMessage << message.str();

    LLogDebug(message.str());

    benchJsons.benches.push_back(
      {
        timerName, 
        thisTimer->_numberOfIterationsToAccumulate, 
        thisTimer->_totalExecutionTimeMicroseconds, 
        thisTimer->_totalExecutionTimeMicroseconds/thisTimer->_iterationCounter
      });

    thisTimer->reset();
  }

  if (!benchJsons.benches.empty())
  {
    benchJsons.write(_reportName, _detailedFileIdx);
    _detailedFileIdx = (_detailedFileIdx + 1) % NUM_DETAILED_BENCHMARKING_FILES;
  }

  _lastReport = totalMessage.str();
  return totalMessage.str();
}

