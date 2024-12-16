/**
 * @file RawToDepthUtil.h
 * @brief Utility functions used by RawToDepth Google Tests
 * 
 * @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once
#include <string>
#include <iostream>
#include <list>
#include <vector>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <LumoLogger.h>

template<typename T>
class RawToDepthUtil
{
 private:
	
 public :

  static std::vector<T> load(std::string filename)
  {
    auto numBytesInFile = std::filesystem::file_size(filename);
    int numElements = numBytesInFile/sizeof(T);

    return load(filename, numElements);
  }

  static std::vector<T> load(std::string filename, uint32_t numElements)
  {
    auto rawData = std::vector<T>(numElements);
    
    auto inf = std::ifstream(filename, std::ios::in | std::ios::binary);
    if (inf.is_open())
    {
      inf.read((char*)(rawData.data()), sizeof(T)*numElements);
    }
    else
    {
      std::cout << "Error opening input file " << filename << "\n";
      return std::vector<T>();
    }

    inf.close();
    return rawData;
  }

  
  static std::vector<T> load(const std::list<std::string>& filenames, const std::vector<uint32_t>&& size, uint32_t numElementsPerPixel=3)
  {
    auto imh = size[0];
    auto imw = numElementsPerPixel * size[1]; //3 shorts per pixel
    auto imsize = imh * imw;

    auto rawData = std::vector<T>(imsize * filenames.size());
    uint32_t idx = 0;
    for (auto const& filename : filenames)
    {
      auto inf = std::ifstream(filename, std::ios::in | std::ios::binary);
      if (inf.is_open())
      {
        inf.read((char*)(rawData.data() + idx * imsize), sizeof(T) * imsize);
      }
      else
      {
        std::cout << "Error opening input file " << filename << "\n";
        return std::move(std::vector<T>());
      }
      inf.close();
      idx++;
    }
    return rawData;

  }


  static void dump(const std::string filename, const std::vector<T> &data)
  {
    auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
    outf.write((char*)(data.data()), sizeof(T) * data.size());
    outf.close();
  }

  static bool numDifs(const std::vector<T> &vecA, const std::vector<T> &vecB, T maxDelta, uint32_t numExpectedDifs) {
    if (vecA.size() != vecB.size()) {
      LLogDebug("Input size mismatch");
      return false;
    }
    
    uint32_t diffCount = 0;
    double maxDif = 0.0;
    for (auto idx=0; idx<vecA.size(); idx++) {
      auto aVal = vecA[idx]; 
      auto bVal = vecB[idx];
      double dif = fabs(aVal-bVal);
      if (dif > maxDif) 
      {
        maxDif = dif;
      }
      if (dif > (double)maxDelta)
      {
        diffCount++;
      }
    }

    LLogDebug("Max allowed dif " << maxDelta << ". Max number of expected difs: " << numExpectedDifs << ". Number of actual difs: " << diffCount << ". Actual Max Dif: " << maxDif);
    return diffCount <= numExpectedDifs;
    
  }
  
  static bool numDifs(std::string fn1, std::string fn2, uint32_t numElements, T maxDelta, uint32_t numExpectedDifs) 
  {
    auto fileAData = load(fn1, numElements);
    auto fileBData = load(fn2, numElements);
    if (fileAData.empty()) { std::cout << "Unable to open " << fn1 << "\n"; return false;}
    if (fileBData.empty()) { std::cout << "Unable to open " << fn2 << "\n"; return false;}

    T actualMaxDif = (T) 0;
    
    uint32_t numDifs = 0;
    for (auto rangeIdx=0; rangeIdx<numElements; rangeIdx++) {
      auto aVal = fileAData[rangeIdx]; // 12.4 format
      auto bVal = fileBData[rangeIdx];
      auto dif = fabs((double)aVal-(double)bVal);
      if (dif > actualMaxDif)
      {
        actualMaxDif = dif;
      }
      numDifs += uint32_t(fabs((double)aVal-(double)bVal) > (double)maxDelta); 
    }

    LLogDebug("Thresh " << maxDelta << ". Max dif " << actualMaxDif << ". Max number of expected difs: " << numExpectedDifs << ". Number of actual difs: " << numDifs);
    return numDifs <= numExpectedDifs;
  }

  static bool compare(std::string fn1, std::string fn2, const std::vector<uint32_t>&& size, T maxPercentDelta, uint32_t maxErrors = 0)
  {
    uint32_t errorCount = 0;
    T maxErr = T(0);
    float meanErr = 0.0F;

    auto fileAData = load({ fn1 }, size, 1);
    auto fileBData = load({ fn2 }, size, 1);
    if (fileAData.empty()) { std::cout << "Unable to open " << fn1 << "\n"; return false;}
    if (fileBData.empty()) { std::cout << "Unable to open " << fn2 << "\n"; return false;}
    
    std::cout << "Comparing " << fn1 << " against " << fn2 << "\n";
    
    for (uint32_t idx = 0; idx < fileAData.size(); idx++)
    {
      auto aval = fileAData[idx];
      auto bval = fileBData[idx];
      auto err = abs((bval - aval)/aval);
      if (err > maxPercentDelta)
      {
        meanErr += float(err);
        if (err > maxErr)
        {
          maxErr = err;
        }
        errorCount++;
        std::cout << " Comparison failed at (" << idx << ") "  << aval << " " << bval << "\n";
        return false;
      }
    }
    std::cout << "\nComparison completed with " << errorCount << " errors. Thresh " << maxPercentDelta << "%. Max error = " << maxErr << " mean err " << meanErr/float(fileAData.size()) << "\n";
    return errorCount <= maxErrors;
  }


  /// compare two files.
  /// Fail if more than maxPercentage percent of the input data is above the absolute threshold maxDelta.
  static bool compare(std::string fn1, std::string fn2, const std::vector<uint32_t>&& size, T maxDelta, float maxPercentage)
  {
    uint32_t errorCount = 0;
    T maxErr = T(0);
    float meanErr = 0.0F;

    auto fileAData = load({ fn1 }, size, 1);
    auto fileBData = load({ fn2 }, size, 1);
    if (fileAData.empty()) { std::cout << "Unable to open " << fn1 << "\n"; return false;}
    if (fileBData.empty()) { std::cout << "Unable to open " << fn2 << "\n"; return false;}
        
    for (uint32_t idx = 0; idx < fileAData.size(); idx++)
    {
      auto aval = fileAData[idx];
      auto bval = fileBData[idx];
      auto err = abs(bval - aval);
      if (err > maxDelta)
      {
        meanErr += float(err);
        if (err > maxErr)
        {
          maxErr = err;
        }
        errorCount++;
        float percentOfPixelsAboveMaxDelta = 100.0F * float(errorCount)/float(fileAData.size());
        if (percentOfPixelsAboveMaxDelta > maxPercentage)
        {
          //return false;
        }
      }
    }

    float percentOfPixelsAboveMaxDelta = 100.0F * float(errorCount)/float(fileAData.size());
    return percentOfPixelsAboveMaxDelta <= maxPercentage;
  }

  

  
  static bool compareAllowAliasing(std::string fn1, std::string fn2, const std::vector<uint32_t>&& size, T maxDelta, uint32_t maxErrors = 0, T aliasJump=T(1))
  {
    uint32_t errorCount = 0;
    T maxErr = T(0);
    float meanErr = 0.0F;

    auto fileAData = load({ fn1 }, size, 1);
    auto fileBData = load({ fn2 }, size, 1);
    if (fileAData.empty()) { std::cout << "Unable to open " << fn1 << "\n"; return false;}
    if (fileBData.empty()) { std::cout << "Unable to open " << fn2 << "\n"; return false;}

    std::cout << "Comparing " << fn1 << " against " << fn2 << "\n";
    
    for (uint32_t idx = 0; idx < fileAData.size(); idx++)
    {
      auto aval = fileAData[idx];
      auto bval = fileBData[idx];
      auto err = abs(bval - aval);

      if (abs(err - aliasJump) < err)
      {
        err = abs(err - aliasJump);
      }
      if (abs(err + aliasJump) < err)
      {
        err = abs(err + aliasJump);
      }

      meanErr += float(err);
      if (err > maxErr)
      {
        maxErr = err;
      }
      
      if ( err > maxDelta)
      {
        errorCount++;
        //std::cout << " Comparison failed at (" << idx << ") "  << aval << " " << bval << "\n";
        //return false;
      }
    }
    std::cout << "\nComparison completed with " << errorCount << " errors. Thresh " << maxDelta <<
      ". Sum Err = " << meanErr << ". Total count = " << fileAData.size() <<
      ". Max error = " << maxErr << " mean err " << meanErr/float(fileAData.size()) << "\n";
    return errorCount <= maxErrors;
  }

  // Compares two files, but only notes a difference if the signal (in the third file) is above thresh.
  static bool compareAboveThresh(std::string fn1, std::string fn2, std::string fnSignal, const std::vector<uint32_t>&& size, T maxDelta, uint32_t maxErrors = 0, T thresh=0, T aliasJump=T(0))
  {
    uint32_t errorCount = 0;
    T maxErr = T(0);
    float meanErr = 0.0F;

    auto fileAData = load({ fn1 }, size, 1);
    auto fileBData = load({ fn2 }, size, 1);
    auto sig = load({ fnSignal }, size, 1);
    if (fileAData.empty()) { std::cout << "Unable to open " << fn1 << "\n"; return false;}
    if (fileBData.empty()) { std::cout << "Unable to open " << fn2 << "\n"; return false;}
    if (sig.empty()) { std::cout << "Unable to open " << fnSignal << "\n"; return false;}

    std::cout << "Comparing " << fn1 << " against " << fn2 << " but only if " << fnSignal << " > " << thresh << "\n";

    int numCheckedPixels = 0;
    int numSkippedPixels = 0;
    for (uint32_t idx = 0; idx < fileAData.size(); idx++)
    {
      auto aval = fileAData[idx];
      auto bval = fileBData[idx];
      auto sigval = sig[idx];
      if (sigval < thresh) 
      {
        numSkippedPixels++;
        continue;
      }
	
	    numCheckedPixels++;
	
	    auto err = abs(bval - aval);

      if (abs(err - aliasJump) < err)
      {
        err = abs(err - aliasJump);
      }
      if (abs(err + aliasJump) < err)
      {
        err = abs(err + aliasJump);
      }

      meanErr += float(err);
      if (err > maxErr)
      {
        maxErr = err;
      }
	
      if ( err > maxDelta)
      {
        errorCount++;
        //std::cout << " Comparison failed at (" << idx << ") "  << aval << " " << bval << "\n";
        //return false;
      }
    }
    std::cout << "\nComparison completed with " << errorCount << " errors. Thresh " << maxDelta <<
      ". Skipped " << numSkippedPixels << " pixels" <<
      ". Sum Err = " << meanErr << ". Total count = " << fileAData.size() <<
      ". Max error = " << maxErr << " mean err " << meanErr/float(numCheckedPixels) << "\n";
    return errorCount <= maxErrors;
  }

};
