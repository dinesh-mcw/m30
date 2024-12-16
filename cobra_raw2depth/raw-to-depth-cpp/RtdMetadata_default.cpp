/**
 * @file RtdMetadata.cpp
 * @brief For default constructors for RawToDepth objects, this 
 * defines a valid default set of data.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "RtdMetadata.h"

#include <vector>
#include <cstdint>

constexpr uint16_t ignore { 0 };

// Note: all entries in RtdMetadata are << 4U, just like the GPixel data.
const std::vector<uint16_t> RtdMetadata::DEFAULT_METADATA =
  {
   0U     << 4U, //sensorMode; // 0:dmfd
   
   0U     << 4U, //roiStartRow;
   480U   << 4U, //roiNumRows;

   8U     << 4U, //f0ModulationIndex;
   7U     << 4U, //f1ModulationIndex;
   1U     << 4U, //nPulseF0;
   1U     << 4U, //nPulseF1;
   1U     << 4U, //inteBurstLenF0;
   1U     << 4U, //inteBurstLenF1;
   89U    << 4U, //roiId;

   ignore << 4U, //ignore_10
   ignore << 4U, //ignore_11
   ignore << 4U, //ignore_12
   ignore << 4U, //ignore_13

   1U     << 4U, //active_stream_bitmask
   0x3U   << 4U, // start_stop_buffered_image 0
   0x0U   << 4U, // start_stop_buffered_image 1
   0x0U   << 4U, // start_stop_buffered_image 2
   0x0U   << 4U, // start_stop_buffered_image 3
   0x0U   << 4U, // start_stop_buffered_image 4
   0x0U   << 4U, // start_stop_buffered_image 5
   0x0U   << 4U, // start_stop_buffered_image 6
   0x0U   << 4U, // start_stop_buffered_image 7

   0U     << 4U, //roiCounter;
   
   0x12U  << 4U, //timestamp0;
   0x34U  << 4U, //timestamp1;
   0x56U  << 4U, //timestamp2;
   0x78U  << 4U, //timestamp3;
   0x9aU  << 4U, //timestamp4;
   0xbcU  << 4U, //timestamp5;
   0xdeU  << 4U, //timestamp6;


   0U      << 4U, // adc0
   0U      << 4U, // adc1
   0U      << 4U, // adc2
   0U      << 4U, // adc3
   0U      << 4U, // adc4
   0U      << 4U, // adc5
   0U      << 4U, // adc6
   0U      << 4U, // adc7
   0U      << 4U, // adc8

   ignore << 4U, // ignore_40 
   ignore << 4U, // ignore_41
   ignore << 4U, // ignore_42 
   ignore << 4U, // ignore_43 
   ignore << 4U, // ignore_44 
   ignore << 4U, // ignore_45 
   ignore << 4U, // ignore_46 
   ignore << 4U, // ignore_47 

   0U      << 4U, //disableStreaming
   0U      << 4U, //reduceMode;

   1792U  << 4U, //sensorId;

   ignore << 4U, //ignore_51;
   ignore << 4U, //ignore_52
   ignore << 4U, //ignore_53

   0xfffU << 4U, //54: saturation_threshold, 0xfff disables HDR.
   0U     << 4U, //55: system_type
   0U     << 4U, //56: rx_pcb_type
   0U     << 4U, //57: tx_pcb_type
   0U     << 4U, //58: lcm_type

   3915U  << 4U, //range_cal_offset_mm_lo_0807; //59 s10.5
   15U    << 4U, //range_cal_offset_mm_hi_0807; //60 s10.5
   212U   << 4U, //range_cal_mm_per_volt_lo_0807; //61 u9.7
   0U     << 4U, //range_cal_mm_per_volt_hi_0807; //62 u9.7
   500U   << 4U, //range_cal_mm_per_celsius_lo_0807; //63 u9.7
   0U     << 4U, //range_cal_mm_per_celsius_hi_0807; //64 u9.7

   486U   << 4U, // range_cal_offset_mm_lo_0908; //65 s10.5
   0U     << 4U, // range_cal_offset_mm_hi_0908; //66 s10.5
   557U   << 4U, // range_cal_mm_per_volt_lo_0908; //67 u9.7
   0U     << 4U, // range_cal_mm_per_volt_hi_0908; //68 u9.7
   86U     << 4U, // range_cal_mm_per_celsius_lo_0908; //69 u9.7
   0U     << 4U, // range_cal_mm_per_celsius_hi_0908; //70 u9.7
   402U   << 4U, // adc_cal_gain; //71 Gain value of the runtime fpga adc calibration
   845U   << 4U, // adc_cal_offset; //72 Offset value of the runtime fpga adc calibration
   0U     << 4U, // random_scan_table_flag //73

   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 10 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 20 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 30 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 40 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 50 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 60 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 70 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 80 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 90 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 100 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 110 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U, // 120 total of 200-74 = 126
   0U,0U,0U,0U,0U,0U,// 126 total of 200-74 = 126

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries

   0x0bfU << 4U, // userTag
   1U     << 4U, // binMode
   1U     << 4U, // nearest_neighbor_level
   0U     << 4U, // fovRowStart
   480U   << 4U, // fovNumRows
   1U     << 4U, // fovNumRois
   0U     << 4U, 
   0U     << 4U, // snrThreshold
   0U     << 4U, // rtdFiltering
   0x00fU << 4U, // transmitted_data_bitmask
   0U     << 4U, // random_fov_tag
   0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,// 32 total per-fov entries


  };
