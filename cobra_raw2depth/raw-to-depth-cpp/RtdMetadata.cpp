/**
 * @file RtdMetadata.cpp
 * @brief Utility methods and getters for metadata.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "RtdMetadata.h"
#include "GPixel.h"
#include <iostream>
#include <cassert>

/**
 * @brief This constructor takes the pointer (caller still owns the pointer).
 * 
 * @param rawData Actual pointer from the driver
 * @param numBytes Number of bytes in the input buffer
 */
RtdMetadata::RtdMetadata(const uint16_t* rawData, uint32_t numBytes) :
  _metadataPtr(rawData)
{
  assert(numBytes >= sizeof(Metadata_t));
}

/**
 * @brief This constructor takes a copy of the input data.
 * 
 * @param rawData Contains at least the first row of the raw input ROI.
 */
RtdMetadata::RtdMetadata(const std::vector<uint16_t> &rawData) :
  _metadataBlock(rawData.data(), rawData.data()+sizeof(Metadata_t)) // Takes a copy
{
  assert(rawData.size()*sizeof(uint16_t) >= sizeof(Metadata_t)); 
  _metadataPtr = _metadataBlock.data();
}

// 
/**
 * @brief Combine two 12-bit unsigned metadata values into a 16-bit signed integer
 * 
 */
int16_t RtdMetadata::s16(uint16_t valA, uint16_t valB) 
{ 
  constexpr uint16_t mask12 = 0xfffU;
  constexpr uint16_t mask4 = 0xfU;
  constexpr uint32_t nibbleShift = 12U;
  uint16_t tmpC = uint16_t(valA & mask12) | 
                  uint16_t(uint16_t(valB & mask4) << nibbleShift); 
  int16_t retVal = *reinterpret_cast<int16_t*>(&tmpC); 
  return retVal; 
}

std::vector<uint16_t> RtdMetadata::getActiveFovs()
{
  auto activeFovs = std::vector<uint16_t>();
  activeFovs.reserve(MAX_ACTIVE_FOVS);
  for (auto fovIdx=0; fovIdx<MAX_ACTIVE_FOVS; fovIdx++)
  {
    if (getIsFovActive(fovIdx))
    {
      activeFovs.push_back(fovIdx);
    }
  }
  return activeFovs;
}

/**
 * @brief Converts the 7 timestamp input values into 3 32-bit words on the output
 * 
 * @return std::vector<uint32_t> A 3-element timestamp.
 */
std::vector<uint32_t> RtdMetadata::getTimestamps() 
{
  const uint32_t val0 = uint32_t(getmd(timestamp0)) | uint32_t(getmd(timestamp1) << 12U) | uint32_t( (getmd(timestamp2) & 0xffU) << 24U );
  const uint32_t val1 = uint32_t(getmd(timestamp2) >> 8U) | uint32_t(getmd(timestamp3) << 4U) | uint32_t(getmd(timestamp4) << 16U) | uint32_t((getmd(timestamp5) & 0xfU) << 28U);
  const uint32_t val2 = uint32_t(getmd(timestamp5) >> 4U) | uint32_t(getmd(timestamp6) << 8U);
  return std::vector<uint32_t> {val0, val1, val2};
}


float_t RtdMetadata::getSnrThresh(uint32_t fovIdx) 
{ 
  const float_t snrThresh = float(getsmd(snrThresh, fovIdx)) / 8.0F; 
  return snrThresh;
}



/**
 * @brief Computes the number of 16-bit elements in an input ROI
 * 
 */
uint32_t RtdMetadata::getRoiNumElements() 
{ 
  return  MD_ROW_SHORTS + getRoiNumRows() * getRoiNumColumns() * NUM_GPIXEL_PHASES * NUM_GPIXEL_FREQUENCIES * (getDoTapAccumulation() ? NUM_GPIXEL_PERMUTATIONS : 1) ;
}

