'''
file: M30Metadata.py

A python implementation of the code to interpret the metadata coming from the 
M30 sensor, in the first line of the raw ROI.

Copyright 2023 (C) Lumotive, Inc. All rights reserved.
'''
import numpy as np
from pathlib import Path
import struct

IDX_TO_FRQ_LUT = np.array([ 0, 0, 0, 1.0e9/(3.0*3.0), 1.0e9/(3.0*4.0), 1.0e9/(3.0*5.0), 1.0e9/(3.0*6.0), 1.0e9/(3.0*7.0), 1.0e9/(3.0*8.0), 
                  1.0e9/(3.0*9.0), 1.0e9/(3.0*10.0)], dtype=np.float32)

GCF34 = np.float32(1.0e9)/np.float32(3.0*12.0)
GCF45 = np.float32(1.0e9)/np.float32(3.0*20.0)
GCF56 = np.float32(1.0e9)/np.float32(3.0*30.0)
GCF67 = np.float32(1.0e9)/np.float32(3.0*42.0)
GCF78 = np.float32(1.0e9)/np.float32(3.0*56.0)
GCF89 = np.float32(1.0e9)/np.float32(3.0*72.0)
GCF90 = np.float32(1.0e9)/np.float32(3.0*90.0)

GCFS = np.array([0,0,0, GCF34, GCF45, GCF56, GCF67, GCF78, GCF89, GCF90], dtype=np.float32)

RTD_ALG_COMMON_STRIPE_MODE            =( 0x01<< 0  ) 
RTD_ALG_COMMON_DISABLE_RANGE_MASKING  =( 0x01<< 1  )
RTD_ALG_COMMON_ENABLE_MAX_RANGE_LIMIT =( 0X01<< 2  )
RTD_ALG_COMMON_ENABLE_TEMP_RANGE_ADJ  =( 0x01<< 3  )
RTD_ALG_COMMON_DISABLE_RTD            =( 0X01<<11  )

RTD_ALG_GRID_DISABLE_CONVOLUTION      =( 0x01<< 0  ) 
RTD_ALG_GRID_ENABLE_RANGE_MEDIAN      =( 0x01<< 1  ) 
RTD_ALG_GRID_ENABLE_GHOST_MIN_MAX     =( 0x01<< 2  )

RTD_ALG_STRIPE_SNR_WEIGHTED_SUM       =( 0x01<<0   )
RTD_ALG_STRIPE_RECT_SUM               =( 0x01<<1   )
RTD_ALG_STRIPE_GAUSSIAN_SUM           =( 0x01<<2   )
RTD_ALG_STRIPE_ENABLE_RANGE_MEDIAN    =( 0x01<<3   )
RTD_ALG_STRIPE_ENABLE_MIN_MAX         =( 0x01<<4   )

NUM_GPIXEL_PHASES = 3
NUM_GPIXEL_PERMUTATIONS = 3
NUM_GPIXEL_FREQUENCIES = 2
MAX_ACTIVE_FOVS = 8
INPUT_RAW_SHIFT = 1
SATURATION_THRESHOLD = 4095
SNR_SCALING_FACTOR = np.float32(8.0)
RAW_SCALING_FACTOR = np.float32(8.0)
FPGA_DATA_SCALE = np.float32(2.0)
RANGE_LIMIT_FRACTION = np.float32(0.8)
STRIPE_DEFAULT_GAUSSIAN_STD = np.float32(2.0)

MD_SHIFT_BY = 4
MD_ROW_SHORTS = 640*3
IMAGE_WIDTH = 640
ROI_NUM_COLUMNS = 640
ROI_START_COLUMN = 0

START_STOP_FLAG_FIRST_ROI = 1
START_STOP_FLAG_FRAME_COMPLETED = 2

SENSOR_MODE_HDR_RETRY = (0x01<<4)

SENSOR_MODE_IDX   = 0
ROI_START_ROW_IDX = 1
ROI_NUM_ROWS_IDX  = 2
F0_MODULATION_IDX = 3
F1_MODULATION_IDX = 4
ACTIVE_STREAM_BITMASK_IDX = 14
START_STOP_FLAGS_IDX = 15 # 15-22
ROI_COUNTER_IDX = 23
SATURATION_THRESHOLD_IDX = 54
SYSTEM_TYPE_IDX = 55

SYSTEM_TYPE_M25 = 2
SYSTEM_TYPE_M30 = 3 

ADC_FIRST_IDX = 31

