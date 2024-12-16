# @file CMakeLists.txt
# @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.

add_executable(raw-to-depth-tests RawToDepthTests.cpp RawToDepthUtil.cpp raw-to-depth-tests.cpp adjust-md-timestamp-tests.cpp)
# target_sources(raw-to-depth-tests PRIVATE "$<IF:$<BOOL:${OpenCL_FOUND}>,raw-to-depth-gpu-tests.cpp,raw-to-depth-cpu-tests.cpp>")
target_link_libraries(raw-to-depth-tests gtest gmock pthread rawtodepth lumoutil)
