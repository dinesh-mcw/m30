/**
 * @file pipeline_data.cpp
 * @brief This file contains the implementation for the pipeline memory manager, which
 *        maintains a memory pools for CPI returns, ROI returns, and return chunks.
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */

#include "pipeline_data.hpp"
#include <cstring>
#include <cstdlib>

using namespace LidarPipeline;

PipelineDataMM::PipelineDataMM(unsigned int ReturnChunkCount, 
                               unsigned int CPIReturnCount,
                               PipelineOutputType outputType) :
    m_PoolMut(PTHREAD_MUTEX_INITIALIZER),
    m_outputType(outputType)
{
    m_returnChunkPoolCapacity = (size_t) ReturnChunkCount;
    m_ReturnChunks.reserve(ReturnChunkCount);
    for(unsigned int i = 0; i < ReturnChunkCount; i++) {
        m_ReturnChunkPool.push_back(&m_ReturnChunks[i]);
    }

    switch (m_outputType) 
    {
        case PipelineOutputType::ProcessedData: {
            m_CPIReturns.reserve(CPIReturnCount);
            for(unsigned int i = 0; i < CPIReturnCount; i++) {
                m_CPIReturnPool.push_back(&m_CPIReturns[i]);
            }
            break;
        }
        case PipelineOutputType::RawData: {
            m_ROIReturns.reserve(ReturnChunkCount);
            for(unsigned int i = 0; i < ReturnChunkCount; i++) {
                m_ROIReturnPool.push_back(&m_ROIReturns[i]);
            }
            break;
        }
        default: { // Unhandled output type
            LLogErr("PipelineDataMM::Constructor: Creating a pipeline with an unknown output type!");
            break;
        }
    }
}

ReturnChunk* PipelineDataMM::GetReturnChunk() {
    ReturnChunk *returnChunk;
    
    pthread_mutex_lock(&m_PoolMut);
    if(m_ReturnChunkPool.empty()) {
        // Could do something graceful here like wait or grow
        // But for now fail-fast
        pthread_mutex_unlock(&m_PoolMut);
        LLogWarning("PipelineDataMM::GetReturnChunk: ReturnChunk Pool Exhausted!\n");
        return nullptr;
    }
    returnChunk = m_ReturnChunkPool.back();
    m_ReturnChunkPool.pop_back();
    pthread_mutex_unlock(&m_PoolMut);

    return returnChunk;
}

size_t PipelineDataMM::GetNumAvailableReturnChunk() {
    size_t retVal;
    pthread_mutex_lock(&m_PoolMut);
    retVal = m_ReturnChunkPool.size();
    pthread_mutex_unlock(&m_PoolMut);
    return retVal;
}

size_t PipelineDataMM::GetReturnChunkPoolCapacity() const {
    return m_returnChunkPoolCapacity;
}

CPIReturn* PipelineDataMM::GetCPIReturn() {
    CPIReturn *cpir;
    
    pthread_mutex_lock(&m_PoolMut);
    if(m_CPIReturnPool.empty()) {
        // Could do something graceful here like wait or grow
        // But for now fail-fast
        LLogErr("PipelineDataMM::GetCPIReturn: CPIReturn Pool Exhausted!");
        exit(1);
    }
    cpir = m_CPIReturnPool.back();
    m_CPIReturnPool.pop_back();
    pthread_mutex_unlock(&m_PoolMut);

    return cpir;
}

ROIReturn* PipelineDataMM::GetROIReturn() {
    ROIReturn *roir;

    pthread_mutex_lock(&m_PoolMut);
    if(m_ROIReturnPool.empty()) {
        // Could do something graceful here like wait or grow
        // But for now fail-fast
        LLogErr("PipelineDataMM::GetROIReturn: ROIReturn Pool Exhausted!");
        exit(1);
    }
    roir = m_ROIReturnPool.back();
    m_ROIReturnPool.pop_back();
    pthread_mutex_unlock(&m_PoolMut);

    return roir;
}

void PipelineDataMM::DisposeReturnChunk(ReturnChunk *done) {

    switch (m_outputType) {
        case PipelineOutputType::ProcessedData: {
            for(unsigned int i = 0; i < done->cpiReturnsUsed; i++) {
                DisposeCPIReturn(done->cpiReturns.at(i));
            }
            break;
        }
        case PipelineOutputType::RawData: {
            DisposeROIReturn(done->roiReturn);
            break;    
        }
    };

    CleanReturnChunk(done);
    pthread_mutex_lock(&m_PoolMut);
    m_ReturnChunkPool.push_back(done);
    pthread_mutex_unlock(&m_PoolMut);
}

void PipelineDataMM::DisposeCPIReturn(CPIReturn *done) {
    CleanCPIReturn(done);
    pthread_mutex_lock(&m_PoolMut);
    m_CPIReturnPool.push_back(done);
    pthread_mutex_unlock(&m_PoolMut);
}

void PipelineDataMM::DisposeROIReturn(ROIReturn *done) {
    CleanROIReturn(done);
    pthread_mutex_lock(&m_PoolMut);
    m_ROIReturnPool.push_back(done);
    pthread_mutex_unlock(&m_PoolMut);
}

void PipelineDataMM::CleanReturnChunk(ReturnChunk *toClean) {
    toClean->cpiReturnsUsed = 0;
    toClean->roiReturn = nullptr;
    toClean->extraDataItemsUsed = 0;
}

void PipelineDataMM::CleanCPIReturn(CPIReturn *toClean) {
    *toClean = {};
}

void PipelineDataMM::CleanROIReturn(ROIReturn *toClean) {
}
