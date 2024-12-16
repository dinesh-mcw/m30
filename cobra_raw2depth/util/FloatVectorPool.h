/**
 * @file FloatVectorPool.h
 * @brief Implements a dynamic pool for std::vector<float_t>.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <cmath>
#include "LumoLogger.h"
#include <mutex>
#include <thread>

#ifdef __clang__
#define SCOPED_VEC_F(a, size) auto a##_scoped = FloatVectorPool::ScopedVector(size); auto &(a) = *(a##_scoped.getVector()); 
#else
#define SCOPED_VEC_F(a, size) auto a##_scoped = FloatVectorPool::ScopedVector(size); auto &a = *(a##_scoped.getVector()); 
#endif

class SyncedFloatVectorPool;

class FloatVectorPool {
 public:

  class ScopedVector {
  private:
    std::shared_ptr<std::vector<float_t>> _vec;
    
  public:
  explicit ScopedVector(uint32_t size) :
    _vec(FloatVectorPool::get(size)) {
    }
    
    ~ScopedVector() {
      FloatVectorPool::release(_vec);
    }
    std::shared_ptr<std::vector<float_t>> getVector() { return _vec; }

    ScopedVector(ScopedVector &other) = delete;
    ScopedVector(ScopedVector &&other) = delete;
    ScopedVector *operator=(ScopedVector &rhs) = delete;
    ScopedVector *operator=(ScopedVector &&rhs) = delete;
  };
  
 private:

  static std::shared_ptr<FloatVectorPool> _inst;
  std::vector<std::shared_ptr<std::vector<float_t>>> _vectors;
  std::vector<bool> _inUse;
  std::mutex _mutex;
  static std::mutex _getterMutex;

  static int find(uint32_t size, std::shared_ptr<FloatVectorPool> inst);
  static SyncedFloatVectorPool getInst();

  
 public:
  static void clear();
  static std::shared_ptr<std::vector<float_t>> get(uint32_t size);
  static void release(std::shared_ptr<std::vector<float_t>> vec);

  static int size(); // used in testing
  static bool exists(std::shared_ptr<std::vector<float_t>> vec); // used in testing
  static int numBusy();

  void lock() { _mutex.lock(); }
  void unlock() { _mutex.unlock(); }
};


class SyncedFloatVectorPool {
public:
  std::shared_ptr<FloatVectorPool> inst;

  explicit SyncedFloatVectorPool(std::shared_ptr<FloatVectorPool> pool) :
    inst(pool)
  {
    inst->lock();
  }

  ~SyncedFloatVectorPool()
  {
    inst->unlock();
  }

  SyncedFloatVectorPool(SyncedFloatVectorPool &&other) = delete;
  SyncedFloatVectorPool(SyncedFloatVectorPool &other) = delete;
  SyncedFloatVectorPool &operator=(SyncedFloatVectorPool &rhs) = delete;
  SyncedFloatVectorPool &operator=(SyncedFloatVectorPool &&rhs) = delete;
};
