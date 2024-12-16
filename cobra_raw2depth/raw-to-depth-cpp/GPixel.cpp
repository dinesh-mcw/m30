/**
 * @file GPixel.cpp
 * @brief Contains some convenience getters and constants related to the 
 * GPixel iTOF sensor.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */
#include "GPixel.h"

const std::vector<float_t> GPixel::IDX_TO_FRQ_LUT =
  {0, 0, 0, 1.0e9F/(3.0F*3.0F), 1.0e9F/(3.0F*4.0F), 1.0e9F/(3.0F*5.0F), 1.0e9F/(3.0F*6.0F), 1.0e9F/(3.0F*7.0F), 1.0e9F/(3.0F*8.0F), 1.0e9F/(3.0F*9.0F), 1.0e9F/(3.0F*10.0F)};

#define GCF34 (1.0e9F/(3.0F*12.0F))
#define GCF45 (1.0e9F/(3.0F*20.0F))
#define GCF56 (1.0e9F/(3.0F*30.0F))
#define GCF67 (1.0e9F/(3.0F*42.0F))
#define GCF78 (1.0e9F/(3.0F*56.0F))
#define GCF89 (1.0e9F/(3.0F*72.0F))
#define GCF90 (1.0e9F/(3.0F*90.0F))

const std::vector<std::vector<float_t>> GPixel::IDX_TO_GCF_LUT =
  {
   {0,0,0, 0,0,0,0,0,0,0},
   {0,0,0, 0,0,0,0,0,0,0},
   {0,0,0, 0,0,0,0,0,0,0},
  //0 1 2  3      4      5      6      7      8      9      10
   {0,0,0, 0,     GCF34, 0,     0,     0,     0,     0,     0    }, //3
   {0,0,0, GCF34, 0,     GCF45, 0,     0,     0,     0,     0    }, //4
   {0,0,0, 0,     GCF45, 0,     GCF56, 0,     0,     0,     0    }, //5
   {0,0,0, 0,     0,     GCF56, 0,     GCF67, 0,     0,     0    }, //6
   {0,0,0, 0,     0,     0,     GCF67, 0,     GCF78, 0,     0    }, //7
   {0,0,0, 0,     0,     0,     0,     GCF78, 0,     GCF89, 0    }, //8
   {0,0,0, 0,     0,     0,     0,     0,     GCF89, 0,     GCF90}, //9
   {0,0,0, 0,     0,     0,     0,     0,     0,     GCF90, 0    }, //10
  };

uint32_t GPixel::getGcf(uint32_t f0ModulationIndex, uint32_t f1ModulationIndex) {
  
  // check for invalid 0,1, or 2 values for mod idx.
  if (0 == IDX_TO_FRQ_LUT[f0ModulationIndex] ||
      0 == IDX_TO_FRQ_LUT[f1ModulationIndex] )
  {
    LLogErr("Undefined modulation frequency index " << f0ModulationIndex << " or " << f1ModulationIndex);
    return 0;
  }

  assert(IDX_TO_GCF_LUT[3][4]  == IDX_TO_GCF_LUT[4][3]);
  assert(IDX_TO_GCF_LUT[4][5]  == IDX_TO_GCF_LUT[5][4]);
  assert(IDX_TO_GCF_LUT[5][6]  == IDX_TO_GCF_LUT[6][5]);
  assert(IDX_TO_GCF_LUT[6][7]  == IDX_TO_GCF_LUT[7][6]);
  assert(IDX_TO_GCF_LUT[7][8]  == IDX_TO_GCF_LUT[8][7]);
  assert(IDX_TO_GCF_LUT[8][9]  == IDX_TO_GCF_LUT[9][8]);
  assert(IDX_TO_GCF_LUT[9][10] == IDX_TO_GCF_LUT[10][9]);

  auto gcf = IDX_TO_GCF_LUT[f0ModulationIndex][f1ModulationIndex];
  
  if (0 == gcf) { // We only support n, n-1 modulation frequency indices.
    LLogErr("Undefined frequency modulation index combination. Modulation indices f0:" << 
                f0ModulationIndex << " f1:" << f1ModulationIndex << ". Frequencies f0:" << IDX_TO_FRQ_LUT[f0ModulationIndex] << ", f1:" << IDX_TO_FRQ_LUT[f1ModulationIndex]);
    return 0;
  }

  return uint32_t(roundf(gcf));

}
