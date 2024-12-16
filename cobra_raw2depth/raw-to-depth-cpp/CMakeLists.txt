# @file CMakeLists.txt
# @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.

add_library(rawtodepth STATIC RawToFovs.cpp RawToDepth.cpp RtdMetadata.cpp NearestNeighbor.cpp MappingTable.cpp)
target_sources(rawtodepth PRIVATE RawToDepthDsp.cpp RtdMetadata_default.cpp GPixel.cpp hdr.cpp hdr_float.cpp RawToDepthStripe_float.cpp RawToDepthCommon.cpp)

if (OpenCL_FOUND)
  add_subdirectory(opencl)
  target_link_libraries(rawtodepth rawtodepth_opencl ${OpenCL_LIBRARIES})
endif(OpenCL_FOUND)

if (NOT OpenCL_FOUND)
target_sources(rawtodepth PRIVATE processRoi_float.cpp)
target_sources(rawtodepth PRIVATE processWholeFrame_float.cpp)
target_sources(rawtodepth PRIVATE smoothSummedData_float.cpp)
target_sources(rawtodepth PRIVATE smoothRaw5x7_float.cpp)
target_sources(rawtodepth PRIVATE smoothRaw7x15_float.cpp)
target_sources(rawtodepth PRIVATE calculatePhaseSmooth_float.cpp)
target_sources(rawtodepth PRIVATE computeWholeFrameRange_float.cpp)
target_sources(rawtodepth PRIVATE medianFilterPlus_float.cpp)
target_sources(rawtodepth PRIVATE binning_float.cpp)
target_sources(rawtodepth PRIVATE calculatePhase_float.cpp)
target_sources(rawtodepth PRIVATE RawToDepthGetters_float.cpp)
target_sources(rawtodepth PRIVATE RawToDepthV2_float.cpp)
target_sources(rawtodepth PRIVATE RawToDepthFactory_float.cpp)
endif()

target_include_directories(rawtodepth PUBLIC ../util ${CMAKE_CURRENT_SOURCE_DIR})
