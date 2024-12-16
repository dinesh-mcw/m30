# @file CMakeLists.txt
# @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.10.0)
project(RawToDepth)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

IF(DEFINE_LOG_TO_CONSOLE)
  set(LOG_TO_CONSOLE -DLOG_TO_CONSOLE)
ENDIF(DEFINE_LOG_TO_CONSOLE)

IF(DEFINE_RTD_DETAILED_BENCHMARKING_DIR)
  SET(RTD_DETAILED_BENCHMARKING_DIR "-DRTD_DETAILED_BENCHMARKING_DIR=\"${DEFINE_RTD_DETAILED_BENCHMARKING_DIR}\"")
ENDIF(DEFINE_RTD_DETAILED_BENCHMARKING_DIR)

IF(DEFINE_DISABLE_ASYNC_PROCESS_WHOLE_FRAME)
  SET(DISABLE_ASYNC_PROCESS_WHOLE_FRAME "-DDISABLE_ASYNC_PROCESS_WHOLE_FRAME")
ENDIF(DEFINE_DISABLE_ASYNC_PROCESS_WHOLE_FRAME)

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
IF (CMAKE_CXX_COMPILER_ID  STREQUAL "Clang")
  set(MY_RELEASE_OPTIONS ${MY_RELEASE_OPTIONS} -Wno-unused -fvectorize -fsave-optimization-record -foptimization-record-file=myOptfile.yaml)
  SET(MY_DEBUG_OPTIONS "-Wno-unused")
  set(CMAKE_CXX_CLANG_TIDY clang-tidy -config-file=${CMAKE_CURRENT_LIST_DIR}/clang-tidy.yml -header-filter=.*)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
ENDIF()

IF (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  SET(MY_DEBUG_OPTIONS -Wno-sign-compare -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable)
  SET(MY_RELEASE_OPTIONS -Wno-sign-compare -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -O3 -ftree-vectorize -ffast-math -ftree-vectorizer-verbose=5 ${NEON_RELEASE_OPTIONS})
ENDIF()

set(MY_DEBUG_OPTIONS ${MY_DEBUG_OPTIONS} -DDebug -DDEBUG ${LOG_TO_CONSOLE} ${RTD_DETAILED_BENCHMARKING_DIR})
set(MY_RELEASE_OPTIONS ${MY_RELEASE_OPTIONS} -DNDebug ${LOG_TO_CONSOLE} ${RTD_DETAILED_BENCHMARKING_DIR} ${DISABLE_ASYNC_PROCESS_WHOLE_FRAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

IF (CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "MY_RELEASE_OPTIONS ${MY_RELEASE_OPTIONS}")
ENDIF()

IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "MY_DEBUG_OPTIONS ${MY_DEBUG_OPTIONS}")
ENDIF()

add_compile_options(-pthread -fno-exceptions -Wall 
  "$<$<CONFIG:Debug>:${MY_DEBUG_OPTIONS}>"
  "$<$<CONFIG:Release>:${MY_RELEASE_OPTIONS}>")

set(CMAKE_PREFIX_PATH "usr/local")
find_library(GTEST_LIBRARY gtest)

add_subdirectory(util)
add_subdirectory(raw-to-depth-cpp)
add_subdirectory(raw-to-depth-cpp-tests)
add_subdirectory(net-pipeline)
add_subdirectory(front-end-cpp)