/**
 * @brief The metadata indicates the nearest neighbor filter level for this FOV.
 * 
 * @param fovIdx The virtual sensor (output FOV) to apply this filter to
 * @return uint16_t The nearest neighbor filter level indicated by the metadata.
 */
uint16_t RtdMetadata::getNearestNeighborFilterLevel(uint32_t fovIdx) 
{ 
  auto val = getDisableRangeMasking(fovIdx) ? 0 : getsmd(nearestNeighborLevel, fovIdx); 
  if (val > MAX_NEAREST_NEIGHBOR_IDX)
  {
    val = MAX_NEAREST_NEIGHBOR_IDX;
  }
  return val;
}

float_t RtdMetadata::getMaxUnambiguousRange() 
{ 
  const float_t mur = 0.5F * C_MPS/(float_t)GPixel::getGcf(getF0ModulationIndex(), getF1ModulationIndex()); 
  return mur; 
}


// Metadata for temperature calibration.

/**
 * @brief Compute the calibration gain for the ADC.
 *        The input is an unsigned 12-bit value with a scale of 2^-19
 * @return float_t The gain required to calculate the ADC values from calibration.
 */
float_t RtdMetadata::getAdcCalGain()
{
  const float_t adcCalGain = float_t(getmd(adc_cal_gain)) * pow(2.0F, -19.0F);
  return adcCalGain;
}

/**
 * @brief Compute the ADC calibration offset.
 *        The input is a signed 12-bit value with a scale of 2^-14
 * 
 * @return float_t The offset required to calculate the ADC values from calibration.
 */
float_t RtdMetadata::getAdcCalOffset()
{
  uint16_t adcCalOffset = getmd(adc_cal_offset) << 4U; // move sign bit into msb of uint16_t
  int16_t adcCalOffset_signed = *(reinterpret_cast<int16_t*>(&adcCalOffset));
  const float_t adcCalOffset_float = float_t(adcCalOffset_signed) * pow(2.0F, -(4.0F + 14.0F)); // extra 4 bits from upshift above.
  return adcCalOffset_float;
}

/**
 * @brief Compute the range cal offset
 * 
 * @param modIdx The higher frequency modulation index
 * @return float_t The offset in mm determined during range calibration.
 */
float_t RtdMetadata::getRangeCalOffsetMm(int modIdx)
{
  uint16_t offsetLo;
  uint16_t offsetHi;
  if (modIdx == MOD_IDX_8)
  {
    offsetLo = getmd(range_cal_offset_mm_lo_0807);
    offsetHi = getmd(range_cal_offset_mm_hi_0807);
  }
  else
  {
    offsetLo = getmd(range_cal_offset_mm_lo_0908);
    offsetHi = getmd(range_cal_offset_mm_hi_0908);
  }

  const float_t offsetMm = float_t(s16(offsetLo,offsetHi))/pow(2.0F,5.0F);
  return offsetMm;
}

/**
 * @brief Compute the scaling value for range cal offset.
 * 
 * The metadata values that hold the mm_per_volt components have a format 
 * of a signed 16-bit fixed-point value with 12 fractional bits.
 * With the lower 12 bits in the "lo" word and the 
 * upper 4-bits in the "hi" word.
 * 
 * @param modIdx The higher frequency modulation index
 * @return float_t The scale of the range cal in mm per volt
 */
float_t RtdMetadata::getRangeCalMmPerVolt(int modIdx)
{
  const float_t mmPerVScale = pow(2.0F, -12.0F);
  uint16_t mmPerVLo;
  uint16_t mmPerVHi;
  if (modIdx == MOD_IDX_8)
  {
    mmPerVLo = getmd(range_cal_mm_per_volt_lo_0807);
    mmPerVHi = getmd(range_cal_mm_per_volt_hi_0807);
  }
  else
  {
    mmPerVLo = getmd(range_cal_mm_per_volt_lo_0908);
    mmPerVHi = getmd(range_cal_mm_per_volt_hi_0908);
  }
  auto mmPerVolt = float_t(s16(mmPerVLo, mmPerVHi)) * mmPerVScale;
  return mmPerVolt;
}

