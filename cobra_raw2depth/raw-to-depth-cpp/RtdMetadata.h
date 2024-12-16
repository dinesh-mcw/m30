/**
 * @file RtdMetadata.h
 * @brief Interprets the metadata from an M20/M25/M30 scanhead. The metadata is passed to the RawToDepth
 * code via an extra video line (640*3 shorts) at the beginning of each ROI.
 * 
 * The metadata values are 12-bit values <<4 in each uint16_t input value.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */


#pragma once
#include "GPixel.h"
#include <LumoLogger.h>
#include <vector>
#include <deque>
#include <cstdint>
#include <memory>
#include <math.h>

constexpr uint16_t MD_SHIFT { 4U }; ///< The right-shift required to retrieve each metadata word.
constexpr uint16_t MD_BITS { 12U };  ///< The number of bits in each metadata word.
constexpr float_t C_MPS { 299792498.0F };

constexpr uint32_t NUM_GPIXEL_PHASES { 3 }; ///< iTOF components per pixel
constexpr uint32_t NUM_GPIXEL_PERMUTATIONS { 3 }; ///< iTOF rotations for hardware variability correction. These raw data points are summed during tap rotation.
constexpr uint32_t NUM_GPIXEL_FREQUENCIES { 2 }; ///< iTOF number of frequencies used for modulation.
constexpr uint32_t RAW_PIXEL_MASK { 0xfffc }; ///< Mask used to capture the active bits of a raw data word.
constexpr uint32_t INPUT_RAW_SHIFT { 1 };   ///< All raw input data is arbitrarily shifted right by this value prior to processing
constexpr float_t INPUT_RAW_SCALE { 0.5F }; ///< All raw input data is arbitrarily scaled by this value by RawToDepth prior to processing 
constexpr float_t FPGA_DATA_SCALE { 2.0F }; ///< All raw data is scaled by the value by the FPGA prior to transmission to RawToDepth
constexpr float_t MAX_RAW_VALUE { 4094.0F * float_t(NUM_GPIXEL_PERMUTATIONS) * float_t(INPUT_RAW_SCALE) * float_t(FPGA_DATA_SCALE ) }; ///< This is maximum raw data value that can be received during processing. Any value matching this indicates saturation.

constexpr uint32_t IMAGE_WIDTH { 640 };      ///< Unbinned image width (sensor size)
constexpr uint32_t MAX_IMAGE_HEIGHT { 480 }; ///< Unbinned image height (sensor size)
constexpr uint32_t MAX_ROI_HEIGHT { 20 };    ///< Maximum height of a supported ROI.
constexpr uint32_t MD_ROW_BYTES {IMAGE_WIDTH*NUM_GPIXEL_PHASES*uint32_t(sizeof(uint16_t))}; ///< Number of bytes in the metadata row.
constexpr uint32_t MD_ROW_SHORTS { IMAGE_WIDTH*NUM_GPIXEL_PHASES }; ///< Number of uint16_t values in the metadata row
constexpr float_t RANGE_LIMIT_FRACTION { 0.8F };  ///< All range values above this fraction of max unambiguous range are set to invalid.
constexpr float_t RANGE_NETWORK_SCALE { 1024.0F };///< uint16_t outputs on the network stream are scaled to 1/RANGE_NETWORK_SCALE meters per step.
constexpr uint32_t NUM_ADC_CHANNELS { 9 };

constexpr uint16_t M10_SENSOR_ID { 0x700U };
constexpr uint32_t ROI_START_COLUMN { 0 }; ///< ROI width is fixed.
constexpr uint32_t ROI_NUM_COLUMNS { 640 }; ///< ROI width is fixed.

constexpr uint16_t START_STOP_FLAG_FIRST_ROI       { 0x01 }; ///< In the startStopFlags array, the first ROI in an FOV is indicated by this bit
constexpr uint16_t START_STOP_FLAG_FRAME_COMPLETED { 0x02 }; ///< In the startStopFlags array, the last ROI in an FOV is indicated by this bit.
constexpr uint16_t START_STOP_FLAG_DUMP_RAW_ROI    { 0xffc };