RANGE_CAL_OFFSET_MM_LO_0807_IDX = 59
RANGE_CAL_OFFSET_MM_HI_0807_IDX = 60
RANGE_CAL_MM_PER_VOLT_LO_0807_IDX = 61
RANGE_CAL_MM_PER_VOLT_HI_0807_IDX = 62
RANGE_CAL_MM_PER_CELSIUS_LO_0807_IDX = 63
RANGE_CAL_MM_PER_CELSIUS_HI_0807_IDX = 64
RANGE_CAL_OFFSET_MM_LO_0908_IDX = 65
RANGE_CAL_OFFSET_MM_HI_0908_IDX = 66
RANGE_CAL_MM_PER_VOLT_LO_0908_IDX = 67
RANGE_CAL_MM_PER_VOLT_HI_0908_IDX = 68
RANGE_CAL_MM_PER_CELSIUS_LO_0908_IDX = 69
RANGE_CAL_MM_PER_CELSIUS_HI_0908_IDX = 70

ADC_CAL_GAIN_IDX = 71
ADC_CAL_OFFSET_IDX = 72

SCAN_TABLE_TAG_IDX = 73

# PER_FOV_METADATA
USER_TAG_IDX = 0
BIN_MODE_IDX = 1
NEAREST_NEIGHBOR_LEVEL_IDX = 2
FOV_ROW_START_IDX = 3
FOV_NUM_ROWS_IDX = 4
FOV_NUM_ROIS_IDX = 5
RTD_ALGORITHM_COMMON_IDX = 6
SNR_THRESH_IDX = 7
RANDOM_FOV_TAG_IDX = 10
RTD_ALGORITHM_GRID_IDX = 11
RTD_ALGORITHM_STRIPE_IDX = 12

REDUCE_MODE_IDX = 49
PER_FOV_IDX = 200
PER_FOV_MD_SIZE = 32

_c = np.float32(299792498.0)

def currentFile(file = __file__) :
    path = Path(file)
    return path.name

def printMetadata(metadata) :
    print(f'{currentFile()} - Selected Metadata')
    for fov_idx in range(MAX_ACTIVE_FOVS) :
        if getIsFovEnabled(metadata, fov_idx):
            print(f'{currentFile()} - per-fov metadata for fov {fov_idx}')
            print(f'{currentFile()} = \tFOV enabled {getIsFovEnabled(metadata, fov_idx)}')
            print(f'{currentFile()} - \tStripeModeEnabled {getStripeModeEnabled(metadata, fov_idx)}')
            print(f'{currentFile()} - \tPerformMinMaxFilter {getPerformMinMaxFilter(metadata, fov_idx)}')
            print(f'{currentFile()} - \tPerformGhostMedian {getPerformGhostMedian(metadata, fov_idx)}')
            print(f'{currentFile()} - \tDisableRangeMasking {getDisableRangeMasking(metadata, fov_idx)}')
            print(f'{currentFile()} - \tNearestNeighborFilterLevel {getNearestNeighborFilterLevel(metadata, fov_idx)}')
            print(f'{currentFile()} - \tSnrThresh {getSnrThresh(metadata, fov_idx)}')
            print(f'{currentFile()} - \tSmoothingFilterSize {getSmoothingFilterSize(metadata, fov_idx)}')
            print(f'{currentFile()} - \tdisable RTD: {getRtdDisabled(metadata, fov_idx)}')
            print(f'{currentFile()} - \trandomFovTag: {getRandomFovTag(metadata, fov_idx)}')
            print(f'{currentFile()} - \tstripe SNR-weighted sum: {getStripeModeSnrWeightedSum(metadata, fov_idx)}')
            print(f'{currentFile()} - \tstripe rect sum: {getStripeModeRectSum(metadata, fov_idx)}')
            print(f'{currentFile()} - \tstripe Gaussian sum: {getStripeModeGaussianSum(metadata, fov_idx)}')
            print(f'{currentFile()} - \tstripe range median enabled: {getStripeModeRangeMedianEnabled(metadata, fov_idx)}')

def setSnrThresh(metadata, fov_idx, thresh) :
    getPerFovMetadata(metadata, fov_idx)[SNR_THRESH_IDX] = thresh

def setStripeWindow(metadata, fov_idx, val) :
    getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] &= 0xfff8 # unset the least 3 bits
    getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] |= val

def enableRtd(metadata, fov_idx) :
    getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_COMMON_IDX] &= ~(RTD_ALG_COMMON_DISABLE_RTD)
    return metadata

