/**
 * @file LumoUtil.cpp
 * @brief Some general purpose utilities used in the RawToDepth repo.
 * 
 * @copyright Copyright (c) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include <LumoUtil.h>
#include <iostream>
#include <fstream>
#include <string>

uint32_t LumoUtil::countWordsInFile(std::string filename, std::string key)
{
  std::ifstream fileStream(filename, std::ifstream::in);
  std::string line;
  uint32_t count=0;
  while (std::getline(fileStream, line))
  {
    if (std::string::npos != line.find(key)) 
    {
    count++;
    }
  }
  return count;
}