/**
 * @brief Compute the scaling value for range cal offset.
 * 
 * @param modIdx The higher frequency modulation index
 * @return float_t The scale of the range cal in mm per degree C
 */
float_t RtdMetadata::getRangeCalMmPerCelsius(int modIdx)
{
  uint16_t mmPerCLo;
  uint16_t mmPerCHi;
  if (modIdx == MOD_IDX_8)
  {
    mmPerCLo = getmd(range_cal_mm_per_celsius_lo_0807);
    mmPerCHi = getmd(range_cal_mm_per_celsius_hi_0807);
  }
  else
  {
    mmPerCLo = getmd(range_cal_mm_per_celsius_lo_0908);
    mmPerCHi = getmd(range_cal_mm_per_celsius_hi_0908);
  }

  const float mmPerC = float_t( (mmPerCHi & 0xfU)<<12U | (mmPerCLo & 0xfffU))/pow(2.0F, 7.0F);
  return mmPerC;
}

/**
 * @brief Compute the saturation threshold knowing that the RawToDepth code
 * may be responsible for tripling each input value by performing tap rotation
 * on the input data.
 * 
 * @return uint16_t The value of the input data that corresponds to saturation.
 */
uint16_t RtdMetadata::getSaturationThreshold() const {
  auto threshold = (uint32_t)getmd(saturationThreshold); // Upper 12 bits from the metadata field.
  
  // HDR is computed prior to tap rotation on the CPU.
  // If the tap rotation happens on the FPGA, then the raw values have been
  // tripled since the saturation comparison was made. So we need to use 3*
  // the provided threshold value.
  if (!getDoTapAccumulation()) 
  {
    threshold *= 3;
  }
  
  // The raw input data has been scaled by FPGA_DATA_SCALE prior to transmission.
  // This threshold is for the raw data as received by RTD, it doesn't not include
  // RTD scaling.
  threshold *= FPGA_DATA_SCALE;
  return uint16_t(threshold);
}

/**
 * @brief Utility code for printing out the metadata from an ROI.
 * 
 */
