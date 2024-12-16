#pragma once

/**
 * @file LumoAffinity.h
 * @brief Processor affinity support
 *
 * @copyright Copyright (c) 2023 Lumotive, Inc. All rights reserved.
 *
 */

#include <sched.h>
#include <vector>

class LumoAffinity {
public:
    static constexpr int A72_0 { 4 };
    static constexpr int A72_1 { 5 };
    static constexpr int A53_0 { 0 };
    static constexpr int A53_1 { 1 };
    static constexpr int A53_2 { 2 };
    static constexpr int A53_3 { 3 };
    static void setAffinity(int processor);
    static void setAffinity(std::vector<int> processors);
private:
    static void setAffinity(cpu_set_t *cpuSetP);
    static bool isNCB();
};