constexpr uint16_t DISABLE_STREAMING_MASK { 0x001 }; ///< Setting this bit in the metadata disabled streaming to the network

constexpr uint16_t SENSOR_MODE_IMAGE     { 0x002 }; 
constexpr uint16_t SENSOR_MODE_SMFD      { 0x001 };
constexpr uint16_t SENSOR_MODE_DMFD      { 0x000 }; ///< The only mode supported on the iTOF chip is DMFD (dual frequency modulated.)
constexpr uint16_t SENSOR_MODE_HDR_RETRY { 0x01U << 4U }; ///< Setting this bit in sensorMode indicates that this ROI has been reacquired due to saturation
constexpr uint16_t SENSOR_MODE_MASK      { 0x07 };


constexpr uint16_t TRANSMIT_BITMASK_RANGE      { 0x001U };
constexpr uint16_t TRANSMIT_BITMASK_SNR        { 0x002U };
constexpr uint16_t TRANSMIT_BITMASK_BACKGROUND { 0x004U };
constexpr uint16_t TRANSMIT_BITMASK_SIGNAL     { 0x008U };
constexpr uint16_t TRANSMIT_BITMASK_INTENSITY  { 0x010U };
constexpr uint16_t TRANSMIT_BITMASK_THETA_PHI  { 0x020U };
constexpr uint16_t TRANSMIT_BITMASK_PHASE01    { 0x040U }; 
constexpr uint16_t TRANSMIT_BITMASK_RAW_IMAGE  { 0x080U };
constexpr uint16_t TRANSMIT_BITMASK_RAW_ROI    { 0x100U };
constexpr uint16_t TRANSMIT_BITMASK_STATUS     { 0x200U };


// Bit definitions common to Grid and Stripe Modes
// Enable stripe mode.
constexpr uint16_t RTD_ALG_COMMON_STRIPE_MODE            { 0x01U<<0U  }; 
// Setting this bit in rtdAlgorithmCommon disables all of the masking that invalidates range pixels based on several criteria (snrThresh, min-max mask, etc.)
constexpr uint16_t RTD_ALG_COMMON_DISABLE_RANGE_MASKING  { 0x01U<<1U  };
// Setting this bit in rtdAlgorithmCommon limits the maximum range to RANGE_LIMIT_FRACTION of the max unambiguous range for this modulation frequency pair.
constexpr uint16_t RTD_ALG_COMMON_ENABLE_MAX_RANGE_LIMIT { 0X01U<<2U  };
// Setting this bit in rtdAlgorithmCommon enables the application of range adjustment based on sensor temperature
constexpr uint16_t RTD_ALG_COMMON_ENABLE_TEMP_RANGE_ADJ  { 0x01U<<3U  };
// Setting this bit in rtdAlgorithmCommon disables the execution of RawToDepth. The code receives the ROI, but does not processing and outputs no results.
constexpr uint16_t RTD_ALG_COMMON_DISABLE_RTD            { 0X01U<<11U };

// Bit definitions for Grid Mode.
// Setting this bit in rtdAlgorithmGrid disables artifact reduction due to smoothing
constexpr uint16_t RTD_ALG_GRID_DISABLE_CONVOLUTION      { 0x01U<<0U  }; 
// Setting this bit in rtdAlgorithmCommon disables the plus-shaped median filter used to mitigate ghosting artifacts.
constexpr uint16_t RTD_ALG_GRID_ENABLE_RANGE_MEDIAN      { 0x01U<<1U  }; 
// Setting this bit in rtdAlgorithmCommon disables the min-max filter applied to the intermediate computation of M
constexpr uint16_t RTD_ALG_GRID_ENABLE_MIN_MAX           { 0x01U<<2U  };

// Bit definitions for Stripe Mode.
constexpr uint16_t RTD_ALG_STRIPE_SNR_WEIGHTED_SUM       { 0x01U<<0U };
constexpr uint16_t RTD_ALG_STRIPE_RECT_SUM               { 0x01U<<1U };
constexpr uint16_t RTD_ALG_STRIPE_GAUSSIAN_SUM           { 0x01U<<2U };
constexpr uint16_t RTD_ALG_STRIPE_ENABLE_RANGE_MEDIAN    { 0x01U<<3U };
constexpr uint16_t RTD_ALG_STRIPE_ENABLE_MIN_MAX         { 0x01U<<4U };

