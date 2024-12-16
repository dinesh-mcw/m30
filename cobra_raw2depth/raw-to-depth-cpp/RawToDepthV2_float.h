/**
 * @file RawToDepthV2_float.h
 * @brief Specialization of the RawToDepth class that implements the float-point
 *        RawToDepth algorithm set.
 * 
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 * 
 */

#pragma once

#include "RawToDepth.h"
#include <future>

  /**
   * @brief Private struct used to peel away data that's used in the processWholeFrame thread.
   *        The data in this struct is a copy of the needed data so that the state of the 
   *        container (RawToDepthV2_float) doesn't need to stay static during the execution of the
   *        separate thread.
   * 
   */
  typedef struct LocalProcessFrameInfo
  {
    bool quitNow = false;
    bool dataReady = false;
    bool dataProcessed = true;
    std::shared_ptr<std::condition_variable> conditionVariable;
    std::shared_ptr<std::mutex> mutex = nullptr;
    std::function<void (std::shared_ptr<FovSegment>)> setFovSegment = [](std::shared_ptr<FovSegment>){} ;
    uint32_t fovIdx = 0;
    std::array<uint32_t, 2> binning = {2,2};
    std::array<uint32_t, 2> size = {MAX_IMAGE_HEIGHT,IMAGE_WIDTH};
    uint32_t rowKernelIdx = 0;
    uint32_t columnKernelIdx = 0;
    std::array<float_t, 2> fs = {0,0};
    std::vector<float_t> fsInt = {0,0};
    float_t c = C_MPS;
    std::vector<uint32_t> minMaxFilterSize;
    bool performGhostMedian = false;
    uint16_t nearestNeighborFilterLevel = 0;
    uint32_t headerNum = 0;
    uint64_t timestamp = 0;
    uint16_t sensorId = 0;
    uint32_t userTag = 0;
    bool lastRoiReceived = false; ///< Indicates whether the final ROI in the FOV was received.
    bool incompleteFov = false;
    double GCF = 0;
    double maxUnambiguousRange = 0;
    std::array<uint32_t,2> imageStart = {0,0};
    std::array<uint32_t,2> imageStep = {2,2};
    std::vector<int32_t>  roiIndexFrame = {};
    std::vector<uint64_t> timestamps = {}; ///< 64-bit timestamp, that is the lower 60 bits of the 7 12-bit metadata values.
    std::vector<std::vector<uint32_t>> timestampsVec = {}; ///< Newer timestamp format, in which all 94 bits are split between 3 32-bit unsigned ints.
    std::string lastTimerReport;
    std::shared_ptr<std::vector<uint16_t>> pixelMask = nullptr;
    std::array<uint16_t,2> fovStart = {0,0};
    std::array<uint16_t,2> fovStep = {0,0};
    std::array<uint16_t,2> fovSize = {0,0}; ///< pre-binned
    uint16_t fovNumRois = 0;
    int32_t lastRoiIdx = 0; ///< The last value of _currentRoiIdx, which should equal the number of expected ROIs in this FOV.
    bool disableRangeMasking = false;
    float_t snrThresh = 0;
    float_t rangeOffsetTemperature = 0;
    bool disableRtd = false;
    std::vector<bool> activeRows = {};
    std::shared_ptr<LumoTimers> timers = nullptr;
    float_t rangeLimit = 0.0F;
    std::vector<float_t> *rawFrame0 = nullptr;
    std::vector<float_t> *rawFrame1 = nullptr;
  } LocalProcessFrameInfo;


/**
 * @brief Specialization of the RawToDepth class that implements the float-point
 *        RawToDepth algorithm set.
 * 
 */
class RawToDepthV2_float : public RawToDepth
{
 protected:

  // Ping-pong buffers are used for processWholeFrame multithreading.
  std::array<
    std::vector<
      std::vector<float_t>>, 2>  _fRawFrames; ///< DSP intermediate value: output of pre-binning snr voting. pingPong, frequency, pixels 
  // Since the data is snr-voted into the raw buffer directly from the input ROI, it's possible that there can be
  // gaps between the ROIs during acquisition. Therefore, this variable keeps track of the rows of the prebinned buffer than 
  // contain valid data so that they can be filled via interpolation.
  std::array<
    std::vector<bool>,2>       _activeRows; ///< DSP intermediate value: One entry per pre-binned row. True if input rois had data in the row.
  std::array<
    std::vector<int32_t>,2>    _roiIndexFrames; ///< ping-pong. Each output pixel is assigned the index of the input roi in arrival order.
  std::vector<float_t>         _fovSnrV2; ///< internal snr used for pre-binning snr-voting
  uint32_t _rawPingOrPong=0;


  bool _wholeFrameRunning;
  std::future<void> _wholeFrameRunningFuture; ///< Holds the future for the thread that runs the whole-frame processing.
  std::shared_ptr<std::mutex> _wholeFrameRunningMutex;
  std::shared_ptr<std::condition_variable> _wholeFrameRunningConditionVariable;
  std::shared_ptr<LocalProcessFrameInfo>   _wholeFrameRunningData;

  bool     _performGhostMedian {false}; ///< (from metadata) Enable a 2D median filter on the output range values
  bool     _performGhostMinMax {false}; ///< (from metadata) Enable the min-max filter on the intermediate value "M"

  uint32_t _rowKernelIdx = 1; ///< Processing parameters: raw data smoothing filter kernel indices
  uint32_t _columnKernelIdx = 1;
  bool saveTimestamp(RtdMetadata &mdat) override;

public:
  explicit RawToDepthV2_float(uint32_t fovIdx, uint32_t headerNum);
  RawToDepthV2_float() = delete;
  RawToDepthV2_float(RawToDepthV2_float &other) = delete;
  RawToDepthV2_float(RawToDepthV2_float &&other) = delete;
  RawToDepthV2_float *operator=(RawToDepthV2_float &rhs) = delete;
  RawToDepthV2_float *operator=(RawToDepthV2_float &&rhs) = delete;
  ~RawToDepthV2_float() override;
  void shutdown() override;
  
  // Called once per received ROI.
  void processRoi(const uint16_t* roi, uint32_t numBytes) override;
  // Called once when (RtdMetadata::frameCompleted() == true) to do whole-FOV processing.
  // This call is asynchronous and returns immediately.
  void processWholeFrame(std::function<void (std::shared_ptr<FovSegment>)> setFovSegment) override;

  // These overridden routines need to be implemented for the data buffers in the subclass.
  void reset(const uint16_t *mdPtr, uint32_t mdBytes) override; ///< called when first-roi-in-frame is received.
  bool bufferSizesChanged(RtdMetadata &mdat) override;

private:
  void realloc(const uint16_t *mdPtr, uint32_t mdBytes);
  static void processOneRoi(RawToDepthV2_float *inst, const uint16_t *roi, uint32_t numBytes);
  std::future<void> _localProcessRoiFuture;
  // RoiIndices is an FOV-sized buffer containing indices indicating which ROI was used to generate
  // each pixel. These indices can be used to lookup the timestamp for each individual pixel.
  static std::shared_ptr<std::vector<uint16_t>> getRoiIndices(std::vector<int32_t> &roiIndices, 
                                                              std::array<uint16_t,2> fovStart, 
                                                              std::array<uint16_t,2> fovStep, 
                                                              std::array<uint16_t,2> fovSize, 
                                                              std::array<uint32_t,2> size);

  static void localProcessFrame(std::shared_ptr<LocalProcessFrameInfo> info);
  static void processWholeFrameEventLoop(std::shared_ptr<LocalProcessFrameInfo> infoPtr);

};
