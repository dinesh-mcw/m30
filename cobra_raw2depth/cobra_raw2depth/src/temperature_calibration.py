import math
import numpy as np
import M30Metadata as md
import math

class TemperatureCalibration() :

  def __init__(self) :
    self._rangeOffsetTemperatureMeters = 0.0
    self._coefs =np.array([7.74757206e-04, 2.88511686e-04, -4.01680505e-06, 3.36325480e-07], dtype=np.float32)
    self._fifoLength = 100
    self._fifoIndex = 0
    self._disable = False

    self.M25_REF_RESISTANCE = 7150.0
    self.M25_EXTERNAL_VREF = 1.22
    self.M25_VLDA_SCALE = 25.85
    self.M25_VLDA_ADC_IDX = 6
    self.M25_LASER_THERM_ADC_IDX = 2
    self.M30_REF_RESISTANCE = 7150.0
    self.M30_EXTERNAL_VREF = 1.22
    self.M30_VLDA_SCALE = 25.85
    self.M30_VLDA_ADC_IDX = 6
    self.M30_LASER_THERM_ADC_IDX = 2

    self.MIN_VLDA_VOLTAGE = 10.0
    self.MAX_VLDA_VOLTAGE = 25.0
    self.M_PER_MM = 1.0e-3

    self._refResistance = self.M30_REF_RESISTANCE
    self._externalVref = self.M30_EXTERNAL_VREF
    self._vldaScale = self.M30_VLDA_SCALE
    self._vldaAdcIdx = self.M30_VLDA_ADC_IDX
    self._laserThermAdcIdx = self.M30_LASER_THERM_ADC_IDX

    self._laserThermMetadataValues = np.array([], dtype=np.float32)
    self._vldaMetadataValues = np.array([], dtype=np.float32)

  def set_adc_values(self, metadata, fov_idx) :
    if not md.getEnableRangeTempRangeAdjustment(metadata, fov_idx) :
      self._rangeOffsetTemperatureMeters = 0.0
      return
    
    if md.getFirstRoi(metadata, fov_idx) :
      
      self._disable = False
      self._adcCalGain = md.getAdcCalGain(metadata)
      self._adcCalOffset = md.getAdcCalOffset(metadata)
      self._mmPerCelsius = md.getRangeCalMmPerCelsius(metadata, md.getF0ModulationIndex(metadata))
      self._mmPerVolt = md.getRangeCalMmPerVolt(metadata, md.getF0ModulationIndex(metadata))
      self._fixedOffsetMm = md.getRangeCalOffsetMm(metadata, md.getF0ModulationIndex(metadata))

      if md.isM25(metadata): 
        self._refResistance = self.M25_REF_RESISTANCE
        self._externalVref = self.M25_EXTERNAL_VREF
        self._vldaScale = self.M25_VLDA_SCALE
        self._vldaAdcIdx = self.M325_VLDA_ADC_IDX
        self._laserThermAdcIdx = self.M30_LASER_THERM_ADC_IDX
      if md.isM30(metadata) :
        self._refResistance = self.M30_REF_RESISTANCE
        self._externalVref = self.M30_EXTERNAL_VREF
        self._vldaScale = self.M30_VLDA_SCALE
        self._vldaAdcIdx = self.M30_VLDA_ADC_IDX
        self._laserThermAdcIdx = self.M30_LASER_THERM_ADC_IDX
      
    if self._disable :
      self._rangeOffsetTemperatureMeters = 0.0
      return

    if (self._laserThermMetadataValues.size == 0 or 
        self._vldaMetadataValues.size == 0) :
      self._laserThermMetadataValues = np.array([md.getAdc(metadata, self._laserThermAdcIdx)]*self._fifoLength, dtype=np.float32)
      self._vldaMetadataValues = np.array([md.getAdc(metadata, self._vldaAdcIdx)]*self._fifoLength, dtype=np.float32)
    
    self._fifoIndex += 1
    self._fifoIndex = self._fifoIndex % self._fifoLength

    self._laserThermMetadataValues[self._fifoIndex] = md.getAdc(metadata, self._laserThermAdcIdx)
    self._vldaMetadataValues[self._fifoIndex] = md.getAdc(metadata, self._vldaAdcIdx)

    if md.getFrameCompleted(metadata, fov_idx) :
      self.compute(metadata, fov_idx)


  def steinhart_eq(self, res) :
    temp_k = np.float32(1.0) / np.float32(
                   (self._coefs[0] + 
                    self._coefs[1] * np.float32(math.log(res)) + 
                    self._coefs[2] * np.float32(pow(math.log(res), np.float32(2.0))) + 
                    self._coefs[3] * np.float32(pow(math.log(res), np.float32(3.0)))))
    temp_c = temp_k + np.float32(-273.15)
    return temp_c
  
  def getLaserThermAdcMetadataValue(self) :
    laserThermValuesSorted = np.sort(self._laserThermMetadataValues)
    return laserThermValuesSorted[laserThermValuesSorted.size // 2]

  def getVldaMetadataValue(self) :
    vldaValuesSorted = np.sort(self._vldaMetadataValues)
    return vldaValuesSorted[vldaValuesSorted.size // 2]
  
  def compute(self, metadata, fov_idx) :
    self._rangeOffsetTemperatureMeters = 0.0
    if (self._disable or #not md.getEnableRangeTempRangeAdjustment(metadata, fov_idx) or
        self._vldaMetadataValues.size == 0 or
          self._laserThermMetadataValues.size == 0) :
      return
    
    self._adcLaserThermMetadata = self.getLaserThermAdcMetadataValue()
    self._vldaMetadataValue = self.getVldaMetadataValue()

    if (self._adcLaserThermMetadata == 0 or 
        self._vldaMetadataValue == 0) :
      self._rangeOffsetTemperatureMeters = np.float32(0.0)
      return

    self._laserThermAdcVoltage = self._adcCalGain * self._adcLaserThermMetadata + self._adcCalOffset
    self._laserThermRes = (self._refResistance * self._laserThermAdcVoltage) / (self._externalVref - self._laserThermAdcVoltage)
    self._tempCelsius = self.steinhart_eq(self._laserThermRes)

    if math.isnan(self._tempCelsius) :
      self._rangeOffsetTemperatureMeters = np.float32(0.0)
      return

    self._vldaAdcVoltage = self._adcCalGain * self._vldaMetadataValue + self._adcCalOffset
    self._vldaVoltage = self._vldaAdcVoltage * self._vldaScale

    if (self._vldaVoltage < self.MIN_VLDA_VOLTAGE or 
        self._vldaVoltage > self.MAX_VLDA_VOLTAGE) :
      self._rangeOffsetTemperatureMeters = np.float32(0.0)
      return
    
    self._vldaRangeOffsetMm = self._mmPerVolt*self._vldaVoltage
    self._tempRangeOffsetMm = self._mmPerCelsius*self._tempCelsius
    rangeOffsetTemperatureMm = self._fixedOffsetMm + self._tempRangeOffsetMm - self._vldaRangeOffsetMm
    self._rangeOffsetTemperatureMeters = self.M_PER_MM * rangeOffsetTemperatureMm


  def getRangeOffsetTemperature(self) :
    if self._disable :
      self._rangeOffsetTemperatureMeters = np.float32(0.0)
    return self._rangeOffsetTemperatureMeters
