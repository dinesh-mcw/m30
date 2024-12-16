# @file CMakeLists.txt
# @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.

add_library(netpipeline STATIC network_streamer.cpp pipeline_data.cpp pipeline_modules.cpp cobra_net_pipeline.cpp)
target_include_directories(netpipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/raw-to-depth-cpp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(netpipeline pthread rawtodepth)

