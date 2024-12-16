/**
 * @file FloatVectorPool.cpp
 * @brief Implements a dynamic pool for std::vector<float_t>.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#include "FloatVectorPool.h"
#include <cassert>
#include <thread>

std::shared_ptr<FloatVectorPool> FloatVectorPool::_inst;
std::mutex FloatVectorPool::_getterMutex;

int FloatVectorPool::size() {
  auto lockedPool = getInst();
  return (int)lockedPool.inst->_vectors.size();
}

bool FloatVectorPool::exists(std::shared_ptr<std::vector<float_t>> vec) {
  auto lockedPool = getInst();
  for (auto idx=0; idx<lockedPool.inst->_vectors.size(); idx++) {
    if (vec.get()==lockedPool.inst->_vectors.at(idx).get()) 
    {
      return true;
    }
  }
  return false;
}

int FloatVectorPool::numBusy() {
  auto lockedPool = getInst();
  int numBusy = 0;
  for (auto idx=0; idx<lockedPool.inst->_inUse.size(); idx++)
  {
    if (lockedPool.inst->_inUse.at(idx)) 
    {
      numBusy++;
    }
  }
  return numBusy;
}

SyncedFloatVectorPool FloatVectorPool::getInst() {
  std::scoped_lock mutexLock(_getterMutex); // The getter method itself needs to be re-entrant
  if (_inst) {
    return SyncedFloatVectorPool(_inst); // This locks the pool
  }
  
  _inst = std::make_shared<FloatVectorPool>();
  return SyncedFloatVectorPool(_inst); // This locks the pool
}

std::shared_ptr<std::vector<float_t>> FloatVectorPool::get(uint32_t size) {

  auto lockedPool = getInst();
  
  auto idx = lockedPool.inst->find(size, lockedPool.inst);
  if (idx >= 0) {
    lockedPool.inst->_inUse.at(idx) = true;
    assert(lockedPool.inst->_vectors.size() == lockedPool.inst->_inUse.size());
    return lockedPool.inst->_vectors.at(idx);
  }
  lockedPool.inst->_vectors.emplace_back(std::make_shared<std::vector<float_t>>(size));
  lockedPool.inst->_inUse.emplace_back(true);
  assert(lockedPool.inst->_vectors.size() == lockedPool.inst->_inUse.size());
  return lockedPool.inst->_vectors.back();
}

int FloatVectorPool::find(uint32_t size, std::shared_ptr<FloatVectorPool> inst) {
  
  for (auto idx=0; idx<inst->_vectors.size(); idx++) 
  {
    if (inst->_vectors.at(idx)->size() == size &&
	      !inst->_inUse.at(idx))
    {
      return idx;
    }
  }
  return -1;
}

void FloatVectorPool::release(std::shared_ptr<std::vector<float_t>> vec) {
  auto lockedPool = getInst();

  for (auto idx=0; idx<lockedPool.inst->_vectors.size(); idx++) 
  {
    if (vec.get() == lockedPool.inst->_vectors.at(idx).get()) 
    { 
      lockedPool.inst->_inUse.at(idx) = false;
      return;
    }
  }
  LLogErr("Could not find vector " << vec.get() << " for release in thread " << std::this_thread::get_id());
}

void FloatVectorPool::clear() {
  auto lockedPool = getInst();

  std::vector<std::shared_ptr<std::vector<float_t>>> vectors;
  std::vector<bool> inUse;

  // Only clear the items that aren't currently in use.
  // Some items might be held by other threads.
  for (auto idx=0; idx<lockedPool.inst->_inUse.size(); idx++)
  {
    if (lockedPool.inst->_inUse[idx])
    {
      vectors.push_back(lockedPool.inst->_vectors[idx]);
      inUse.push_back(lockedPool.inst->_inUse[idx]);
    }
  }

  assert(vectors.size() == inUse.size());

  lockedPool.inst->_vectors.clear();
  lockedPool.inst->_inUse.clear();

  lockedPool.inst->_vectors = vectors;
  lockedPool.inst->_inUse = inUse;

  assert(lockedPool.inst->_vectors.size() == lockedPool.inst->_inUse.size());
}