#define OUTPUT_METADATA \
{\
  OUTPUT_LINE("M2x Metadata:"); \
  OUTPUT_LINE("sensorMode " << getSensorMode() ); \
  OUTPUT_LINE("timestamp " << getTimestamp() ); \
  OUTPUT_LINE("RoiStartRow " << getRoiStartRow() ); \
  OUTPUT_LINE("RoiNumRows " << getRoiNumRows() ); \
\
  OUTPUT_LINE("F0ModulationIndex " << getF0ModulationIndex() ); \
  OUTPUT_LINE("F1ModulationIndex " << getF1ModulationIndex() ); \
  OUTPUT_LINE("Max unambiguous range " << getMaxUnambiguousRange()); \
  OUTPUT_LINE("NPulseF0 " << getNPulseF0() ); \
  OUTPUT_LINE("NPulseF1 " << getNPulseF1() ); \
  OUTPUT_LINE("InteBurstLenF0 " << getInteBurstLenF0() ); \
  OUTPUT_LINE("InteBurstLenF1 " << getInteBurstLenF1() ); \
\
  OUTPUT_LINE("RoiCounter " << getRoiCounter() ); \
  OUTPUT_LINE("RoiId " << getRoiId() ); \
\
  for (uint32_t idx=0; idx<MAX_ACTIVE_FOVS; idx++) \
  { \
    OUTPUT_LINE("\tstart_stop_buffered_image[" << idx << "] " << getStartStopFlags(idx)); \
  } \
\
  OUTPUT_LINE("ActiveBitStreamBitmask " << getActiveFovsBitmask()); \
  OUTPUT_LINE("Disable Streaming " << getDisableStreaming()); \
\
  OUTPUT_LINE("sensorId 0x" << std::hex << getSensorId() ); \
  OUTPUT_LINE("reduceMode " << getReduceMode() ); \
  OUTPUT_LINE("metadata saturation threshold " << (int)getmd(saturationThreshold)); \
  OUTPUT_LINE("wasPreviousRoiSaturated (hdr retry) " << (int)wasPreviousRoiSaturated()); \
  OUTPUT_LINE("systemType " << (int)getSystemType() ); \
  OUTPUT_LINE("rxPcbType " << (int)getRxPcbType() ); \
  OUTPUT_LINE("txPcbType " << (int)getTxPcbType() ); \
  OUTPUT_LINE("lcmType " << (int)getLcmType() );  \
  \
  std::ostringstream adcStream;\
  adcStream << "ADC values: ";\
  for (auto idx=0; idx<8; idx++)\
  {\
    adcStream << std::setw(5) << getmda(adc, idx);\
  }\
  OUTPUT_LINE(adcStream.str());\
\
  for (auto idx : getActiveFovs()) { \
    OUTPUT_LINE("stream " << idx << " is enabled."); \
    OUTPUT_LINE("\tstripe mode " << (getStripeModeEnabled(idx) ? "is" : "is not") << " enabled."); \
    OUTPUT_LINE("\tdisableRtd " << getDisableRtd(idx)); \
    OUTPUT_LINE("\tfirst ROI " << getFirstRoi(idx)); \
    OUTPUT_LINE("\tlast ROI " << getFrameCompleted(idx)); \
    OUTPUT_LINE("\tuser_tag 0x" << std::hex << getUserTag(idx)); \
    OUTPUT_LINE("\tbinModeX " << getBinModeX(idx)); \
    OUTPUT_LINE("\tbinModeY " << getBinModeY(idx)); \
    OUTPUT_LINE("\tbinningX " << getBinningX(idx)); \
    OUTPUT_LINE("\tbinningY " <<  getBinningY(idx)); \
\
    OUTPUT_LINE("\tfovStartRow " << getFovStartRow(idx)); \
    OUTPUT_LINE("\tfovNumRows " << getFovNumRows(idx)); \
    OUTPUT_LINE("\tfovNumRois " << getFovNumRois(idx)); \
    OUTPUT_LINE("\tfovStartColumn " << getFovStartColumn(idx)); \
    OUTPUT_LINE("\tfovNumColumns " << getFovNumColumns(idx)); \
    OUTPUT_LINE("\tSNR Threshold " << getSnrThresh(idx)); \
    OUTPUT_LINE("\tNN Filter " << getNearestNeighborFilterLevel(idx)); \
    OUTPUT_LINE("\tdumpetyDump " << (int)getDumpRawRoi(idx)); \
\
    OUTPUT_LINE("\tRTD Alg Common 0x" << std::hex << getRtdAlgorithmCommon(idx)); \
    OUTPUT_LINE("\tRTD Alg Grid 0x" << std::hex << getRtdAlgorithmGrid(idx)); \
    OUTPUT_LINE("\tRTD Alg Stripe 0x" << std::hex << getRtdAlgorithmStripe(idx)); \
    OUTPUT_LINE("\tRTD Alg Disable Convolution " << getDisablePhaseSmoothing(idx)); \
    OUTPUT_LINE("\tRTD Alg Ghost Min Max " << getPerformGhostMinMaxFilter(idx)); \
    OUTPUT_LINE("\tRTD Alg Perform Ghost Median " << getPerformGhostMedianFilter(idx)); \
    OUTPUT_LINE("\tRTD Alg Enable Range Adjustment " << getEnableRangeTempRangeAdjustment(idx)); \
    OUTPUT_LINE("\tRTD Alg Disable Range Masking " << (int)getDisableRangeMasking(idx)); \
    OUTPUT_LINE("\tRTD Alg Stripe Rect Window " << (int)getStripeModeRectSum(idx)); \
    OUTPUT_LINE("\tRTD Alg Stripe Gaussian Window " << (int)getStripeModeGaussianSum(idx)); \
    OUTPUT_LINE("\tRTD Alg Stripe SNR-weighted Window " << (int)getStripeModeSnrWeightedSum(idx)); \
    OUTPUT_LINE("\tstartStopFlags 0x" << std::hex << getStartStopFlags(idx)); \
  }\
} 

