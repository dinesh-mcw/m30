/**
 * @file RawToDepthTests.cpp
 * @brief Google Test specialization
 * 
 * Copyright 2023 (C) Lumotive, Inc. All rights reserved.
 * 
 */

#include "RawToDepthTests.h"
#include <iostream>
#include "LumoLogger.h"

RawToDepthTests::RawToDepthTests()
{ 
  LLogSetLogLevel(LUMO_LOG_DEBUG);

  std::cout << "RawToDepthTests ctor\n"; 
}