constexpr uint16_t REDUCE_MODE_RTD { 0 }; ///< reduceMode indicates that RawToDepth needs to perform tap rotation sum.
constexpr uint16_t REDUCE_MODE_FGPA { 1 }; ///< reduceMode indicates that the FPGA has performed tap rotation.

constexpr uint16_t SATURATION_THRESHOLD { 4095 };  ///< Indicates that value of saturationThreshold that disables HDR.
constexpr uint16_t MAX_NEAREST_NEIGHBOR_IDX { 5 }; ///< The maximum setting for the nearest neighbor filter.

constexpr uint16_t SYSTEM_TYPE_UNSPECIFIED { 0 };
constexpr uint16_t SYSTEM_TYPE_M20 { 1 };
constexpr uint16_t SYSTEM_TYPE_M25 { 2 };
constexpr uint16_t SYSTEM_TYPE_M30 { 3 };

constexpr uint16_t MAX_ACTIVE_FOVS { 8 }; ///< The maximum number of FOVs supportable by the RawToDepth software.

// When specifying modulation index, the higher index cannot be lower than this (Supported modulation frequency pairs is 9,8 and 8,7)
constexpr uint16_t MIN_MOD_IDX { 8 }; 
constexpr uint16_t MOD_IDX_8   { 8 };
constexpr uint16_t MOD_IDX_9   { 9 };
// When specifying modulation index, the higher index cannot be higher than this (Supported modulation frequency pairs is 9,8 and 8,7)
constexpr uint16_t MAX_MOD_IDX { 9 };

constexpr uint16_t PER_FOV_METADATA_OFFSET { 200 }; // The number of uint16_t past the beginning of the metadata starts the per-fov data.
constexpr uint16_t NUMBER_OF_METADATA_ELEMENTS { 74 };

/**
 * @brief This data structure indicates input values that apply to each of the (max 8) FOVs.
 * Each input ROI can be processed into any (or multiple) of the output FOVs.
 * This is also known as "virtual sensors," and each FOV has independent processing 
 * parameters defined as follows
 * 
 */
struct PerFovMetadata_t {
  uint16_t userTag; ///< An arbitrary 12-bit value set by the System Control software for this ROI.
  uint16_t binMode; ///< Binning mode is either 1, 2, or 4
  uint16_t nearestNeighborLevel; ///< nearestNeighbor filter level, valid values are 0-5
  uint16_t fovRowStart; ///< The row on the sensor which defines the top row of this FOV.
  uint16_t fovNumRows;  ///< The number of rows in the FOV that this ROI is applied to.
  uint16_t fovNumRois; ///< The number of ROIs used to generate the current FOV
  uint16_t rtdAlgorithmCommon; ///< Controls various processing options
  uint16_t snrThresh; ///< The SNR below which output range values are invalidated
  uint16_t unused_08; 
  uint16_t unused_09;
  uint16_t randomFovTag; ///< This tag is unique for each new FOV that arrives.
  uint16_t rtdAlgorithmGrid;
  uint16_t rtdAlgorithmStripe;
  uint16_t unused_13;
  uint16_t unused_14;
  uint16_t unused_15;
  uint16_t unused_16;
  uint16_t unused_17;
  uint16_t unused_18;
  uint16_t unused_19;
  uint16_t unused_20;
  uint16_t unused_21;
  uint16_t unused_22;
  uint16_t unused_23;
  uint16_t unused_24;
  uint16_t unused_25;
  uint16_t unused_26;
  uint16_t unused_27;
  uint16_t unused_28;
  uint16_t unused_29;
  uint16_t unused_30;
  uint16_t unused_31;
};

/**
 * @brief Each ROI received from the driver contains a (possibly) unique 
 * line of metadata. The 640*3 uint16_t values in the metadata block are defined as follows.
 * Since each ROI contains independent set of metadata, these values apply
 * uniquely to this ROI.
 * 
 */