#define OUTPUT_LINE(message) std::cout << message << std::endl; /* NOLINT(bugprone-macro-parentheses) can't enclose a stream expression in parens */

void RtdMetadata::printMetadata()
{
OUTPUT_METADATA
}

#undef OUTPUT_LINE
#define OUTPUT_LINE(message) LLogInfo(message) 

void RtdMetadata::logMetadata()
{
OUTPUT_METADATA
}

/**
 * @brief Adjust the metadata time if the time offset is nonzero
 *
 * param ptr Pointer to metadata
 * param offset Time offset in seconds to convert FPGA time to UTC
 */
//   15   14   13   12   11   10    9    8    7    6    5    4    3    2    1    0
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |ns11|ns10|ns09|ns08|ns07|ns06|ns05|ns04|ns03|ns02|ns01|ns00| X  | X  | X  | X  | timestamp0
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |ns23|ns22|ns21|ns20|ns19|ns18|ns17|ns16|ns15|ns14|ns13|ns12| X  | X  | X  | X  | timestamp1
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |s03 |s02 |s01 |s00 |ns31|ns30|ns29|ns28|ns27|ns26|ns25|ns24| X  | X  | X  | X  | timestamp2
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |s15 |s14 |s13 |s12 |s11 |s10 |s09 |s08 |s07 |s06 |s05 |s04 | X  | X  | X  | X  | timestamp3
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |s27 |s26 |s25 |s24 |s23 |s22 |s21 |s20 |s19 |s18 |s17 |s16 | X  | X  | X  | X  | timestamp4
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |s39 |s38 |s37 |s36 |s35 |s34 |s33 |s32 |s31 |s30 |s29 |s28 | X  | X  | X  | X  | timestamp5
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
// |s51 |s50 |s49 |s48 |s47 |s46 |s45 |s44 |s43 |s42 |s41 |s40 | X  | X  | X  | X  | timestamp6
// +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+

void RtdMetadata::adjustTimestamp(uint8_t *ptr, uint64_t offset) {
    auto *mdp = (struct Metadata_t *)ptr;

    if (offset != 0) {
        // NOLINTBEGIN(readability-magic-numbers)  Magic numbers needed for format conversion
        uint64_t secs = (((uint64_t) mdp->timestamp2) >> 12ULL) |
                        (((uint64_t) mdp->timestamp3) & 0xfff0ULL) |
                        ((((uint64_t)mdp->timestamp4) & 0xfff0ULL) << 12ULL) |
                        ((((uint64_t)mdp->timestamp5) & 0xfff0ULL) << 24ULL) |
                        ((((uint64_t)mdp->timestamp6) & 0xfff0ULL) << 36ULL);
        // NOLINTEND(readability-magic-numbers)

        secs += offset;

        // NOLINTBEGIN(readability-magic-numbers)  Magic numbers needed for format conversion
        mdp->timestamp2 = (uint16_t)((uint64_t)(mdp->timestamp2 & 0xff0ULL)) | ((secs & 0xfULL) << 12ULL);
        mdp->timestamp3 = (uint16_t) (secs & 0x000000000000fff0ULL);
        mdp->timestamp4 = (uint16_t)((secs & 0x000000000fff0000ULL) >> 12ULL);
        mdp->timestamp5 = (uint16_t)((secs & 0x000000fff0000000ULL) >> 24ULL);
        mdp->timestamp6 = (uint16_t)((secs & 0x000fff0000000000ULL) >> 36ULL);
        // NOLINTEND(readability-magic-numbers)
    }
}

