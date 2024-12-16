/**
 * @file LumoAffinity.h
 * @brief Processor affinity support
 *
 * @copyright Copyright (c) 2023 Lumotive, Inc. All rights reserved.
 *
 */

#include "LumoAffinity.h"
#include "LumoLogger.h"
#include <sys/stat.h>

constexpr int CPU_COUNT { 6 };

bool LumoAffinity::isNCB() {
    static int _isNCB = -1;
    struct stat stat_buf{};
    if (_isNCB < 0) {
        // consider this an NCB if it contains the /etc/lumotive_fs_rev file
        _isNCB = (stat("/etc/lumotive_fs_rev", &stat_buf) < 0) ? 0 : 1;
    }
    return _isNCB != 0;
}

void LumoAffinity::setAffinity(cpu_set_t *cpuSetP) {
    int error = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), cpuSetP);

    if (error != 0) {
        LLogWarning("setaffinity_failed:error=" << error);
    }
}

void LumoAffinity::setAffinity(int processor) {
    // Don't set affinity on the PC
    if (!LumoAffinity::isNCB()) {
        return;
    }

    if (processor < 0 || processor >= CPU_COUNT) {
        LLogWarning("setAffinity_param:processor=" << processor << ",min=0,max=" << CPU_COUNT);
    } else {
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        CPU_SET(processor, &cpuSet);
        setAffinity(&cpuSet);
    }
}

void LumoAffinity::setAffinity(std::vector<int> processors) {
    // Don't set affinity on the PC
    if (!LumoAffinity::isNCB()) {
        return;
    }

    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    for (auto processor = processors.begin(); processor != processors.end(); processor++) {
        if (*processor < 0 || *processor >= CPU_COUNT) {
            LLogWarning("setAffinity_param:processor=" << *processor << ",min=0,max=" << CPU_COUNT);
        } else {
            CPU_SET(*processor, &cpuSet);
        }
    }
    setAffinity(&cpuSet);
}