struct Metadata_t
{
  uint16_t sensorMode; ///< 0:dmfd, 1: smfd, 2:image. Only DMFD is supported.
  
  uint16_t roiStartRow; ///< On the sensor, which row is the first in this ROI
  uint16_t roiNumRows;  ///< How many rows is contained within this ROI
  
  uint16_t f0ModulationIndex; ///< an iTOF modulation parameter
  uint16_t f1ModulationIndex; ///< an iTOF modulation parameter
  uint16_t nPulseF0; ///< an iTOF modulation parameter
  uint16_t nPulseF1; ///< an iTOF modulation parameter
  uint16_t inteBurstLenF0; ///< an iTOF modulation parameter
  uint16_t inteBurstLenF1; ///< an iTOF modulation parameter
  uint16_t roiId; ///< A value unique to this ROI

  uint16_t ignore_10;
  uint16_t ignore_11;
  uint16_t ignore_12;
  uint16_t ignore_13;
  uint16_t activeStreamBitmask; ///< Indicates which output FOVs this ROI is processed into
  uint16_t startStopFlags[MAX_ACTIVE_FOVS]; /*!< "start_stop_buffered_image" 0: normal ROI, 1: First ROI in an fov, 2: Last ROI in an fov. */ //NOLINT(hicpp-avoid-c-arrays)

  uint16_t roiCounter; ///< Monotonically increasing counter for each ROI
  
  uint16_t timestamp0; //24
  uint16_t timestamp1; //25
  uint16_t timestamp2; //26
  uint16_t timestamp3; //27
  uint16_t timestamp4; //28
  uint16_t timestamp5; //29
  uint16_t timestamp6; //30

  uint16_t adc[NUM_ADC_CHANNELS]; /*!< The 8 ADC channels on the sensor head. Varies per design. */ //NOLINT(hicpp-avoid-c-arrays)
  // End of dynamic section.

  uint16_t ignore_40;
  uint16_t ignore_41;
  uint16_t ignore_42;
  uint16_t ignore_43;
  uint16_t ignore_44;
  uint16_t ignore_45;
  uint16_t ignore_46;
  uint16_t ignore_47;

  // Static section starts at pixel 48
  uint16_t disableStreaming; ///< "raw2depth_output": Setting the DISABLE_STREAMING_MASK bit prevents RawToDepth from sending data through the network
  uint16_t reduceMode; ///< 0: RTD does tap summation, 1: FPGA does tap summation

  uint16_t sensorId;
  uint16_t ignore_51; // test_mode, indicates that the FPGA is generating test patterns.
  uint16_t ignore_52; // 2023-03-08 quant_mode fixed at zero.
  uint16_t ignore_53; // 2023-03-08 raw_mode is now only 16-bit pixels.
  uint16_t saturationThreshold; ///< The level at which pixels are considered saturated for HDR.
  
  uint16_t system_type; ///< 1:M20, 2:M25
  uint16_t rx_pcb_type; ///< 1:A, 2:B
  uint16_t tx_pcb_type; ///< 1:A, 2:B
  uint16_t lcm_type;    ///< 1:Delta, 2:Tango

  uint16_t range_cal_offset_mm_lo_0807;   //59 s10.5
  uint16_t range_cal_offset_mm_hi_0807;   //60 s10.5
  uint16_t range_cal_mm_per_volt_lo_0807; //61 s3.12
  uint16_t range_cal_mm_per_volt_hi_0807; //62 s3.12
  uint16_t range_cal_mm_per_celsius_lo_0807; //63 u9.7
  uint16_t range_cal_mm_per_celsius_hi_0807; //64 u9.7

  uint16_t range_cal_offset_mm_lo_0908;   //65 s10.5
  uint16_t range_cal_offset_mm_hi_0908;   //66 s10.5
  uint16_t range_cal_mm_per_volt_lo_0908; //67 s3.12
  uint16_t range_cal_mm_per_volt_hi_0908; //68 s3.12
  uint16_t range_cal_mm_per_celsius_lo_0908; //69 u9.7
  uint16_t range_cal_mm_per_celsius_hi_0908; //70 u9.7

