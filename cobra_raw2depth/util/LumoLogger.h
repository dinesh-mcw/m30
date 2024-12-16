/**
 * @file LumoLogger.h
 * @brief Logging for the RawToDepth repo.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 */


#pragma once

#include <iostream>
#include <memory>
#include <ctime>
#include <set>
#include <string>
#include <sstream>
#include <atomic>

#if defined(__unix__)
#include <unistd.h>
#else
#define getpid() 0
#endif

#if defined(__unix__) && !defined(LOG_TO_CONSOLE)

// Log to syslog
#include <syslog.h>

#else // LOG_TO_CONSOLE

// Define log levels corresponding to the POSIX ones
#define	LOG_EMERG	0
#define	LOG_ALERT	1
#define	LOG_CRIT	2
#define	LOG_ERR		3
#define	LOG_WARNING	4
#define	LOG_NOTICE	5
#define	LOG_INFO	6
#define	LOG_DEBUG	7

#endif

#define LLog(_level,_expr)                      \
{                                               \
    if ((_level) <= LumoLogger::logLevel) {     \
        std::stringstream msgStream;            \
        msgStream << _expr;                     /* NOLINT(bugprone-macro-parentheses) prevents using a literal C string */ \
        LumoLogger::logString((_level), (const char *)__func__, __LINE__, msgStream.str());  \
    }                                           \
}


#define LUMO_LOG_ERR LOG_ERR
#define LUMO_LOG_WARNING LOG_WARNING
#define LUMO_LOG_INFO LOG_INFO
#define LUMO_LOG_DEBUG LOG_DEBUG
#define LUMO_LOG_DEBUG1 LOG_DEBUG
#define LUMO_LOG_DEBUG2 ((LOG_DEBUG)+1)
#define LUMO_LOG_DEBUG3 ((LOG_DEBUG)+2)
#define LUMO_LOG_DEBUG4 ((LOG_DEBUG)+3)
#define LUMO_LOG_DEBUG5 ((LOG_DEBUG)+4)
#define LUMO_LOG_DEBUG6 ((LOG_DEBUG)+5)

#define LLogDebug(_string) LLog(LUMO_LOG_DEBUG,_string)
#define LLogWarning(_string) LLog(LUMO_LOG_WARNING,_string)
#define LLogErr(_string) LLog(LUMO_LOG_ERR,_string)
#define LLogInfo(_string) LLog(LUMO_LOG_INFO,_string)

#define LLogSetLogLevel(_level) LumoLogger::setLogLevel(_level)

#ifndef LUMO_LOG_TAG
#define LUMO_LOG_TAG "lumotive"
#endif

#define LUMO_LOG_TAG_SIZE 32

class LumoLogger
{
 private:

  static LumoLogger *_inst;
  static std::array<char, LUMO_LOG_TAG_SIZE> _id;

  LumoLogger();
  ~LumoLogger();
  
 public:
  LumoLogger(LumoLogger &inst) = delete;
  LumoLogger(LumoLogger &&inst) = delete;
  LumoLogger& operator=(const LumoLogger&) = delete;
  LumoLogger& operator=(LumoLogger&&) = delete;

  static inline LumoLogger *getInst()
  {
    if (nullptr == _inst)
    {
      _inst = new LumoLogger();
    }
    return _inst;
  }

  static void setId(std::string idString);
  static void setLogLevel(unsigned int level);
  static void logString(int priority, const char *func, int line, std::string message);
  static std::atomic_uint logLevel;
};
