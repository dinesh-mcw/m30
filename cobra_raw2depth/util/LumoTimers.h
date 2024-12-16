/**
 * @file LumoTimers.h
 * @brief A set of utility classes for capturing timing information during live execution.
 * 
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include "LumoLogger.h"
#include <map>
#include <string>
#include <cstdint>
#include <chrono>
#include <mutex>

class OneTimeTimer
{
private:
  std::chrono::_V2::system_clock::time_point _t0;
  uint32_t &_totalTime;
  std::string _tag;
  uint32_t &_count;
  uint32_t _updateAfter;

public:
  OneTimeTimer(uint32_t &totalTime, uint32_t &count, uint32_t updateAfter, std::string tag) :
    _t0(std::chrono::high_resolution_clock::now()),
    _totalTime(totalTime), 
    _tag(tag),
    _count(count),
    _updateAfter(updateAfter)
  {
    _updateAfter = updateAfter;
    _count = ++count;
  }

  ~OneTimeTimer()
  {
    _totalTime += uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _t0).count());
    if (_count >= _updateAfter)
    {
      LLogDebug("Timer " << _tag << " total time: " << _totalTime/_count << " us");
      _count = 0;
      _totalTime = 0;
    }
  }    

  OneTimeTimer(OneTimeTimer &other) = delete;
  OneTimeTimer(OneTimeTimer &&other) = delete;
  OneTimeTimer & operator=(OneTimeTimer &rhs) = delete;
  OneTimeTimer & operator=(OneTimeTimer &&rhs) = delete;
};

/**
 * @brief The primary class for performing timing measurements.
 * This parent class holds a number of individual timers that are 
 * each identified by a unique label.
 * 
 * The timers are designed so that time can accumulate over multiple start/stop
 * calls.
 * 
 * A number of measurements can be averaged together. For a particular timer, 
 * report() returns a non-empty string once for every "_reportEvery" times that report()
 * is called. Of start/stop are called multiple times between report() calls, then
 * all of those times are accumulated.
 * 
 * Since report() returns a non-empty string only once every "_reportEvery" calls,
 * it's possible to gather timing information over a longer time period. That way,
 * if the output of LumoTimers is logged, then this prevents LumoTimers from spamming the
 * log with too many reports.
 * 
 * If RTD_DETAILED_BENCHMARKING_DIR is defined at compile time, the report() method
 * generates a json file containing the timing information within the specified
 * directory. NUM_DETAILED_BENCHMARKING_FILES json files are rotated within the output 
 * directory, named according to _detailedFileIdx.
 * 
 */
class LumoTimers {

 public: // inner classes

  /**
   * @brief The inner class that contains timing information for one measurement.
   * Each timer is uniquely identified by a string -- the key to the _timers std::map.
   * The first time LumoTimers::start() is called with a unique string, a new timer is created.
   * Whevever stop() is called with that string, a single timing measurement is stored into the timer.
   * 
   */
  class Timer {
  public:

    Timer() :
      _lastStartTime(std::chrono::high_resolution_clock::now()),
      _numberOfIterationsToAccumulate(1),
      _totalExecutionTimeMicroseconds(0),
      _iterationCounter(0),
      _started(false),
      _oneMeasurement(false)
    {}
    
    void reset() {
      _lastStartTime = std::chrono::high_resolution_clock::now();
      _totalExecutionTimeMicroseconds = 0;
      _iterationCounter=0;
      _started = false;
      _oneMeasurement = false;
    }
    
    /// Whenever start() is called on a timer, the current time is stored.
    std::chrono::time_point<std::chrono::high_resolution_clock> _lastStartTime;
    /// Average the time over this many calls the report()
    uint32_t _numberOfIterationsToAccumulate;
    /// Sum of the total amount of execution time -- time between start()/stop() calls
    uint32_t _totalExecutionTimeMicroseconds;
    /// The total number of times report() has been called on this timer.
    uint32_t _iterationCounter;
    /// Set to true if the timer has been started at least once.
    bool _started;
    /// Set to true of a pair of start()/stop() calls has been called on this timer.
    bool _oneMeasurement;
  };
  
  /***
   * A class to enable RAII for a particular timer.
   * Calls start() on the timer at construction time.
   * Calls stop() on the timer when the ScopedTimer is destructed.
  */
  class ScopedTimer {
  private:
    /// A reference to the LumoTimers object containing the relevant timer.
    LumoTimers &_timers;
    /// The unique name for the timer that is being manipulated by this class.
    std::string _timerName;

  public: 
    /// Constructor for this RAII object. Starts the timer at construction time.
    ScopedTimer(LumoTimers &timers, std::string timerName, uint32_t reportEvery=1) :
      _timers(timers),
      _timerName(timerName) {
      _timers.start(timerName, reportEvery);
    }
    
    /// Destructor for this RAII object. Stops the timer at destruction time.
    virtual ~ScopedTimer() {
      _timers.stop(_timerName);
    }

  ScopedTimer(ScopedTimer &other) = delete;
  ScopedTimer(ScopedTimer &&other) = delete;
  ScopedTimer &operator=(ScopedTimer &rhs) = delete;
  ScopedTimer &operator=(ScopedTimer &&rhs) = delete;
  };
  
private: //fields
  /// Holds all timers. Each timer is uniquely identified by the key, a std::string
  std::map<std::string, std::shared_ptr<LumoTimers::Timer>> _timers;
  /// Each LumoTimers object contains a string that is printed with each timer.
  std::string _reportName;
  /// Holds the report generated the last time the report() method returned a non-empty string
  std::string _lastReport;
  /// All start/stop operations are protected by this mutex.
  std::mutex _mutex;
  /// if RTD_DETAILED_BENCHMARKING_DIR is defined, report timing results as a json file, rotating through this many files.
  static constexpr uint32_t NUM_DETAILED_BENCHMARKING_FILES {10};
  /// if RTD_DETAILED_BENCHMARKING_DIR is defined, increment this value each time report() returns a non-empty string. Alias this index when it reaches NUM_DETAILED_BENCHMARKING_FILES
  uint32_t _detailedFileIdx {0};

  
public: // methods
  void start(std::string timerName, uint32_t reportEvery=1);
  void stop(std::string timerName);
  std::string report();
  std::string getLastReport() { return _lastReport; }
  
  explicit LumoTimers() = default;
  explicit LumoTimers(std::string reportName);
  ~LumoTimers() = default;

  LumoTimers(LumoTimers &other) = delete;
  LumoTimers(LumoTimers &&other) = delete;
  LumoTimers &operator=(LumoTimers &rhs) = delete;
  LumoTimers &operator=(LumoTimers &&rhs) = delete;
  
private: // methods
  Timer &get(std::string timerName);
  
   
};