  uint16_t adc_cal_gain; //71 Gain value of the runtime fpga adc calibration
  uint16_t adc_cal_offset; //72 Offset value of the runtime fpga adc calibration
  uint16_t random_scan_table_tag; ///< Reset to a new value whenever the scan table is changed.

  uint16_t ignore_74_99[PER_FOV_METADATA_OFFSET-NUMBER_OF_METADATA_ELEMENTS]; // perFovMetadata starts at offset 200. NOLINT(hicpp-avoid-c-arrays)

  PerFovMetadata_t perFovMetadata[MAX_ACTIVE_FOVS]; /*!< The array of metadata applicable to each FOV */ //NOLINT(hicpp-avoid-c-arrays)
};

#define getmd(a) uint16_t((reinterpret_cast<const Metadata_t*>(_metadataPtr))->a>>MD_SHIFT)
#define getsmd(a, idx) uint16_t((reinterpret_cast<const Metadata_t*>(_metadataPtr))->perFovMetadata[idx].a>>MD_SHIFT)
#define getmda(a, idx) uint16_t((reinterpret_cast<const Metadata_t*>(_metadataPtr))->a[idx]>>MD_SHIFT)
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>

/**
 * @brief RtdMetadata class contains the routines necessary for the computation of relevant parameters as 
 * well as getters for the metadata values.
 * 
 */
class RtdMetadata {
private:
  const uint16_t              *_metadataPtr;
  const std::vector<uint16_t> _metadataBlock;
  
public:
  RtdMetadata(const uint16_t *rawData, uint32_t numBytes);
  explicit RtdMetadata(const std::vector<uint16_t> &rawData);
  RtdMetadata() = delete;
  
  // Convert the 12-bit input data to signed 8-bit value
  static int8_t  s8(uint16_t val) { int8_t retVal = *(reinterpret_cast<int8_t*>(&val)); return retVal; }
  // Convert the 12-bit input data to signed 16-bit value
  static int16_t s16(uint16_t valA, uint16_t valB);

  bool getStripeModeEnabled(uint32_t fovIdx) const { return 0 != (getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_STRIPE_MODE); }
  bool getGridModeEnabled(uint32_t fovIdx) const { return 0 == (getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_STRIPE_MODE); }

  float_t getRangeCalOffsetMm(int modIdx);
  float_t getRangeCalMmPerVolt(int modIdx);
  float_t getRangeCalMmPerCelsius(int modIdx);
  
  float_t getAdcCalGain();
  float_t getAdcCalOffset();

  bool isM20() { return getmd(system_type) == SYSTEM_TYPE_M20 || getmd(system_type) == SYSTEM_TYPE_UNSPECIFIED; }
  bool isM25() { return getmd(system_type) == SYSTEM_TYPE_M25; }
  bool isM30() { return getmd(system_type) == SYSTEM_TYPE_M30; }
  
  // Returns true if this is the last ROI in an FOV. Returns true unconditionally for stripe mode.
  bool getFrameCompleted(uint32_t fovIdx) const { return getStripeModeEnabled(fovIdx) || (getmda(startStopFlags, fovIdx) & START_STOP_FLAG_FRAME_COMPLETED) != 0; }
  // Returns true if this is the first ROI in an FOV
  bool getFirstRoi(uint32_t fovIdx) const { return getStripeModeEnabled(fovIdx) || (getmda(startStopFlags, fovIdx) & START_STOP_FLAG_FIRST_ROI) != 0; }
  // Returns true if the CPU is to perform tap accumulation
  bool getDoTapAccumulation() const { return REDUCE_MODE_RTD == getmd(reduceMode); }
  bool getDumpRawRoi(uint32_t fovIdx) { return getmda(startStopFlags, fovIdx) & START_STOP_FLAG_DUMP_RAW_ROI; }
  