def enableStripeModeRangeMedian(metadata, fov_idx) :
    getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] |= RTD_ALG_STRIPE_ENABLE_RANGE_MEDIAN

def getMaxUnambiguousRange(metadata) :
    #  const float_t mur = 0.5F * C_MPS/(float_t)GPixel::getGcf(getF0ModulationIndex(), getF1ModulationIndex()); 
    return np.float32(0.5 * _c / (GCFS[getF1ModulationIndex(metadata)]))

def getPerFovMetadata(metadata, fov_idx) :
    return metadata[PER_FOV_IDX + fov_idx*PER_FOV_MD_SIZE : PER_FOV_IDX + (fov_idx+1)*PER_FOV_MD_SIZE]

def getRtdAlgorithmCommon(metadata, fov_idx):
    return getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_COMMON_IDX]

def getRtdAlgorithmGrid(metadata, fov_idx):
    return getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_GRID_IDX]

def getRtdAlgorithmStripe(metadata, fov_idx):
    return getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX]

def getStripeModeEnabled(metadata, fov_idx) :
    return (getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_COMMON_IDX] & RTD_ALG_COMMON_STRIPE_MODE) != 0

def getStripeModeSnrWeightedSum(metadata, fov_idx) :
    return (getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] & RTD_ALG_STRIPE_SNR_WEIGHTED_SUM) != 0

def getStripeModeRectSum(metadata, fov_idx) :
    return (getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] & RTD_ALG_STRIPE_RECT_SUM) != 0
            
def getStripeModeGaussianSum(metadata, fov_idx) :
    return (getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] & RTD_ALG_STRIPE_GAUSSIAN_SUM) != 0

def getStripeModeRangeMedianEnabled(metadata, fov_idx) :
    return (getPerFovMetadata(metadata, fov_idx)[RTD_ALGORITHM_STRIPE_IDX] & RTD_ALG_STRIPE_ENABLE_RANGE_MEDIAN) != 0

def getSensorMode(metadata) :
    return metadata[SENSOR_MODE_IDX]

def wasPreviousRoiSaturated(metadata) :
    return 0 != (getSensorMode(metadata) & SENSOR_MODE_HDR_RETRY)

