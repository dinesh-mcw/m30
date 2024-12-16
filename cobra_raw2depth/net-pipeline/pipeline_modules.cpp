/**
 * @file pipeline_modules.cpp
 * @brief This file contains the implementation for the base class for
 *        pipeline modules. The pipeline module system is designed to allow
 *        you to chain networking tasks to be handled by different threads,
 *        effectively creating a pipeline.
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <pthread.h>

#include "pipeline_modules.hpp"

using namespace LidarPipeline;

PipelineModule::PipelineModule() :
    m_ownedChunksMut(PTHREAD_MUTEX_INITIALIZER),
    m_ownedChunks({}),
    m_newChunkSig(PTHREAD_COND_INITIALIZER),
    m_thread(0)
{
    m_inModule = nullptr;
    m_outModule = nullptr;
    m_running = false;
    m_memMgr = nullptr;
    m_ownedChunks_readIdx = 0;
    m_ownedChunks_writeIdx = 0;

    m_ownedChunks_len = 0; // This needs to be set to the proper size by each net-pipeline to optimize memory usage

    for (int i = 0; i < RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE; i++)
    {
        m_ownedChunks.at(i) = nullptr;
    }

}

bool PipelineModule::SetCircularBufferSize(size_t requestedSize)
{

    if (m_running)
    {
        return false;
    }

    if (requestedSize <= 0 || requestedSize > RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE)
    {
        return false;
    }

    m_ownedChunks_len = requestedSize;

    return true;
}

void PipelineModule::SetMemMgr(PipelineDataMM *memMgr)
{
    if(!m_running && memMgr != nullptr)
    {
        m_memMgr = memMgr;
    }
    else
    {
        LLogErr("PipelineModule::SetMemMgr Failed, errno=" << errno);
        exit(1);
    }
}

bool PipelineModule::IsPipelineRunning()
{
    return m_running;
}

// Spin up thread for module
void PipelineModule::StartModule()
{
    if(m_memMgr == nullptr)
    {
        LLogErr("PipelineModule::StartModule cannot proceed without a MM association.");
        exit(1);
    }

    int ret = pthread_create(&m_thread, NULL, &PipelineModule::ThreadEntry, (void *) this);

    if(ret != 0)
    {
        LLogErr("PipelineModule::StartModule pthread_create: " << ret);
        exit(1);
    }
}

// Static helper method to call PumpPineline() with object context
// (i.e. this pointer) rolled back in.
void *PipelineModule::ThreadEntry(void *module)
{
    auto *ctx = (PipelineModule*) module;

    ctx->m_ownedChunksMut = PTHREAD_MUTEX_INITIALIZER;
    ctx->m_newChunkSig = PTHREAD_COND_INITIALIZER;

    ctx->m_running = true;
    ctx->PumpPipeline();
    return nullptr;
}

// Hand in a ReturnChunk to this PipelineModule for processing. 
// Transfers ownership of ReturnChunk -- caller must not access the chunk again
void PipelineModule::HandChunkIn(ReturnChunk *inputChunk)
{
    // Pass chunk over
    pthread_mutex_lock(&m_ownedChunksMut);
    m_ownedChunks.at(m_ownedChunks_writeIdx) = inputChunk;
    
    size_t ownedChunks_len = (m_ownedChunks_len != 0) ? m_ownedChunks_len : RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE;

    uint32_t next_m_ownedChunks_writeIdx = m_ownedChunks_writeIdx + 1;
    if (next_m_ownedChunks_writeIdx >= ownedChunks_len)
    {
        next_m_ownedChunks_writeIdx = 0;
    }

    if (next_m_ownedChunks_writeIdx != m_ownedChunks_readIdx)
    {
        m_ownedChunks_writeIdx = next_m_ownedChunks_writeIdx;
    }
    else
    {
        LLogErr("PipelineModule::HandChunkIn Circular buffer is full");
        exit(1);
    }

    pthread_cond_signal(&m_newChunkSig);
    pthread_mutex_unlock(&m_ownedChunksMut);
}

// Wait for one new ReturnChunk, process it, and pass it on
// May want to override if waiting-for/inspecting multiple chunks
void PipelineModule::PumpPipeline()
{
    while(true)
    {
        // Wait for signal that we may have something new
        //pthread_cond_wait(&m_newChunkSig, &m_newChunkMut);
        //pthread_mutex_unlock(&m_newChunkMut);

        // Do we have what we need?
        // (May want to inspect multiple chunks in a derived class)
        pthread_mutex_lock(&m_ownedChunksMut);
        while(m_ownedChunks_readIdx == m_ownedChunks_writeIdx)
        {
            pthread_cond_wait(&m_newChunkSig, &m_ownedChunksMut);
        }

        // Get the chunk, then unlock the mutex so we can asyncrhonously
        // work
        ReturnChunk* targetChunk = m_ownedChunks.at(m_ownedChunks_readIdx);
        m_ownedChunks.at(m_ownedChunks_readIdx) = nullptr;

        size_t ownedChunks_len = (m_ownedChunks_len != 0) ? m_ownedChunks_len : RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE;

        m_ownedChunks_readIdx++;
        if (m_ownedChunks_readIdx >= ownedChunks_len)
        {
            m_ownedChunks_readIdx = 0;
        }

        pthread_mutex_unlock(&m_ownedChunksMut);

        // Process the chunk
        WorkOnSingleChunk(targetChunk);

        // Pass it on
        if(m_outModule != nullptr)
        {
            m_outModule->HandChunkIn(targetChunk);
        }
        // Or discard it    
        else 
        {
            m_memMgr->DisposeReturnChunk(targetChunk);
        }

    }
}

// Test Printer
void TestPrintModule::WorkOnSingleChunk(ReturnChunk *chunk)
{
    std::cout << "Got Something: Return Chunk " << (void *)chunk <<
    " : SO in Chunk = " << chunk->cpiReturnsUsed << "\n";
}

