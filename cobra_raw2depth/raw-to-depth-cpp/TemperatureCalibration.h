/**
 * @file TemperatureCalibration.h
 * @brief Calculate fixed offset due to changes in temperature
 * on the sensor. The temperature measurements and other parameters
 * are provided in the metadata.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <RtdMetadata.h>
#include <algorithm>

constexpr float_t MIN_VLDA_VOLTAGE { 10.0F };
constexpr float_t MAX_VLDA_VOLTAGE { 25.0F };
constexpr float_t TEMP_CELSIUS_DEFAULT { 25.0F };
constexpr float_t M_PER_MM { 1.0e-3F };
constexpr uint32_t DEFAULT_FIFO_LENGTH { 100 };

// Steinhart-Hart coefficients
static const std::array<float_t, 4> coeffs {7.74757206e-04F, 2.88511686e-04F, -4.01680505e-06F, 3.36325480e-07F};

class TemperatureCalibration
{
public:
  enum TECHNIQUE {LATEST, MEAN, MEDIAN};
  void setTechnique(TECHNIQUE technique) { _technique = technique; }
private:

  bool _disable = false; // set to true if M20, or any input metadata is inconsistent across the FOV.
  int _fifoLength = DEFAULT_FIFO_LENGTH;
  int _fifoIndex = 0;
  TECHNIQUE _technique = MEDIAN;

  const float_t M20_REF_RESISTANCE = 41200.0F;
  const float_t M20_EXTERNAL_VREF = 2.5F;
  const float_t M20_VLDA_SCALE = 29.70F*0.5F;
  const uint32_t M20_VLDA_ADC_IDX = 3;
  const uint32_t M20_LASER_THERM_ADC_IDX = 4;

  const float_t M25_REF_RESISTANCE = 7150.0F;
  const float_t M25_EXTERNAL_VREF = 1.22F;
  const float_t M25_VLDA_SCALE = 25.85F;
  const uint32_t M25_VLDA_ADC_IDX = 6;
  const uint32_t M25_LASER_THERM_ADC_IDX = 2;

  const float_t M30_REF_RESISTANCE = 7150.0F;
  const float_t M30_EXTERNAL_VREF = 1.22F;
  const float_t M30_VLDA_SCALE = 25.85F;
  const uint32_t M30_VLDA_ADC_IDX = 6;
  const uint32_t M30_LASER_THERM_ADC_IDX = 2;

  float_t REF_RESISTANCE { M30_REF_RESISTANCE };
  float_t EXTERNAL_VREF  { M30_EXTERNAL_VREF };
  float_t VLDA_SCALE     { M30_VLDA_SCALE };
  uint32_t VLDA_ADC_IDX  { M30_VLDA_ADC_IDX };
  uint32_t LASER_THERM_ADC_IDX { M30_LASER_THERM_ADC_IDX };

  float_t _tempCelsius = TEMP_CELSIUS_DEFAULT;
  float_t _vldaVoltage = 0.0F;
  float_t _rangeOffsetTemperatureMeters = 0.0F;
  float_t _adcCalGain = 0.0F;
  float_t _adcCalOffset = 0.0F;

  float_t _laserThermAdcVoltage = 0.0F;
  float_t _adcLaserThermMetadata = 0.0F;
  float_t _laserThermRes = 0.0F;

  float_t _vldaRangeOffsetMm = 0.0F;
  float_t _tempRangeOffsetMm = 0.0F;
  float_t _vldaMetadataValue = 0;
  float_t _vldaAdcVoltage = 0.0F;

  float_t _mmPerCelsius = 0.0F;
  float_t _mmPerVolt = 0.0F;
  float_t _fixedOffsetMm = 0.0F;

  std::vector<float_t> _laserThermMetadataValues;
  std::vector<float_t> _vldaMetadataValues;


  static float_t steinhart_eq(float_t res)
  {
    const float_t squared = 2.0F;
    const float_t cubed = 3.0F;
    const float_t kToC = -273.15F;
    float_t temp_k = 1.0F / (coeffs[0] +
                   coeffs[1] * log(res) +
                   coeffs[2] * pow(log(res), squared) +
                   coeffs[3] * pow(log(res), cubed));
    float_t temp_c = temp_k + kToC;
    return temp_c;
  }

  float_t getLaserThermAdcMetadataValue()
  {
    if (_technique == LATEST)
    {
      return _laserThermMetadataValues[_fifoIndex];
    }
    if (_technique == MEAN)
    {
      float_t sum = 0.0F;
      for (auto &thermVal : _laserThermMetadataValues)
      {
        sum += thermVal;
      }
      return sum / float_t(_laserThermMetadataValues.size());
    }
    auto laserThermValuesSorted = std::vector<float_t>(_laserThermMetadataValues.size());
    std::copy(_laserThermMetadataValues.begin(), _laserThermMetadataValues.end(), laserThermValuesSorted.begin());
    std::sort(laserThermValuesSorted.begin(), laserThermValuesSorted.end());
    return laserThermValuesSorted[laserThermValuesSorted.size() / 2 ];

  }

public:
  float_t getVldaMetadataValue()
  {
    if (_technique == LATEST)
    {
      return _vldaMetadataValues[_fifoIndex];
    }
    if (_technique == MEAN)
    {
      float_t sum = 0.0F;
      for (auto &vldaVal : _vldaMetadataValues)
      {
        sum += vldaVal;
      }
      return sum / float_t(_vldaMetadataValues.size());
    }

    auto vldaValuesSorted = std::vector<float_t>(_vldaMetadataValues.size());
    std::copy(_vldaMetadataValues.begin(), _vldaMetadataValues.end(), vldaValuesSorted.begin());
    std::sort(vldaValuesSorted.begin(), vldaValuesSorted.end());
    return vldaValuesSorted[vldaValuesSorted.size() / 2 ];
  }

private:
  void compute()
  {
    _rangeOffsetTemperatureMeters = 0.0F;    
    if (_disable || 
        _vldaMetadataValues.empty() ||
        _laserThermMetadataValues.empty())
    {
      return;
    }

    _adcLaserThermMetadata = getLaserThermAdcMetadataValue(); //md.getAdc(LASER_THERM_ADC_IDX);
    _vldaMetadataValue = getVldaMetadataValue(); //md.getAdc(VLDA_ADC_IDX) ;

    _laserThermAdcVoltage = _adcCalGain * _adcLaserThermMetadata + _adcCalOffset;
    _laserThermRes = (REF_RESISTANCE * _laserThermAdcVoltage) / (EXTERNAL_VREF - _laserThermAdcVoltage);
    _tempCelsius = steinhart_eq(_laserThermRes);

    if (isnan(_tempCelsius))
    {
      _rangeOffsetTemperatureMeters = 0.0F;
      LLogErr("Measured temperature resulted in an invalid result. Error on measured thermistor value. Temperature compensation is disabled.");
      return;
    }

    _vldaAdcVoltage = _adcCalGain * _vldaMetadataValue + _adcCalOffset;
    _vldaVoltage = _vldaAdcVoltage * VLDA_SCALE;

    if (_vldaVoltage < MIN_VLDA_VOLTAGE || _vldaVoltage > MAX_VLDA_VOLTAGE)
    {
      _rangeOffsetTemperatureMeters = 0.0F;
      LLogErr("Measured VLDA voltage for temperature compensation is " << _vldaVoltage << 
              ". This is outside accepted range of (" << MIN_VLDA_VOLTAGE << "," << MAX_VLDA_VOLTAGE << "). Temperature compensation is disabled.");
      return;
    }

    _vldaRangeOffsetMm = _mmPerVolt*_vldaVoltage;
    _tempRangeOffsetMm = _mmPerCelsius*_tempCelsius;
    float_t rangeOffsetTemperatureMm = _fixedOffsetMm + _tempRangeOffsetMm - _vldaRangeOffsetMm;
    _rangeOffsetTemperatureMeters = M_PER_MM * rangeOffsetTemperatureMm;
  }


public:
  void setAdcValues(RtdMetadata &mdat, uint32_t fovIdx)
  {    
    if (!mdat.getEnableRangeTempRangeAdjustment(fovIdx))
    {
      _rangeOffsetTemperatureMeters = 0.0F;
      return;
    }

    if (mdat.getFirstRoi(fovIdx))
    {
      if (mdat.isM20())
      {
        REF_RESISTANCE = M20_REF_RESISTANCE;
        EXTERNAL_VREF = M20_EXTERNAL_VREF;
        VLDA_SCALE = M20_VLDA_SCALE;
        VLDA_ADC_IDX = M20_VLDA_ADC_IDX;
        LASER_THERM_ADC_IDX = M20_LASER_THERM_ADC_IDX;

        _disable = true;
        _rangeOffsetTemperatureMeters = 0.0F;
        return;
      }

      _disable = false;
      _adcCalGain = mdat.getAdcCalGain();
      _adcCalOffset = mdat.getAdcCalOffset();
      _mmPerCelsius = mdat.getRangeCalMmPerCelsius(mdat.getF0ModulationIndex());
      _mmPerVolt = mdat.getRangeCalMmPerVolt(mdat.getF0ModulationIndex());
      _fixedOffsetMm = mdat.getRangeCalOffsetMm(mdat.getF0ModulationIndex());

      if (mdat.isM25())
      {
        REF_RESISTANCE = M25_REF_RESISTANCE;
        EXTERNAL_VREF = M25_EXTERNAL_VREF;
        VLDA_SCALE = M25_VLDA_SCALE;
        VLDA_ADC_IDX = M25_VLDA_ADC_IDX;
        LASER_THERM_ADC_IDX = M25_LASER_THERM_ADC_IDX;
      }
      if (mdat.isM30())
      {
        REF_RESISTANCE = M30_REF_RESISTANCE;
        EXTERNAL_VREF = M30_EXTERNAL_VREF;
        VLDA_SCALE = M30_VLDA_SCALE;
        VLDA_ADC_IDX = M30_VLDA_ADC_IDX;
        LASER_THERM_ADC_IDX = M30_LASER_THERM_ADC_IDX;
      }
    }
    
    if (_disable) 
    {
      return;
    }
  
    // Error check: if metadata is inconsistent, disable temperature correction for this FOV.
    if (
      _adcCalGain != mdat.getAdcCalGain() || 
      _adcCalOffset != mdat.getAdcCalOffset() ||
      _mmPerCelsius != mdat.getRangeCalMmPerCelsius(mdat.getF0ModulationIndex()) ||
      _mmPerVolt != mdat.getRangeCalMmPerVolt(mdat.getF0ModulationIndex()) ||
      _fixedOffsetMm != mdat.getRangeCalOffsetMm(mdat.getF0ModulationIndex()) ||
      0 == _adcCalGain
    )
    {
      _rangeOffsetTemperatureMeters = 0.0F;
      _laserThermMetadataValues.clear();
      _vldaMetadataValues.clear();
      _disable = true;
      return;
    }

    auto laserThermMetadataVal = (float_t)mdat.getAdc(LASER_THERM_ADC_IDX);
    auto vldaMetadataVal = (float_t)mdat.getAdc(VLDA_ADC_IDX);
    if (_laserThermMetadataValues.empty() || 
        _vldaMetadataValues.empty())
    { // Initialize the ADC value buffers with the initial value on first call.
      _laserThermMetadataValues = std::vector<float_t>(_fifoLength, laserThermMetadataVal);
      _vldaMetadataValues = std::vector<float_t>(_fifoLength, vldaMetadataVal);
    }
    
    _fifoIndex++;
    _fifoIndex = _fifoIndex % _fifoLength;

    _laserThermMetadataValues[_fifoIndex] = laserThermMetadataVal;
    _vldaMetadataValues[_fifoIndex] = vldaMetadataVal;

    if (mdat.getFrameCompleted(fovIdx))
    {
      compute();
    }
  }

  float_t getRangeOffsetTemperature() const 
  { 
    if (_disable) 
    {
      return 0.0F;
    }
    // LumoLogInfo("Range correction %gm. Temperature %g deg c", _rangeOffsetTemperatureMeters, _tempCelsius);
    return _rangeOffsetTemperatureMeters; 
  } // in meters.

  float_t getTempAdcValue() const { return _laserThermAdcVoltage; }
  uint16_t getTempMetadataValue() const { return uint16_t(roundf(_adcLaserThermMetadata)); }
  float_t getLaserThermRes() const { return _laserThermRes; }
  float_t getLaserTempCelsius() const { return _tempCelsius; }

  float_t getAdcCalGain() const { return _adcCalGain; }
  float_t getAdcCalOffset() const { return _adcCalOffset;}

  float_t getVldaAdcVoltage() const { return _vldaAdcVoltage; }
  float_t getVldaVoltage() const { return _vldaVoltage; }

  float_t getVldaRangeOffsetMm() const { return _vldaRangeOffsetMm; }
  float_t getLaserTempRangeOffsetMm() const { return _tempRangeOffsetMm; }
};
