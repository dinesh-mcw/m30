/**
 * @file RtdVec.h
 * @brief A utility class for holding vectors for processing.
 * The RtdVec class is defined separately, for CPU vs GPU builds.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 */

#pragma once
#include <cassert>

#define MAKE_VECTOR(a, atype, asize_) {		\
  auto asize = std::size_t(asize_); \
  if ( (a).size() != asize ) \
      { (a).resize(asize); changed = true; }	\
  }

#define MAKE_VECTOR2(a, atype, asize_) {\
  auto asize = std::size_t(asize_); \
  if ( (a).empty() ) { (a) = { std::vector<atype>(asize), std::vector<atype>(asize) }; changed=true; } \
  if ( ( (a)[0].size() != asize ) || ( (a)[1].size() != asize ) ) \
   { (a)[0].resize(asize); (a)[1].resize(asize); changed=true; }		\
  }

#define RtdVec std::vector<float_t>

