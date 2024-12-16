#ifndef PIPELINE_DATA_HPP
#define PIPELINE_DATA_HPP

/**
 * @file pipeline_data.hpp
 * @brief This file contains definitions for two things:
 *        1. The structures used to pass data between network pipeline modules:
 *           a. The CPI return, which contains 64 pixels of depth data
 *           b. The ROI return, which contains a raw ROI
 *           c. The return chunk, which holds a bunch of CPI returns OR a single ROI return
 *        2. The pipeline memory manager, which maintains memory pools for said structures
 *        The raw ROI return is used for raw ROI streaming, which is currently not fully
 *        implemented.
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */

#include <stdint.h>
#include <vector>
#include <pthread.h>
#include <memory>
#include <RtdMetadata.h>

constexpr unsigned int MAX_CPI_PER_RETURN       { 64 };
constexpr unsigned int MAX_CPI_PER_CHUNK        { 480*10 };
constexpr unsigned int MAX_EXTRA_DATA_PER_CHUNK { 8 };
constexpr unsigned int TIMESTAMP_SIZE           { 10 };

// ROI
constexpr unsigned int MD_SIZE      { NUM_GPIXEL_PHASES * ROI_NUM_COLUMNS };
constexpr unsigned int NUM_FREQS    { 2 };
constexpr unsigned int ROI_SIZE     { 2UL*(MD_SIZE + NUM_GPIXEL_PHASES * NUM_FREQS * MAX_ROI_HEIGHT * ROI_NUM_COLUMNS) };

namespace LidarPipeline {

    /**
     *  @brief This enum distinguishes the two types of pipeline: ProcessedData (depth) and RawData (raw ROIs).
     */
    enum class PipelineOutputType {
        RawData,
        ProcessedData
    };

    struct ReturnChunkExtraData;

    /**
     *  @brief The timescale of the timestamp, which allows the receiver to compare
     *         timestamps with other timestamps in the system.
     *         TAI - International atomic time
     *         UTC - Universal coordinated time (used in Linux)
     *         GPS - Global positioning system time
     *         ARB - Arbitrary time scale (used in unsynchronized systems)
     *         Only ARB is currently supported
     */
    enum class TimestampScale {
        TAI = 0,
        UTC = 1,
        GPS = 2,
        ARB = 3
    };

    /**
     *  @brief This structure contains depth information for MAX_CPI_FOR_RETURN
     *         (64) pixels. The CPI return structure conveys this depth
     *         information from the thread that computes the it to the thread
     *         that sends it out. This structure is also used when sending out the
     *         mapping table, which is why the calibration fields are in the
     *         structure too. The calibration data is static and does not change
     *         over the lifetime of the pipeline.
     *
     *         CPI stands for Coherent Processing Interval, but this terminology
     *         is out of date, and now it simply refers to a collection of 64
     *         output pixels. This is also the same number of pixels sent in a
     *         single TCP packet.
     */
    struct CPIReturn {
        std::array<uint16_t, MAX_CPI_PER_RETURN> range;
        std::array<bool, MAX_CPI_PER_RETURN> rangeValid;
        std::array<uint16_t, MAX_CPI_PER_RETURN> intensity;
        std::array<bool, MAX_CPI_PER_RETURN> intensityValid;
        std::array<uint16_t, MAX_CPI_PER_RETURN> background;
        std::array<bool, MAX_CPI_PER_RETURN> backgroundValid;
        std::array<uint16_t, MAX_CPI_PER_RETURN> snr;
        std::array<bool, MAX_CPI_PER_RETURN> snrValid;
        std::array<uint8_t, MAX_CPI_PER_RETURN> extraAnnotation;
        std::array<uint8_t, MAX_CPI_PER_RETURN> extraAnnotationType;
        std::array<uint8_t, TIMESTAMP_SIZE> timestamp;
        TimestampScale tscale;
        uint16_t completeSizeSteerDim;
        uint16_t completeSizeStareDim;
        uint16_t startingSteerOrder;
        uint16_t startingStareOrder;
        // Hack in SWDL variable vertical crop and bin for now
        uint16_t bs_SteerOffset;
        uint16_t bs_SteerStep;
        uint16_t bs_StareOffset;
        uint16_t bs_StareStep;
        uint16_t bs_UserTag;

        bool metaSupressStream;
        bool metaLastCpiInFrame;
        bool metaLastCpiInBuffer;

        bool prefixMetaDataUpdate;

        std::shared_ptr<std::vector<int32_t>> calibrationX;
        std::shared_ptr<std::vector<int32_t>> calibrationY;
        std::shared_ptr<std::vector<int32_t>> calibrationTheta;
        std::shared_ptr<std::vector<int32_t>> calibrationPhi;
    };

    // ROIs
    struct ROIReturn {
        std::array<char, ROI_SIZE> roi;
    };

    /**
     *  @brief This structure contains a collection of CPI return structures
     *         and represents an entire field of view (FoV). It is the parent
     *         struct that conveys depth information from the thread
     *         generating depth information and the thread sending it.
     *
     *         The return chunk is also used in the raw data pipeline, but
     *         in this case it contain roiReturn and not cpiReturns.
     *
     *         The extraDataItems member is not currently being used.
     */
    struct ReturnChunk {
        std::array<CPIReturn*, MAX_CPI_PER_CHUNK> cpiReturns;
        uint32_t cpiReturnsUsed;
        ROIReturn* roiReturn; // 1 RoiReturn per ReturnChunk
        std::array<ReturnChunkExtraData*, MAX_EXTRA_DATA_PER_CHUNK> extraDataItems;
        uint32_t extraDataItemsUsed;
    };

    /**
     *  @brief Allows extra data to be attached to a return chunk. Not currently used.
     */
    struct ReturnChunkExtraData {
    };

    /**
     *  @brief Pipeline data memory manager singleton class. Maintains three memory pools:
     *         1. Return chunk pool, which contains return chunks
     *         2. CPI return pool, which contains CPI returns
     *         3. Raw ROI return pool, which contains entire raw ROIs
     *         Memory pools allow O(0) memory allocation overhead
     */
    class PipelineDataMM {
        public:
            PipelineDataMM(unsigned int ReturnChunkCount, 
                unsigned int CPIReturnCount, PipelineOutputType outputType);
            ReturnChunk* GetReturnChunk();
            size_t GetNumAvailableReturnChunk();
            size_t GetReturnChunkPoolCapacity() const;
            CPIReturn* GetCPIReturn();
            ROIReturn* GetROIReturn();
            void DisposeReturnChunk(ReturnChunk *done);
            void DisposeCPIReturn(CPIReturn *done);
            void DisposeROIReturn(ROIReturn *done);
        private:
            size_t m_returnChunkPoolCapacity;
            std::vector<ReturnChunk> m_ReturnChunks;
            std::vector<ReturnChunk*> m_ReturnChunkPool;
            std::vector<CPIReturn> m_CPIReturns;
            std::vector<CPIReturn*> m_CPIReturnPool;
            std::vector<ROIReturn> m_ROIReturns;
            std::vector<ROIReturn*> m_ROIReturnPool;

            pthread_mutex_t m_PoolMut;
            PipelineOutputType m_outputType;
            static void CleanReturnChunk(ReturnChunk *toClean);
            static void CleanCPIReturn(CPIReturn *toClean);
            static void CleanROIReturn(ROIReturn *toClean);
    };

};

#endif