def getRandomFovTag(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[RANDOM_FOV_TAG_IDX]

def getScanTableTag(metadata) :
    return metadata[SCAN_TABLE_TAG_IDX]

def getEnabledMaxRangeLimit(metadata, fov_idx) :
    return (getRtdAlgorithmCommon(metadata, fov_idx) & RTD_ALG_COMMON_ENABLE_MAX_RANGE_LIMIT) != 0

def getPerformMinMaxFilter(metadata, fov_idx) :
    return (getRtdAlgorithmGrid(metadata, fov_idx) & RTD_ALG_GRID_ENABLE_GHOST_MIN_MAX) != 0

def getPerformGhostMedian(metadata, fov_idx) :
    return (getRtdAlgorithmGrid(metadata, fov_idx) & RTD_ALG_GRID_ENABLE_RANGE_MEDIAN) != 0

def getDisableRangeMasking(metadata, fov_idx) :
    return (getRtdAlgorithmCommon(metadata, fov_idx) & RTD_ALG_COMMON_DISABLE_RANGE_MASKING) != 0

def getEnableRangeTempRangeAdjustment(metadata, fov_idx) :
    return (getRtdAlgorithmCommon(metadata, fov_idx) & RTD_ALG_COMMON_ENABLE_TEMP_RANGE_ADJ) != 0

def getNearestNeighborFilterLevel(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[NEAREST_NEIGHBOR_LEVEL_IDX]

def getRtdDisabled(metadata, fov_idx) :
    return getRtdAlgorithmCommon(metadata, fov_idx) & RTD_ALG_COMMON_DISABLE_RTD != 0

def getSnrThresh(metadata, fov_idx) :
    return np.float32(getPerFovMetadata(metadata, fov_idx)[SNR_THRESH_IDX]) / np.float32(8.0)

def isM30(metadata) :
    return metadata[SYSTEM_TYPE_IDX] == SYSTEM_TYPE_M30

def isM25(metadata) :
    return metadata[SYSTEM_TYPE_IDX] == SYSTEM_TYPE_M25

def getSaturationThreshold(metadata):
    sat_thresh = metadata[SATURATION_THRESHOLD_IDX] * FPGA_DATA_SCALE
    if not getDoTapAccumulation(metadata) :
        sat_thresh *= NUM_GPIXEL_PERMUTATIONS
    return np.float32(sat_thresh)           

def isHdrDisabled(metadata) :
    return SATURATION_THRESHOLD == metadata[SATURATION_THRESHOLD_IDX]

def getSmoothingFilterSize(metadata, fov_idx) :
    binning = getBinning(metadata, fov_idx)
    if (binning == 4) : return [5,3]
    return [7,5]

def getFovNumRois(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[FOV_NUM_ROIS_IDX]

# prebinned size
def getFovNumRows(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[FOV_NUM_ROWS_IDX]

def getFovStartColumn(metadata, fov_idx) :
    return ROI_START_COLUMN

def getFovStartRow(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[FOV_ROW_START_IDX]

# prebinned size
def getFovNumColumns(metadata, fov_idx) :
    return ROI_NUM_COLUMNS

def getBinning(metadata, fov_idx) :
    return getPerFovMetadata(metadata, fov_idx)[BIN_MODE_IDX]

def getRawFovNumRows(metadata, fov_idx) : 
    return getFovNumRows(metadata, fov_idx)

def getRawFovNumColumns(metadata, fov_idx) :
    return getFovNumColumns(metadata, fov_idx) * NUM_GPIXEL_PHASES

def getRawFovSize(metadata, fov_idx) :
    return  getRawFovNumRows(metadata, fov_idx) * getRawFovNumColumns(metadata, fov_idx) 

def getBinnedRawFovNumRows(metadata, fov_idx) :
    return getFovNumRows(metadata, fov_idx) // getBinning(metadata, fov_idx)

def getBinnedRawFovNumColumns(metadata, fov_idx) :
    return NUM_GPIXEL_PHASES * (getFovNumColumns(metadata, fov_idx) // getBinning(metadata, fov_idx))

def getBinnedRawFovSize(metadata, fov_idx) :
    return getBinnedRawFovNumRows(metadata, fov_idx) * getBinnedRawFovNumColumns(metadata, fov_idx)

def getFovSize(metadata, fov_idx) : 
    return getFovNumRows(metadata, fov_idx) * getFovNumColumns(metadata, fov_idx)

def getBinnedFovSize(metadata, fov_idx) : 
    return ( getFovNumRows(metadata, fov_idx) * getFovNumColumns(metadata, fov_idx) //
            (getBinning(metadata, fov_idx)*getBinning(metadata, fov_idx)) )

def getBinnedFovHeight(metadata, fov_idx) :
    return getFovNumRows(metadata, fov_idx) // getBinning(metadata, fov_idx)

def getBinnedFovWidth(metadata, fov_idx) :
    return getFovNumColumns(metadata, fov_idx) // getBinning(metadata, fov_idx)

def getBinnedFovSize(metadata, fov_idx) : 
    return getBinnedFovWidth(metadata, fov_idx) * getBinnedFovHeight(metadata, fov_idx)

def getIsFovEnabled(metadata, fov_idx):
    return 1 == ((metadata[ACTIVE_STREAM_BITMASK_IDX] >> fov_idx) & 0x01)

def getActiveFovs(metadata) :
    active_fovs = []
    for fov_idx in range(MAX_ACTIVE_FOVS) :
        if getIsFovEnabled(metadata, fov_idx) : active_fovs.append(fov_idx)
    return active_fovs

def getRawPixelMask(metadata) : return 0xfffc

def getDoTapAccumulation(metadata) :
    return 0 == (0x01 & metadata[REDUCE_MODE_IDX])

def getRoiStartRow(metadata) :
    return metadata[ROI_START_ROW_IDX]

def getRoiNumRows(metadata) :
    return metadata[ROI_NUM_ROWS_IDX]

def getRoiNumColumns(metadata) : 
    return IMAGE_WIDTH

def getNumPhases(metadata) : 
    return NUM_GPIXEL_PHASES

def getNumFrequencies(metadata) :
    return NUM_GPIXEL_FREQUENCIES

def getNumPermutations(metadata) : # 1 if tap sum on FPGA, 3 if not.
    if (metadata[REDUCE_MODE_IDX] == 0) : return 3
    return 1

def getF0ModulationIndex(metadata):
    return metadata[F0_MODULATION_IDX]

def getF1ModulationIndex(metadata):
    return metadata[F1_MODULATION_IDX]

def getFs(metadata) :
    return [IDX_TO_FRQ_LUT[getF0ModulationIndex(metadata)], 
            IDX_TO_FRQ_LUT[getF1ModulationIndex(metadata)]]

def getGcf(metadata) :
    return [GCFS[getF1ModulationIndex(metadata)]]

def getFsInt(metadata) :
    fs = getFs(metadata)
    gcf = getGcf(metadata)

    return np.round(np.array([fs[0]/gcf, fs[1]/gcf])).astype(np.float32)

def getReduceMode(metadata):
    return metadata[REDUCE_MODE_IDX]

def getRoiCounter(metadata) :
    return metadata[ROI_COUNTER_IDX]

def getFirstRoi(metadata, fov_idx):
    return 0 != (START_STOP_FLAG_FIRST_ROI & metadata[START_STOP_FLAGS_IDX + fov_idx])

def getFrameCompleted(metadata, fov_idx):
    return  getStripeModeEnabled(metadata, fov_idx) or 0 != (START_STOP_FLAG_FRAME_COMPLETED & metadata[START_STOP_FLAGS_IDX + fov_idx]) 

def s16(lo, hi) :
    return struct.unpack('<h', bytearray([lo & 0xff, (hi & 0xf)<<4 | (lo & 0xf00)>>8]))

def getRangeCalMmPerVolt(metadata, mod_idx) :
    mm_per_volt_scale = np.float32(pow(2.0, -12.0))
    if (mod_idx == 8) :
        mm_per_v_lo = metadata[RANGE_CAL_MM_PER_VOLT_LO_0807_IDX]
        mm_per_v_hi = metadata[RANGE_CAL_MM_PER_VOLT_HI_0807_IDX]
    else :
        mm_per_v_lo = metadata[RANGE_CAL_MM_PER_VOLT_LO_0908_IDX]
        mm_per_v_hi = metadata[RANGE_CAL_MM_PER_VOLT_HI_0908_IDX]
    
    return np.float32(s16(mm_per_v_lo, mm_per_v_hi)) * mm_per_volt_scale

def getRangeCalOffsetMm(metadata, mod_idx):
    range_cal_offset_scale = np.float32(pow(2.0, -5.0))
    if (mod_idx == 8) :
        offset_lo = metadata[RANGE_CAL_OFFSET_MM_LO_0807_IDX]
        offset_hi = metadata[RANGE_CAL_OFFSET_MM_HI_0807_IDX]
    else :
        offset_lo = metadata[RANGE_CAL_OFFSET_MM_LO_0908_IDX]
        offset_hi = metadata[RANGE_CAL_OFFSET_MM_HI_0908_IDX]
    return np.float32(s16(offset_lo, offset_hi)) * range_cal_offset_scale


'''
lo is a 12-bit signed value.
return a signed floating point value that corresponds to the signed 12-bit value
'''
def s12(lo) :
    s_12_scale = np.float32(pow(2.0, -4.0))
    return np.float32(struct.unpack('<h', bytearray([(lo&0xf) << 4, (lo&0xff0) >> 4]))) * s_12_scale
    
def getAdcCalOffset(metadata) :
    adc_cal_offset_scale = np.float32(pow(2.0, -14.0))
    return s12(metadata[ADC_CAL_OFFSET_IDX]) * adc_cal_offset_scale

def getRangeCalMmPerCelsius(metadata, mod_idx) :
    mm_per_c_scale = np.float32(pow(2.0, -7.0))
    if (mod_idx == 8) :
        mm_per_c_lo = metadata[RANGE_CAL_MM_PER_CELSIUS_LO_0807_IDX]
        mm_per_c_hi = metadata[RANGE_CAL_MM_PER_CELSIUS_HI_0807_IDX]
    else :
        mm_per_c_lo = metadata[RANGE_CAL_MM_PER_CELSIUS_LO_0908_IDX]
        mm_per_c_hi = metadata[RANGE_CAL_MM_PER_CELSIUS_HI_0908_IDX]
    return np.float32((mm_per_c_lo & 0xfff) | ((mm_per_c_hi & 0xf) << 12)) * mm_per_c_scale

def getAdcCalGain(metadata) :
    adc_cal_gain_scale = np.float32(pow(2.0, -19.0))
    adcCalGain = np.float32(metadata[ADC_CAL_GAIN_IDX]) * adc_cal_gain_scale
    return adcCalGain
    pass

def getAdc(metadata, adc_idx):
    return metadata[ADC_FIRST_IDX + adc_idx]