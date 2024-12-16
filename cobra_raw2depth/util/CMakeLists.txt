# @file CMakeLists.txt
# @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.

add_library(lumoutil STATIC LumoLogger.cpp LumoUtil.cpp LumoTimers.cpp FloatVectorPool.cpp LumoAffinity.cpp)
target_include_directories(lumoutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