  bool getDisableStreaming() { return 0 != (getmd(disableStreaming) & DISABLE_STREAMING_MASK); }
  uint16_t getNearestNeighborFilterLevel(uint32_t fovIdx);
  uint16_t getNumPermutations() { return  getmd(reduceMode)==0 ? 3 : 1; } // Tap Accumulations occur on the FPGA.
  
  uint16_t getSensorId() { return getmd(sensorId); }
  uint16_t getSensorMode() { return getmd(sensorMode) & SENSOR_MODE_MASK; }
  uint16_t getReduceMode() const { return getmd(reduceMode); }
  
   // Three uint format for timestamps in which all 94 bits are split between 3 32-bit unsigned ints.
  std::vector<uint32_t> getTimestamps();
  
  // 64-bit timestamp, that is the lower 60 bits of the 7 12-bit metadata values.
  uint64_t getTimestamp() {
    return uint64_t(getmd(timestamp0)) +
      (uint64_t(getmd(timestamp1))<<MD_BITS) +
      (uint64_t(getmd(timestamp2))<<2U*MD_BITS) +
      (uint64_t(getmd(timestamp3))<<3U*MD_BITS) +
      (uint64_t(getmd(timestamp4))<<4U*MD_BITS);
  }
  
  // Returns the row on the sensor that matches the top row of the ROI
  uint16_t getRoiStartRow() { return getmd(roiStartRow); }
  // Returns the column on the sensor that matches the left column of the ROI.
  static uint16_t getRoiStartColumn() { return ROI_START_COLUMN; }
  // Returns the number of rows in this ROI
  uint16_t getRoiNumRows() { return getmd(roiNumRows); }
  // Returns the number of columns in this ROI
  static uint16_t getRoiNumColumns() { return IMAGE_WIDTH; }
  // Returns the number of 16-bit elements in this ROI
  uint32_t getRoiNumElements();
  
  // Returns the modulation index for the lower modulation frequency
  uint16_t getF0ModulationIndex() { return getmd(f0ModulationIndex); }
  // Returns the modulation index for the higher modulation frequency
  uint16_t getF1ModulationIndex() { return getmd(f1ModulationIndex); }
  uint16_t getNPulseF0() { return getmd(nPulseF0); }
  uint16_t getNPulseF1() { return getmd(nPulseF1); }
  uint16_t getInteBurstLenF0() { return getmd(inteBurstLenF0); }
  uint16_t getInteBurstLenF1() { return getmd(inteBurstLenF1); }
  uint16_t getRoiCounter() { return getmd(roiCounter); }
  uint16_t getRoiId() { return getmd(roiId); }
  // Returns the ADC value for one of the ADC on the sensor
  uint16_t getAdc(uint32_t idx) { return getmda(adc, idx); }
  float_t getMaxUnambiguousRange();

  // Returns the width of the output FOV after binning.
  uint16_t getFullImageWidth(uint32_t fovIdx) { return IMAGE_WIDTH/getBinningX(fovIdx); } 
  // Returns the height of the output FOV after binning.
  uint16_t getFullImageHeight(uint32_t fovIdx) { return getsmd(fovNumRows, fovIdx)/getBinningY(fovIdx); }
  // Returns the width of the full FOV before binning (the range of pixels read from the sensor)
  static uint16_t getInputImageWidth(uint32_t fovIdx) { return IMAGE_WIDTH; }
  // Returns the height of the full FOV before binning.
  uint16_t getInputImageHeight(uint32_t fovIdx) { return getsmd(fovNumRows, fovIdx); }
  
  // Returns the number of modulation frequencies (only two modulation frequencies is supported)
  uint16_t getNumModulationFrequencies() {
    switch(getSensorMode()) {
      case SENSOR_MODE_IMAGE:  return 0; // image
      case SENSOR_MODE_SMFD :  return 1; // smfd
      case SENSOR_MODE_DMFD :
      default: return 2; // dmfd
    }
  }
  // Returns the SNR threshold below which any output range value is set to invalid.
  float_t getSnrThresh(uint32_t fovIdx);

  // Per-stream metadata values

