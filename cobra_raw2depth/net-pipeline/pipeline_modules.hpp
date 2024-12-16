#ifndef PIPELINE_MODULES_HPP
#define PIPELINE_MODULES_HPP

/**
 * @file pipeline_modules.hpp
 * @brief This file defines the base class for pipeline modules. The
 *        pipeline module system is designed to allow you to chain
 *        networking tasks to be handled by different threads, effectively
 *        creating a pipeline.
 *
 *        The raw to depth pipelines only use a single module called the
 *        TCPWrappedStreamer, which is a subclass of the NetworkStreamer
 *        class, which itself is a subclass of the PipelineModule class.
 *        A UDPStreamer is also available, but it is not supported and
 *        subject to change.
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */

#include <pthread.h>
#include <queue>
#include "pipeline_data.hpp"

#define RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE    100

namespace LidarPipeline {
    /**
     *  @brief Base class for pipeline modules. Every pipeline module represents a
     *         stage of the pipeline that encapsulates a queue of "chunks" and a
     *         thread that performs work on the chunks it pulls from the queue.
     *
     *         Although the pipeline can be multiple stages, we only use a single
     *         stage pipeline for depth streaming and raw ROI streaming, the latter
     *         which isn't currently supported.
     *
     *         The PipelineModule uses pthread_create to create the thread. The
     *         C thread function is called ThreadEntry. It calls PumpPipeline,
     *         which is the main loop for the thread. This function pulls chunks
     *         from the queue and calls the pure virtual WorkOnSingleChunk
     *         function, which is the one function that subclasses MUST implement.
     */
    class PipelineModule {
        public:
            PipelineModule();
            virtual void StartModule();
            virtual void SetMemMgr(PipelineDataMM *memMgr);
            virtual void HandChunkIn(ReturnChunk *inputChunk);
            virtual bool SetCircularBufferSize(size_t requestedSize);
            virtual bool IsPipelineRunning();
        protected:
            // A common case is processing a single chunk, so we build it in 
            // here as the default. It is possible to override PumpPipeline()
            // do something more custom with multiple chunks, however. See implementation of PumpPipeline() as a starting point.
            virtual void WorkOnSingleChunk(ReturnChunk *chunk) = 0;
        private:
            static void *ThreadEntry(void *Module);
            virtual void PumpPipeline();
            pthread_mutex_t m_ownedChunksMut;
            std::array<ReturnChunk*, RETURNCHUNK_MAX_CIRCULAR_BUFFER_SIZE> m_ownedChunks;
            uint32_t m_ownedChunks_readIdx;
            uint32_t m_ownedChunks_writeIdx;
            pthread_cond_t m_newChunkSig;
            PipelineModule *m_inModule;
            PipelineModule *m_outModule;
            size_t m_ownedChunks_len;
            pthread_t m_thread;
            bool m_running;
            PipelineDataMM *m_memMgr;
    };

    class TestPrintModule : public PipelineModule {
        protected:
            void WorkOnSingleChunk(ReturnChunk *chunk) override;
    };

};

#endif
