/**
 * @file LumoLogger.cpp
 * @brief Logging for the RawToDepth repo.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 */

#include "LumoLogger.h"
#include <chrono>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstdarg>
#include <vector>
#include <thread>

static const int NS_PER_MS  { 1000000 };

LumoLogger *LumoLogger::_inst { nullptr };
std::array<char, LUMO_LOG_TAG_SIZE> LumoLogger::_id = { LUMO_LOG_TAG };
std::atomic_uint LumoLogger::logLevel { LUMO_LOG_INFO };

// Force construction at startup -- otherwise log might not
// be opened in syslog case if we log directly to syslog
// through macros!
static LumoLogger *inst = LumoLogger::getInst();

#if defined(__unix__) && !defined(LOG_TO_CONSOLE)
LumoLogger::LumoLogger()
{
  openlog(_id.data(), LOG_PID|LOG_CONS, LOG_USER);
  setlogmask(LOG_UPTO(LOG_DEBUG));
}
LumoLogger::~LumoLogger()
{
  closelog();
  delete _inst;
}

#else
LumoLogger::LumoLogger() = default;
LumoLogger::~LumoLogger()
{
  delete _inst;
}
#endif

void LumoLogger::setId(std::string idString)
{
  size_t copiedLen = idString.copy(idString.data(), idString.size() - 1, 0);
  _id[copiedLen] = '\0';
}

static time_t getNowTime()
{
  auto now = std::chrono::system_clock::now();
  return std::chrono::system_clock::to_time_t(now);
}

static void getNowTimeSecAndMs(int &sec, int &msec)
{
  struct timespec now_ts {};
  if (clock_gettime(CLOCK_REALTIME, &now_ts) < 0)
  {
      return; // in case of failure return start of epoch because we can't log (because we use this function to log)
  }
  struct tm now_tm {};
  if (localtime_r(&now_ts.tv_sec, &now_tm) == NULL)
  {
      return; // in case of failure return start of epoch because we can't log (because we use this function to log)
  }
  sec = now_tm.tm_sec;
  msec = (int)(now_ts.tv_nsec / NS_PER_MS);
}

#if defined(__unix__) && !defined(LOG_TO_CONSOLE)

void LumoLogger::logString(int priority, const char *func, int line, std::string message)
{
  std::stringstream logStream;

  int nowS = 0;
  int nowMs = 0;

  getNowTimeSecAndMs(nowS, nowMs);
  logStream << std::setw(2) << std::setfill('0') << nowS << " " << std::setw(3) << std::setfill('0') << nowMs << " " << func << ":" << line << " " << message;
  syslog(priority, "%s", logStream.str().c_str());    /* NOLINT(hicpp-vararg) Calling Linux vararg API */
}

#else

// No millisecond timestamps when running on PC
void LumoLogger::logString(int priority, const char *func, int line, std::string message)
{
  auto nowTime = getNowTime();
  
  std::stringstream logStream;
  logStream << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %X") << _id.data() << "[" << getpid() << "]: " << func << ":" << line << " " << message;

  if (priority == LUMO_LOG_WARNING || priority == LUMO_LOG_ERR)
  {
    std::cerr << logStream.str() << "\n";
  }
  else
  {
    std::cout << logStream.str() << "\n";
  }
}

#endif

void LumoLogger::setLogLevel(unsigned int level)
{
  LumoLogger::logLevel = level;
}