  // Returns the bitmask defining all of the FOVs this ROI will be processed into
  uint16_t getActiveFovsBitmask() { return getmd(activeStreamBitmask); }
  // Given an fovIdx, returns true if this FOV will be processed into it.
  bool getIsFovActive(uint32_t fovIdx) { return 0U != (uint32_t(getmd(activeStreamBitmask) >> fovIdx) & 0x01U); }
  // Return a collection containing all of the FOV IDs for active FOVs.
  std::vector<uint16_t> getActiveFovs();
  // Returns the custom-defined user tag
  uint16_t getUserTag(uint32_t fovIdx) { return getsmd(userTag, fovIdx); }
  // returns the metadata value that specifies binning (Only one binning dimension is binning is specified by the metadata)
  uint16_t getBinModeX(uint32_t fovIdx) { return getsmd(binMode, fovIdx); }
  // returns the metadata value that specifies binning (Only one binning dimension is binning is specified by the metadata)
  uint16_t getBinModeY(uint32_t fovIdx) { return getsmd(binMode, fovIdx); } // Only binModX is specified in the metadata
  // Returns the value expected for binning. Returns 1, 2, or 4. X and Y binning are identical.
  uint16_t getBinningX(uint32_t fovIdx) { return getBinModeX(fovIdx)==0 ? 1 : getBinModeX(fovIdx); }
  // Returns the value expected for binning. Returns 1, 2, or 4. X and Y binning are identical
  uint16_t getBinningY(uint32_t fovIdx) { return getBinModeX(fovIdx)==0 ? 1 : getBinModeX(fovIdx); }
  
  // Returns the starting row where the given input FOV starts on the sensor.
  uint16_t getFovStartRow(uint32_t fovIdx) { return getsmd(fovRowStart, fovIdx); }
  // Returns the starting column where the given input FOV starts on the sensor (FOV width is fixed, this is always zero.)
  static uint16_t getFovStartColumn(uint32_t fovIdx) { return ROI_START_COLUMN; }
  // Returns the number of rows in the given input FOV
  uint16_t getFovNumRows(uint32_t fovIdx) { return getsmd(fovNumRows, fovIdx); }
  // Returns the number of columns in the given input FOV (FOV width is fixed at 640)
  static uint16_t getFovNumColumns(uint32_t fovIdx) { return ROI_NUM_COLUMNS; }
  // Returns the number of ROIs that make up the given FOV
  uint16_t getFovNumRois(uint32_t fovIdx) { return getStripeModeEnabled(fovIdx) ? 1 : getsmd(fovNumRois, fovIdx); }
  
  // Returns the start_stop_flags metadata word.
  uint16_t getStartStopFlags(uint32_t fovIdx) { return getmda(startStopFlags, fovIdx); }
  
  // Returns the algorithm enable/disable flags applicable to Grid and Stripe mode.
  uint16_t getRtdAlgorithmCommon(uint32_t fovIdx) { return getsmd(rtdAlgorithmCommon, fovIdx); }
  // Returns the algorithm enable/disable flags applicable to Grid mode.
  uint16_t getRtdAlgorithmGrid(uint32_t fovIdx) { return getsmd(rtdAlgorithmGrid, fovIdx); }
  // Returns the algorithm enable/disable flags applicable to Stripe mode.
  uint16_t getRtdAlgorithmStripe(uint32_t fovIdx) { return getsmd(rtdAlgorithmStripe, fovIdx); }
  // Returns true if phase smoothing is to be disabled.
  bool getDisablePhaseSmoothing(uint32_t fovIdx) { return getsmd(rtdAlgorithmGrid, fovIdx) & RTD_ALG_GRID_DISABLE_CONVOLUTION; }
  // Return true if the median filter, as applied to the output range data, is to be enabled.
  bool getPerformGhostMedianFilter(uint32_t fovIdx) { return getsmd(rtdAlgorithmGrid, fovIdx) & RTD_ALG_GRID_ENABLE_RANGE_MEDIAN; }
  // Return true if the min-max filter (as applied to the intermediate value "M") is to be enabled.
  bool getPerformGhostMinMaxFilter(uint32_t fovIdx) { return getsmd(rtdAlgorithmGrid, fovIdx) & RTD_ALG_GRID_ENABLE_MIN_MAX; }
  // Return true if the range masking is to be disabled. This includes things like snrThresh, minMaxMask, pixelMask, etc.
  bool getDisableRangeMasking(uint32_t fovIdx) { return getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_DISABLE_RANGE_MASKING; }
  // Return true if the output range values are to be adjusted due to the temperature of the sensor.
  bool getEnableRangeTempRangeAdjustment(uint32_t fovIdx) { return getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_ENABLE_TEMP_RANGE_ADJ; }
  // Return true of RawToDepth algorithms should ignore the input data.
  bool getDisableRtd(uint32_t fovIdx) { return getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_DISABLE_RTD; }
  // Return true if the range should be limited to a fraction (RANGE_LIMIT_FRACTION) of the maximum unambiguous range.
  bool getEnableMaxRangeLimit(uint32_t fovIdx) { return getsmd(rtdAlgorithmCommon, fovIdx) & RTD_ALG_COMMON_ENABLE_MAX_RANGE_LIMIT; }

