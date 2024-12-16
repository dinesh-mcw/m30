/**
 * @file LumoUtil.h
 * @brief Some general purpose utilities used in the RawToDepth repo.
 * 
 * @copyright Copyright (c) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <ios>
#include <iostream>
#include <fstream>

#ifndef STRINGIFY
#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)
#endif

class LumoUtil
{
 public:
  static uint32_t countWordsInFile(std::string filename, std::string key);

  static size_t roundToMultiple(size_t val, size_t factor) // increase val so that it is an even multiple of factor
  {
    if (0 == val%factor) 
    {
      return val;
    }
    return (1 + val/factor)*factor;
  }
  
  static void dump(const std::string filename, const std::vector<uint16_t> data)
  {
    auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
    outf.write((char*)(data.data()), std::streamsize(sizeof(uint16_t) * data.size()));
    outf.close();
  }
  static void dump(const std::string filename, const std::vector<int32_t> data)
  {
    auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
    outf.write((char*)(data.data()), std::streamsize(sizeof(int32_t) * data.size()));
    outf.close();
  }
  static void dump(const std::string filename, const std::vector<bool> data)
  {
    auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
    auto int_data = std::vector<float_t>(data.size(), 0.0F);
    for (auto idx=0; idx<data.size(); idx++)
    {
      if (data[idx]) 
      {
        int_data[idx] = 1;
      }
    }
    outf.write((char*)(int_data.data()), std::streamsize(sizeof(float_t) * int_data.size()));
    outf.close();
  }

  static void dump(const std::string filename, const std::vector<float_t> data_, float_t scale = 1.0F)
  {
    auto data = std::vector<float_t>(data_.begin(), data_.end());
    auto outf = std::ofstream(filename, std::ios::out | std::ios::binary);
    if (scale != 1.0F)
    {
      for (auto idx=0; idx<data.size(); idx++)
      {
        data[idx] = (float_t)round(double(data[idx])*double(scale));
      }
    }
    outf.write((char*)(data.data()), std::streamsize(sizeof(float_t) * data.size()));
    outf.close();
  }

  
};