  // Stripe mode. When collapsing the ROI into a single stripe, sum each column weighted by its SNR.
  bool getStripeModeSnrWeightedSum(uint32_t fovIdx) { return (getRtdAlgorithmStripe(fovIdx) & RTD_ALG_STRIPE_SNR_WEIGHTED_SUM) != 0; }
  // Stripe mode. When collapsing the ROI into a single stripe, take the average value of each column
  bool getStripeModeRectSum(uint32_t fovIdx) { return (getRtdAlgorithmStripe(fovIdx) & RTD_ALG_STRIPE_RECT_SUM) != 0; }
  // Stripe mode. When collapsing the ROI into a single stripe, weight each columh by a Gaussian window.
  bool getStripeModeGaussianSum(uint32_t fovIdx) { return (getRtdAlgorithmStripe(fovIdx) & RTD_ALG_STRIPE_GAUSSIAN_SUM) != 0; }
  // Stripe mode. Turn on a 1D median filter on the output range values.
  bool getStripeModeEnableRangeMedian(uint32_t fovIdx) { return (getRtdAlgorithmStripe(fovIdx) & RTD_ALG_STRIPE_ENABLE_MIN_MAX) != 0; }
  // Stripe mode. Turn on the stripe mode version of the min-max filter.
  bool getStripeModeEnabledMinMax(uint32_t fovIdx) { return (getRtdAlgorithmStripe(fovIdx) & RTD_ALG_STRIPE_ENABLE_MIN_MAX) != 0; }
  
  
  // For HDR: return true if this ROI indicates that it has been retaken due to the previous ROI being saturated.
  bool wasPreviousRoiSaturated() const { return 0 != (getmd(sensorMode) & SENSOR_MODE_HDR_RETRY); }
  // For HDR: Return true of the metadata disables saturation by setting the saturation threshold to the maximum allowable pixel value (4095)
  bool isHdrDisabled() const { return SATURATION_THRESHOLD == getmd(saturationThreshold); }
  // For HDR: Returns the saturation threshold used when determining whether an ROI has been saturated.
  uint16_t getSaturationThreshold() const;

  uint16_t getSystemType() { return getmd(system_type); }
  uint16_t getRxPcbType()  { return getmd(rx_pcb_type); }
  uint16_t getTxPcbType()  { return getmd(tx_pcb_type); }
  uint16_t getLcmType()    { return getmd(lcm_type); }
  uint16_t getScanTableTag() { return getmd(random_scan_table_tag); }
  uint16_t getRandomFovTag(uint32_t fovIdx) { return getsmd(randomFovTag, fovIdx); }

  static uint16_t getRawPixelMask() { return RAW_PIXEL_MASK; }
  
  // Prints the metadata for this ROI to stdout
  void printMetadata();
  // Prints the metadata for this ROI to the log
  void logMetadata();
  static const std::vector<uint16_t> DEFAULT_METADATA;  

  // adds the offset (seconds) to the timestamp (mutates the metadata)
  static void adjustTimestamp(uint8_t *ptr, uint64_t offset);
};


