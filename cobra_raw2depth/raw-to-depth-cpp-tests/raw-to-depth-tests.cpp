/**
 * @file: raw-to-depth-tests.cpp
 * 
 * @brief Google test file for executing tests against the code within raw-to-depth-cpp
 * directory of the cobra_raw2depth repo.
 * 
 * @copyright Copyright 2023 (C) Lumotive, Inc. All rights reserved.
 * 
 */
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include <thread>
#include "RawToDepthTests.h"
#include "RawToDepth.h"
#include "RawToDepthUtil.h"
#include "RawToDepthDsp.h"
#include "LumoUtil.h"
#include "RawToFovs.h"
#include "RtdMetadata.h"
#include "MappingTable.h"
#include "FloatVectorPool.h"
#include "LumoLogger.h"

#include <climits>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <condition_variable>
#include <filesystem>

#include "LumoTimers.h"
#include "RawToDepthFactory.h"

using std::operator ""s;

constexpr float_t MHZ { 1.0e6F };

#define LLF(width, precision) std::setw(width) << std::setprecision(precision)

/**
 * Write the geometry for an output FOV to a json file.
 * 
 * @param filepath The fully qualifed path to the json output file
 * @param fovSize The two-dimensional size of the FOV (height, width)
 * @param topLeft The coordinates of the top left of the FOV in mapping table coordinates.
 * @param step The two dimensional step within the mapping table for the spatial sampling of the FOV.
 * @returns void
*/
static void dumpCoords(std::string filepath, std::vector<uint32_t> fovSize, 
                       std::vector<uint32_t> topLeft, std::vector<uint32_t> step,
                       std::array<uint32_t,2> fovTopLeft, std::array<uint32_t,2> fovStep)
{
  auto outf = std::ofstream(filepath, std::ios::out);

  outf << "{ \"size\" :[" << fovSize.at(0) << "," << fovSize.at(1) << "],\n";
  outf << "  \"topLeft\" :[" << topLeft.at(0) << "," << topLeft.at(1) << "],\n";
  outf << "  \"step\" :[" << step.at(0) << "," << step.at(1) << "],\n";
  outf << "  \"fovTopLeft\" :[" << fovTopLeft.at(0) << "," << fovTopLeft.at(1) << "],\n";
  outf << "  \"fovStep\" :[" << fovStep.at(0) << "," << fovStep.at(1) << "]\n";
  outf << "}";
}

#include <cinttypes>
/**
 * A template test that passes. Included for testing pipeline automation.
*/
TEST_F(RawToDepthTests, yes_test)
{
}

/**
 * Takes a directory of the given directory and returns all of the files with the
 * suffix ".bin".
 * 
 * @param indir The fully-qualified path to the directory containing .bin (raw ROI) files.
 * @returns A vector of strings, each of which contains the filename of a .bin file. Sorted ascending alphabetically.
*/
std::vector<std::string> findBinFiles(std::string indir)
{
  std::vector<std::string> filenames;
  for (const auto &directoryItem : std::filesystem::directory_iterator(indir)) 
  {
    if (directoryItem.path().extension() != ".bin") 
    {
      continue;
    }
    filenames.push_back(directoryItem.path());
  }

  std::sort(filenames.begin(), filenames.end());

  LLogInfo("Num files is " << filenames.size());
  return filenames;
}

/**
 * @brief Writes RawToDepth output to files in the file system. 
 * The output directory is ../../tmp.
 * 
 * @param data The output data from RawToDepth
 * @param tag A string to use during filename creation. Often related to the name of the directory containing the test data.
 * @param fovIdx The index of the FOV output.
 * @param frameCount The index of the frame of the output. Some input test data generate multiple output FOVs. Each one is
 * indexed with frameCount.
*/
void dumpData(const std::shared_ptr<FovSegment> data, const std::string &tag, int frameCount)
{
  uint32_t fovIdx = data->getFovIdx();
  std::ostringstream outfn;
  outfn << "../../tmp/cpp_range_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << std::setw(4) << std::setfill('0') << frameCount << ".bin";
  RawToDepthUtil<uint16_t>::dump(outfn.str(), *(data->getRange()));
  LLogInfo(outfn.str() << " start row " << data->getMappingTableTopLeft()[0] << " size " << data->getImageSize()[0] << "x" << data->getImageSize()[1]);

  std::ostringstream coordsfn;
  coordsfn << "../../tmp/cpp_coords_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << std::setw(4) << std::setfill('0') << frameCount << ".json";
  dumpCoords(coordsfn.str(), data->getImageSize(), data->getMappingTableTopLeft(), data->getMappingTableStep(), data->getFovTopLeft(), data->getFovStep());

  std::ostringstream signalfn;
  signalfn << "../../tmp/cpp_signal_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << std::setw(4) << std::setfill('0') << frameCount << ".bin";
  RawToDepthUtil<uint16_t>::dump(signalfn.str(), *(data->getSignal()));

  std::ostringstream bkgfn;
  bkgfn << "../../tmp/cpp_bkg_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << std::setw(4) << std::setfill('0') << frameCount << ".bin";
  RawToDepthUtil<uint16_t>::dump(bkgfn.str(), *(data->getBackground()));

  std::ostringstream snrfn;
  snrfn << "../../tmp/cpp_snr_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << std::setw(4) << std::setfill('0') << frameCount << ".bin";
  RawToDepthUtil<uint16_t>::dump(snrfn.str(), *(data->getSnr()));

}

std::vector<std::vector<uint16_t>> findRois(const std::vector<std::string> &filenames)
{
  std::vector<std::vector<uint16_t>> rois;
  for (const auto &filename : filenames)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filename);
    rois.push_back(roi);
  }
  return rois;
}

/**
 * @brief Processes all ROIs provided by the input vector and returns the data associated with each resulting FOV
 * 
 * @param rtf The RawToFovs object used to perform the computation on all of the ROIs.
 * @param rois A vector of raw ROI data.
 * @return std::list<std::pair<uint16_t, std::shared_ptr<FovSegment>>> Each pair contains first: A unique index for each output FOV, and second: the data as an FovSegment.
 */
std::list<std::pair<uint16_t, std::shared_ptr<FovSegment>>> processRois(RawToFovs &rtf, const std::vector<std::vector<uint16_t>> &rois, uint32_t firstFrameCount=0)
{
  std::list<std::pair<uint16_t, std::shared_ptr<FovSegment>>> data;
  int frameCount = (int)firstFrameCount-1;
  for (const auto &roi : rois)
  {
    auto mdat = RtdMetadata(roi.data(), roi.size()*sizeof(uint16_t));
    auto *mdat_ptr = (Metadata_t*)roi.data();
    // Often, with dumped ROIs, the disable rawtodepth bit is set during acquisition.
    for (auto fovIdx : mdat.getActiveFovs())
    {
        // Re-enable RTD processing in case it was disabled.
        mdat_ptr->perFovMetadata[fovIdx].rtdAlgorithmCommon &= uint16_t(~uint16_t(RTD_ALG_COMMON_DISABLE_RTD << MD_SHIFT));
    }

    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));

    bool incrementFrame = true;
    for (const auto fovIdx : rtf.fovsAvailable())
    {
      if (incrementFrame) // There can be multiple fovs in a single frame.
      {
        frameCount++;
      }
      incrementFrame = false;

      data.emplace_back(frameCount, rtf.getData(fovIdx));
    }
  }
  return data;
}

/**
 * @brief Given the input directory, run the raw ROIs from that directory sequentially through the
 * RawToDepth algorithms and write the output results to the ../../tmp directory.
 * 
 * @param indir The fully-qualified path to the directory containing input .bin (raw ROI) files.
 * @param tag A string describing the input data -- often the name of the directory.
 */
void processOneDirectoryOfRois(const std::string &indir, const std::string &tag)
{
  auto filenames = findBinFiles(indir);
  auto rois = findRois(filenames);
  RawToFovs rtf;
  rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/mapping_table_A.bin", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");
  auto data = processRois(rtf, rois);

  for (const auto &[frameCount, datum] : data)
  {
    dumpData(datum, tag, frameCount);
  }
  rtf.shutdown();
}

bool processOneStripe(const std::string &indir, const std::string &tag, uint32_t fileIdx)
{
  auto filenames = findBinFiles(indir);
  std::ostringstream key;
  key << "_" << std::setw(4) << std::setfill('0') << fileIdx << ".bin";
  const auto keystr = key.str();

  auto item = std::find_if(filenames.begin(), filenames.end(), 
    [keystr](const std::string &filename)-> bool 
    { 
      return std::equal(filename.end()-(int)keystr.size(), filename.end(), keystr.begin());
    }
  );
  if (item == filenames.end())
  {
    return false;
  }

  auto filename = *item;
  auto roi = RawToDepthUtil<uint16_t>::load(filename);

  RawToFovs rtf;
  rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/mapping_table_A.bin", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");
  rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));

  for (auto fovIdx : rtf.fovsAvailable())
  {
    const auto frameCount = 0;
    dumpData(rtf.getData(fovIdx), tag, frameCount);
  }

  rtf.shutdown();
  return true;

}

TEST_F(RawToDepthTests, manually_test_one_stripe)
{
  constexpr uint32_t fileIdx=17;
  auto ret = processOneStripe("../unittest-artifacts/snth_stripe_various_windows-f98-91_8linerois-bin222", "snth_stripe_various_windows-f98-91_8linerois-bin222", fileIdx);
  ASSERT_TRUE(ret);
}


void processOneDirectoryOfRoisNTimes(const std::string &indir, const std::string &tag, uint32_t numIters)
{
  auto filenames = findBinFiles(indir);
  auto rois = findRois(filenames);
  {
    RawToFovs rtf;
    rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/mapping_table_A.bin", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");

    uint32_t totalFrameCount = 0;
    for (auto iter=0; iter<numIters; iter++)
    {
      auto data = processRois(rtf, rois, totalFrameCount);

      for (const auto &[frameCount, datum] : data)
      {
        dumpData(datum, tag, frameCount);
        if (frameCount > totalFrameCount)
        {
          totalFrameCount = frameCount;
        }
      }
      if (!data.empty())
      {
        totalFrameCount++;
      }
    }
    rtf.shutdown();
  }
}

/**
 * @brief Iterate through all rois in a particular directory a number of times.
 * This routine is used (for example) to run benchmarking tests.
 * It iterates through all the ROIs, but it does not recreate the RawToFovs object,
 * nor does it reaload the ROIs between iterations.
 * 
 * @param indir The fully-qualified path to a directory containing raw ROI files sortable textually.
 * @param tag Unused in this implementation, since writing the results to the output dir is disabled.
 * @param numIters The number of times to iterate through all the given ROIs.
 */
void benchmarkOneDirectoryOfRoisNTimes(const std::string &indir, const std::string &tag, uint32_t numIters)
{
  auto filenames = findBinFiles(indir);
  auto rois = findRois(filenames);
  {
    RawToFovs rtf;
    rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/mapping_table_A.bin", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");

    for (auto iter=0; iter<numIters; iter++)
    {
      auto data = processRois(rtf, rois);
    }

    rtf.shutdown();
  }
}

/**
 * @brief Interrupts the first FOV by starting the second FOV
 * before the first is completed. This forces the scan table tag to be
 * different, causing the ROI to be ignored (and the error logged).
 * 
 * Starting the second FOV to start with the first_roi is entirely legal
 * and results in no errors, so we need to throw in a mid-FOV ROI to fire
 * this event.
 * 
 */
TEST_F(RawToDepthTests, changing_scan_table_tag)
{
  RawToFovs rtf;
  auto indir = "../unittest-artifacts/snth_nonzero_roi_start-f87-75_6linerois-bin124/"s;
  auto filenames = findBinFiles(indir);

  const int numRois = 1;
  for (auto idx=0; idx<numRois; idx++)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filenames[idx]);
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
  }

  indir = "../unittest-artifacts/snth_gaps-f98-68_8linerois-bin2"s;
  filenames = findBinFiles(indir);
  for (int idx=1; idx<filenames.size(); idx++)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filenames[idx]);
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
  }
  rtf.shutdown();
}

/**
 * @brief Modifies the random_fov_tag in the ROIs following the first.
 * 
 */
TEST_F(RawToDepthTests, changing_fov_tag)
{
  RawToFovs rtf;
  auto indir = "../unittest-artifacts/snth_nonzero_roi_start-f87-75_6linerois-bin124/"s;
  auto filenames = findBinFiles(indir);

  auto roi = RawToDepthUtil<uint16_t>::load(filenames[0]);
  rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));

  // Modify the randomFovTag for all remaining rois.
  for (int idx=1; idx<filenames.size(); idx++)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filenames[idx]);
    auto *mdat = (Metadata_t*)roi.data();
    auto fov_tag = mdat->perFovMetadata[0].randomFovTag;
    mdat->perFovMetadata[0].randomFovTag = ~mdat->perFovMetadata[0].randomFovTag;
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
  }
  rtf.shutdown();
}

TEST_F(RawToDepthTests, lidar_mode_plus_hdr)
{
  processOneDirectoryOfRois("../unittest-artifacts/lidar_mode_plus_hdr", "lidar_mode_plus_hdr");
}

TEST_F(RawToDepthTests, snth_stripe_various_windows_f98_91_8linerois_bin222)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_stripe_various_windows-f98-91_8linerois-bin222", "snth_stripe_various_windows-f98-91_8linerois-bin222");
}

TEST_F(RawToDepthTests, grid_mode_compare_office)
{
  processOneDirectoryOfRois("../unittest-artifacts/grid-mode-compare-office", "grid-mode-compare-office");
}

TEST_F(RawToDepthTests, stripe_mode_compare_office)
{
  processOneDirectoryOfRois("../unittest-artifacts/stripe-mode-compare-office", "stripe-mode-compare-office");
}

TEST_F(RawToDepthTests, stripe_mode)
{
  processOneDirectoryOfRois("../unittest-artifacts/stripe-mode", "stripe-mode");
}

TEST_F(RawToDepthTests, grid_mode)
{
  processOneDirectoryOfRois("../unittest-artifacts/grid-mode", "grid-mode");
}

TEST_F(RawToDepthTests, snth_stripe_various_binning_f98_68_8linerois_bin124)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_stripe_various_binning-f98-68_8linerois-bin124", "snth_stripe_various_binning-f98-68_8linerois-bin124");
}

TEST_F(RawToDepthTests, snth_stripe_simple_f98_68_8linerois_bin2)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_stripe_simple-f98-68_8linerois-bin2", "snth_stripe_simple-f98-68_8linerois-bin2");
}

TEST_F(RawToDepthTests, iterate_snth_simple_f98_91_8linerois_bin2)
{
  constexpr uint32_t numIters {3};
  processOneDirectoryOfRoisNTimes("../unittest-artifacts/snth_simple-f98-91_8linerois-bin2", "snth_simple-f98-91_8linerois-bin2", numIters);
}

TEST_F(RawToDepthTests, benchmark_snth_simple_f98_91_8linerois_bin2)
{
  constexpr uint32_t numIters {1010};
  benchmarkOneDirectoryOfRoisNTimes("../unittest-artifacts/snth_simple-f98-91_8linerois-bin2", "snth_simple-f98-91_8linerois-bin2", numIters);
}

TEST_F(RawToDepthTests, snth_nonzero_roi_start_f87_75_6linerois_bin124)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_nonzero_roi_start-f87-75_6linerois-bin124/", "snth_nonzero_roi_start-f87-75_6linerois-bin124");
}

TEST_F(RawToDepthTests, snth_various_binning_f87_91_6linerois_bin124)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_various_binning-f87-91_6linerois-bin124", "snth_various_binning-f87-91_6linerois-bin124");
}

TEST_F(RawToDepthTests, snth_gaps_f98_68_8linerois_bin2)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_gaps-f98-68_8linerois-bin2", "snth_gaps-f98-68_8linerois-bin2");
}

TEST_F(RawToDepthTests, snth_simple_f98_91_8linerois_bin2)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_simple-f98-91_8linerois-bin2", "snth_simple-f98-91_8linerois-bin2");
}

TEST_F(RawToDepthTests, snth_one_480_f87_1_480linerois_bin124)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_one_480-f87-1_480linerois-bin124", "snth_one_480-f87-1_480linerois-bin124");
}

TEST_F(RawToDepthTests, snth_simple_f98_88_6linerois_bin2)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_simple-f98-88_6linerois-bin2", "snth_simple-f98-88_6linerois-bin2");
}

TEST_F(RawToDepthTests, snth_tiny_f87_1_6linerois_bin2)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_tiny-f87-1_6linerois-bin2", "snth_tiny-f87-1_6linerois-bin2");
}

TEST_F(RawToDepthTests, snth_various_ghost_f87_89_6linerois_bin2222)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_various_ghost-f87-89_6linerois-bin2222", "snth_various_ghost-f87-89_6linerois-bin2222");
}

TEST_F(RawToDepthTests, snth_various_neighbor_f87_75_6linerois_bin2222)
{
  processOneDirectoryOfRois("../unittest-artifacts/snth_various_neighbor-f87-75_6linerois-bin2222", "snth_various_neighbor-f87-75_6linerois-bin2222");
}

TEST_F(RawToDepthTests, dymamic_grid_to_stripe_switching)
{
  std::string gridtag = "snth_simple-f98-88_6linerois-bin2";
  std::string stripetag = "snth_stripe_simple-f98-68_8linerois-bin2";
  std::string testdir = "../unittest-artifacts/";

  processOneDirectoryOfRois(testdir + gridtag, gridtag);
  processOneDirectoryOfRois(testdir + stripetag, stripetag);
  processOneDirectoryOfRois(testdir + gridtag, gridtag);
}

TEST_F(RawToDepthTests, test_bin_mapping_table)
{
  auto filenames = findBinFiles("../unittest-artifacts/snth_gaps-f98-68_8linerois-bin2"s);

  RawToFovs rtf;
  rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/mapping_table_A.bin", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");

  for (const auto &filename : filenames)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filename);
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
    for (auto fovIdx : rtf.fovsAvailable())
    {
      auto data = rtf.getData(fovIdx);
      auto calX = data->getCalibrationX();
      auto calY = data->getCalibrationY();
      auto calTheta = data->getCalibrationTheta();
      auto calPhi = data->getCalibrationPhi();

      auto outfn = "../../tmp/mapping_table_frombin_Out.bin"s;
      std::ofstream file(outfn);
      ASSERT_TRUE(file.is_open());
      ASSERT_TRUE(calX->size() == calY->size());
      ASSERT_TRUE(calX->size() == calTheta->size());
      ASSERT_TRUE(calX->size() == calPhi->size());

      for (auto idx=0; idx<calX->size(); idx++)
      {
        file.write(reinterpret_cast<char*>(&(calX->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calY->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calTheta->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calPhi->at(idx))), sizeof(int32_t));
      }
      return;
    }
  }
  rtf.shutdown();
}

TEST_F(RawToDepthTests, test_csv_mapping_table)
{
  auto filenames = findBinFiles("../unittest-artifacts/snth_gaps-f98-68_8linerois-bin2"s);

  RawToFovs rtf;
  rtf.reloadCalibrationData("../unittest-artifacts/mapping_table/supersampled_mapping_table.csv", "../unittest-artifacts/mapping_table/pixel_mask_A.bin");

  for (const auto &filename : filenames)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filename);
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
    for (auto fovIdx : rtf.fovsAvailable())
    {
      auto data = rtf.getData(fovIdx);
      auto calX = data->getCalibrationX();
      auto calY = data->getCalibrationY();
      auto calTheta = data->getCalibrationTheta();
      auto calPhi = data->getCalibrationPhi();

      auto outfn = "../../tmp/mapping_table_fromcsv_Out.bin"s;
      std::ofstream file(outfn);
      ASSERT_TRUE(file.is_open());
      ASSERT_TRUE(calX->size() == calY->size());
      ASSERT_TRUE(calX->size() == calTheta->size());
      ASSERT_TRUE(calX->size() == calPhi->size());

      for (auto idx=0; idx<calX->size(); idx++)
      {
        file.write(reinterpret_cast<char*>(&(calX->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calY->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calTheta->at(idx))), sizeof(int32_t));
        file.write(reinterpret_cast<char*>(&(calPhi->at(idx))), sizeof(int32_t));
      }
      return;
    }
  }
  rtf.shutdown();
}

#define INPUT_FIFO_LENGTH 32
/**
 * @brief A class to assist in a particular type of benchmarking, "with cadence."
 * This thread-safe class contains a buffer with input indices.
 * It includes notifications using a condition_variable when data arrives.
 */
class Fifo
{
public:
  uint32_t totalWrites = 0;
  std::atomic_uint32_t totalReads = 0;
  uint32_t read_idx = 0;
  uint32_t write_idx = 0;
  uint32_t totalOverruns = 0;
  std::atomic_int bufferLength = 0;
  bool someItems() { return bufferLength > 0; }

  std::mutex mtex;
  std::condition_variable cv;

  std::array<int, INPUT_FIFO_LENGTH> fifo {};
  void put(int item) 
  {
    std::lock_guard mutexLock(mtex);
    fifo.at(write_idx++) = item;
    write_idx = write_idx % INPUT_FIFO_LENGTH;
    totalWrites++;
    bufferLength++;
    if (totalWrites - totalReads > INPUT_FIFO_LENGTH) 
    {
      totalOverruns++;
    }
    // int bufferLength_tmp = bufferLength;
    // if (totalOverruns % 100) LumoLogInfo("Input Buffer overruns %d. length %d", totalOverruns, bufferLength_tmp);
    cv.notify_all();

  }

  int get()
  {
    auto item = fifo[read_idx++];
    read_idx = read_idx % INPUT_FIFO_LENGTH;
    totalReads++;
    bufferLength--;
    return item;
  }

  ~Fifo()
  {
    LLogInfo("FIFO total input buffer overruns " << totalOverruns);
  }
  
  Fifo() = default;
  Fifo(Fifo &other) = delete;
  Fifo(Fifo &&other) = delete;
  Fifo &operator=(Fifo &rhs) = delete;
  Fifo &operator=(Fifo &&rhs) = delete;

};


#include <thread>
#include <chrono>
using namespace std::chrono_literals;

/**
 * @brief A routine that runs in a separate thread and feeds indices into the Fifo at a 
 * specified rate. This rate specifies the rate at which the data is fed into 
 * RawToDepth
 * 
 * @param numRois The number of ROIs being fed to RawToDepth for one iteration of this particular test.
 * @param numiters The number of iterations (presumably the number of FOVs) fed into RawToDepth for a particular benchmark
 * @param fifo The Fifo object holding the indices for execution.
 */
static void feed(int numRois, int numiters, std::shared_ptr<Fifo> fifo)
{
  LLogInfo("Starting feed with " << numRois << " rois and " << numiters << " iters");
  for (auto iteridx=0; iteridx<numiters; iteridx++)
  {
    for (auto idx=0; idx<numRois; idx++)
    {
  #ifdef DEBUG
      auto delay = 100ms;
  #else
      auto delay = 1ms;
  #endif
      std::this_thread::sleep_for(delay);
      fifo->put(idx);
    }
  }
  LLogInfo("feed exiting.");
}

/**
 * @brief Executes a single timing benchmark for the execution time of the RawToDepth algorithms.
 * 
 * @param rois All of the ROIs for this test have been preloaded to isolate this test from file loading time.
 * @param rtf The RawToFovs object used for processing the ROIs
 * @param roiProcessingTime (input/output) The total processing time for per-roi processing is added to this variable
 * @param lastRoiProcessingTime (input/output) The total processing time for the last ROI (which include whole-frame processing) is added to this variable.
 * @param gettersProcessingTime (input/output) The total time for calling the RawToFov getters is added to this variable. This include
 * the time for formatting the output data into the correct scale and types.
 */
static inline void runOneBenchmarkTest(std::shared_ptr<std::vector<std::vector<uint16_t>>> rois, 
                                       RawToFovs &rtf, 
                                       int64_t &roiProcessingTime, int64_t &lastRoiProcessingTime, int64_t &gettersProcessingTime)
{
#ifdef DEBUG
  auto roi_delay = 0ms;
#else
  auto roi_delay = 800us;
#endif

  for (auto idx=0; idx<rois->size(); idx++)
  {
    auto &roi = rois->at(idx);
    auto roisize = roi.size()*sizeof(uint16_t);
    if (idx < rois->size()-1) 
    {
      std::this_thread::sleep_for(roi_delay); // simulate acquisition time.
      auto start = std::chrono::high_resolution_clock::now();
      rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
      roiProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
    }
    else
    {
      std::this_thread::sleep_for(roi_delay); // simulate acquisition time.
      auto start = std::chrono::high_resolution_clock::now();
      rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
      lastRoiProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
    }

    for (auto fovIdx : rtf.fovsAvailable())
    {
      auto start = std::chrono::high_resolution_clock::now();
      auto data = rtf.getData(fovIdx);
      auto rangeData = data->getRange();
      auto signalData = data->getSignal();
      auto backgroundData = data->getBackground();
      auto snrData = data->getSnr();
      gettersProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
    }
  }
}

/**
 * @brief Performs a benchmark test while the data is supplied at a specific rate.
 * Typically this rate is ~ 1/1ms, which approximately simulates the acquisition rate
 * of M25/M30. The benchmark times exclude the wait time between ROIs.
 * 
 * @param rois All ROIs used for this test, preloaded to isolate disk loading time from the benchmark.
 * @param rtf The RawToFovs object used to process the fovs
 * @param fifo The Fifo object used to signal ROI availability at a specific rate.
 * @param roiProcessingTime (input/output) The total time for just per-roi processing time is added to this parameter.
 * @param lastRoiProcessingTime (input/output) The time for processing the last ROI (which include whole-frame processing) is added to this parameter.
 * @param gettersProcessingTime (input/output) The time for retrieving, and converting/scaling, the output data is added to this parameter.
 * @param first Indicates that this is the first test. The first test is excluded from timing.
 * @param tag A string included in the filename of the output file dump.
 * @param frameIdx An index indicating which frame of this input data is being generated.
 */
static inline void benchmark_with_cadence(std::shared_ptr<std::vector<std::vector<uint16_t>>> rois,
                                          RawToFovs &rtf, 
                                          std::shared_ptr<Fifo> fifo, 
                                          int64_t &roiProcessingTime, int64_t &lastRoiProcessingTime, int64_t &gettersProcessingTime, 
                                          bool first=false, std::string tag="", int frameIdx=0)
{
  int minTime=INT_MAX; 
  int maxTime = 0;
  int sumTime = 0;
  auto numRois = rois->size();
  for  (auto idx=0; idx<numRois; idx++)
  {
    std::unique_lock<std::mutex> mutexLock(fifo->mtex);
    fifo->cv.wait(mutexLock, [fifo] { return fifo->someItems(); });
    while (fifo->someItems())
    {
      int roiIdx = idx; auto dummy = fifo->get();
      const uint16_t* roi = rois->at(roiIdx).data();
      const int numBytes = int(rois->at(roiIdx).size()*sizeof(uint16_t));
      if (idx < numRois-1)
      {
        auto start = std::chrono::high_resolution_clock::now();
        rtf.processRoi(roi, numBytes);
        if (!first)
        {
          roiProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();; 
        }
      }
      else
      {
        auto start = std::chrono::high_resolution_clock::now();
        rtf.processRoi(roi, numBytes);
        if (!first)
        {
          lastRoiProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
        }
      }

      for (auto fovIdx : rtf.fovsAvailable())
      {
        auto start = std::chrono::high_resolution_clock::now();
        auto data = rtf.getData(fovIdx);
        auto rangeData = data->getRange();
        auto signalData = data->getSignal();
        auto bkgData = data->getBackground();
        auto snrData = data->getSnr();
        if (!first)
        {
          gettersProcessingTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
        }
        
        std::ostringstream outfn;
        outfn << "../../tmp/cpp_range_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << frameIdx << ".bin";
        RawToDepthUtil<uint16_t>::dump(outfn.str(), *(data->getRange()));
        LLogInfo(outfn.str() << " size " << data->getImageSize()[0] << "x" << data->getImageSize()[1]);

        std::ostringstream signalfn;
        signalfn << "../../tmp/cpp_signal_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << frameIdx << ".bin";
        RawToDepthUtil<uint16_t>::dump(signalfn.str(), *(data->getSignal()));

        std::ostringstream bkgfn;
        bkgfn << "../../tmp/cpp_bkg_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << frameIdx << ".bin";
        RawToDepthUtil<uint16_t>::dump(bkgfn.str(), *(data->getBackground()));

        std::ostringstream snrfn;
        snrfn << "../../tmp/cpp_snr_float_as_short_" << tag << "_fov" << fovIdx << "_frame" << frameIdx << ".bin";
        RawToDepthUtil<uint16_t>::dump(snrfn.str(), *(data->getBackground()));
      }

    }
  }
}

#include <future>
#define USE_FEED  // simulate data arriving at a fixed rate.

// Unfortunately, the addition of the feed thread actually slows down RTD processing --
//   slows down the actual CPU by about 3x on my intel ubuntu laptop. I believe this is a thread scheduling 
//   issue less of an issue with Jetson.

/**
 * @brief Runs a benchmark on one directory of ROIs.
 * Marked as DISABLED because this test tends to be run manually on target hardware
 * 
 */
TEST_F(RawToDepthTests, DISABLED_raw_to_depth_tests_benchmark_cpu) // six-line rois, cpu tap rot
{
  RawToFovs rtf;
  auto fifo = std::make_shared<Fifo>();

  std::string keyword = "swnth_six-row-roi-benchmark-f87-91_6linerois-bin2";
  std::string indir = "../unittest-artifacts/" + keyword + "/";

  std::vector<std::string> filenames;
  for (const auto &thing : std::filesystem::directory_iterator(indir)) 
  {
    if (thing.path().extension() != ".bin") 
    {
      continue;
    }
    filenames.push_back(thing.path());
  }

  std::sort(filenames.begin(), filenames.end());

  LLogInfo("Num files is " << filenames.size());

  auto all_rois = std::make_shared<std::vector<std::vector<uint16_t>>>();
  for (const auto &filename : filenames)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filename);
    
    all_rois->push_back(roi);
  }

  const int numIters = 71;

#ifdef USE_FEED
  auto feed_future = std::async(std::launch::async, &feed, filenames.size(), numIters+1, fifo); // One extra iter to throw away the first one.
  LLogInfo("Feeding data at a constant rate");
#endif

  int64_t roiProcessingTime = 0;
  int64_t lastRoiProcessingTime = 0;
  int64_t gettersProcessingTime = 0;
  for (auto idx=0; idx<=numIters; idx++)
  {
    const int logEvery = 10;
    if (0 == idx%logEvery) 
    {
      LLogInfo("Running benchmarking on iteration " << idx);
    }
#ifdef USE_FEED
    benchmark_with_cadence(all_rois, rtf, fifo, roiProcessingTime, lastRoiProcessingTime, gettersProcessingTime, idx==0, "6_cpu", idx);
#else
    runOneBenchmarkTest(all_rois, rtf, roiProcessingTime, lastRoiProcessingTime, gettersProcessingTime);
#endif
  }

  // Compute the final wait time (this is latency, not added compute time)
  // wait for thread to complete

  const float_t msToSec = 0.001F;
  LLogInfo("Average of " << numIters << " iterations of " << keyword << ". \n\t" << 
           "roiProcessing  " << msToSec*float(roiProcessingTime)/float(numIters) << " ms. \n\t" << 
           "lastRoiProcess " << msToSec*float(lastRoiProcessingTime)/float(numIters) << " ms. \n\t" << 
           "gettersProcess " << msToSec*float(gettersProcessingTime)/float(numIters) << " ms. \n\t-------------------------------------------------\n\t" << 
           "Total time " << msToSec*float(roiProcessingTime + lastRoiProcessingTime + gettersProcessingTime)/float(numIters) << " ms.\n\n\n"
          );
  rtf.shutdown();
}

/**
 * @brief A utility function that makes a vector of ints mimicking the range operation in python.
 *        This range also includes a gap in the middle to simulate missing ROIs in the stream.
 * 
 * @param start The first index in the output vector.
 * @param stop One more than the last index in the output vector.
 * @param gapstart Adds a gap starting at this index.
 * @param gapsize The size of the gap.
 * @return std::vector<int> A Vector of indices incrementing from start to one less than stop, with
 * a gap that spans from gapstart, of length gapsize.
 */
static std::vector<int> make_range(int start, int stop, int gapstart=-1, int gapsize=0)
{
  auto range_vec = std::vector<int>();
  for (auto idx=start; idx<stop; idx++) 
  {
    if (idx >= gapstart && idx < gapstart+gapsize) 
    {
      continue;
    }
    range_vec.push_back(idx);
  }
  return range_vec;
}

/**
 * @brief Runs a test against a single directory of raw ROIs.
 * Running this test in Release mode also tests the asynchronous nature
 * of the multithreaded whole-frame processing. 
 * 
 * An index of this test (testCount) relative to other tests run sequentially
 * using this function is copied to the userTag, which allows the output file to be 
 * identified with the input data.
 * 
 * The same RawToFovs object is used repeated with calls to this function, which means that
 * The processing and FOV parameters can change in the course of a single ROI with no gaps 
 * between. This tests the ability of the RawToDepth codebase to respond to sudden changes to the
 * input stream.
 * 
 * @param keyword The name of the directory containing the input data.
 * @param rtf The RawToFovs object used to process this data (which may be still finalizing the previous FOV)
 * @param testCount An index unique for each call to this function. Used to tag the output file to match its input data.
 * @param roiIndices If defined, a vector of indices that contains all of the rois to include in this test. This gives the 
 * test the ability to include only a subset of the ROIs in the test.
 * @return true unconditionally.
 */
static bool bin124_testing(std::string keyword, RawToFovs &rtf, uint32_t &testCount, std::vector<int> roiIndices = std::vector<int>())
{
  testCount++;
  int fovIdx = 0;
  std::string indir = "../unittest-artifacts/" + keyword + "/";
  std::string fnbase = "snth_";

  LLogInfo("Testing from " << indir);

  auto filenames = findBinFiles(indir);

  int roiIdx = -1;
  for (const auto &filepath : filenames)
  {
    roiIdx++;
    if (!roiIndices.empty() &&
        std::none_of(roiIndices.begin(), roiIndices.end(), [roiIdx](int &val)->bool {return val == roiIdx;}))
    {
      continue;
    }
    auto roi = RawToDepthUtil<uint16_t>::load(filepath);

    auto mdat = RtdMetadata(roi);

    // Write the current test index into the user tag.
    auto *mdPtr = (Metadata_t*)roi.data();
    for (auto idx=0; idx<MAX_ACTIVE_FOVS; idx++)
    {
      mdPtr->perFovMetadata[idx].userTag = testCount << 4U;
    }

    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));
    
    for (auto fovIdx : rtf.fovsAvailable())
    {
      // Note that since RawToDepth includes multi-threaded processing for whole-frame computation,
      // this data may be retrieved from a previous call to the bin124_testing function. Thus, the
      // unique id for this test (stored in getUserTag()) has to be retrieved from the data itself.
      auto data = rtf.getData(fovIdx);
      LLogInfo("test " << data->getUserTag() << " fov " << fovIdx << 
              " size " << data->getImageSize()[0] << "x" << data->getImageSize()[1] <<
              " GCF: " << data->getGcf()/MHZ << " MHz. Max Unambiguous Range " << data->getMaxUnambiguousRange() << " m"
              );

      std::ostringstream coordsfn;
      coordsfn << "../../tmp/cpp_coords_float_as_short_test" << data->getUserTag() << "-fov" << fovIdx << "_binV2.json";
      dumpCoords(coordsfn.str(), data->getImageSize(), data->getMappingTableTopLeft(), data->getMappingTableStep(), data->getFovTopLeft(), data->getFovStep());

      std::ostringstream rangefn; rangefn << "../../tmp/cpp_range_float_as_short_test" << data->getUserTag() << "-fov" << fovIdx << "_binV2.bin";
      RawToDepthUtil<uint16_t>::dump(rangefn.str(), *(data->getRange()));

      std::ostringstream signalfn; signalfn << "../../tmp/cpp_signal_float_as_short_test" << data->getUserTag() << "-fov" << fovIdx << "_binV2.bin";
      RawToDepthUtil<uint16_t>::dump(signalfn.str(), *(data->getSignal()));

      std::ostringstream snrfn; snrfn << "../../tmp/cpp_snr_float_as_short_test" << data->getUserTag() << "-fov" << fovIdx << "_binV2.bin";
      RawToDepthUtil<uint16_t>::dump(snrfn.str(), *(data->getSnr()));

      std::ostringstream bkgfn; bkgfn << "../../tmp/cpp_bkg_float_as_short_test" << data->getUserTag() << "-fov" << fovIdx << "_binV2.bin";
      RawToDepthUtil<uint16_t>::dump(bkgfn.str(), *(data->getBackground()));
    }
  }
  return true;
}


/**
 * @brief Run a series of tests that test the ability of the RawToDepth code to produce output and
 * respond to unexpected changes in the input datastream.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_bin124Testing)
{
  RawToFovs rtf;
  uint32_t testCount = 0;

  LLogInfo(" ++++ Gaps 86 8-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_gaps-f98-68_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 91 8-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 91 8-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Testing 1 6-line ROI. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_tiny-f87-1_6linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 88 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-88_6linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 88 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-88_6linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Testing 1 6-line ROI. test%d" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_tiny-f87-1_6linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Testing 89 6-line 4-fov rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_various_ghost-f87-89_6linerois-bin2222", rtf, testCount));

  LLogInfo(" ++++ Testing 1 6-line ROI. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_tiny-f87-1_6linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Sending in a first_roi and an incomplete FOV. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_various_ghost-f87-89_6linerois-bin2222", rtf, testCount, make_range(0, 10))); // send a only a few ROIs. No errors, but only top few ROIs are there.

  LLogInfo(" ++++ Starting a new FOV without the first_roi bit. Changes image sizes in mid-stream. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount, make_range(85,91))); // No first-roi, but, huh, the previous fov0 comletedbecause last-roi bit here.
 
  LLogInfo(" ++++ Testing 75 6-line rois. various neighbor 4 fovs. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_various_neighbor-f87-75_6linerois-bin2222", rtf, testCount));

  LLogInfo(" ++++ Testing one 480-line roi, bin124. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_one_480-f87-1_480linerois-bin124", rtf, testCount));
  
  LLogInfo(" ++++ Testing 75 6-line rois various nearest neighbor with a gap in the middle. 3fovs. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_various_neighbor-f87-75_6linerois-bin2222", rtf, testCount, make_range(0, 75, 20, 30)));

  LLogInfo(" ++++ Simple 91 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 91 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 91 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  LLogInfo(" ++++ Simple 91 6-line rois. test" << testCount+1);
  ASSERT_TRUE(bin124_testing("snth_simple-f98-91_8linerois-bin2", rtf, testCount));

  rtf.shutdown();
}

/**
 * @brief Tests the ability of the RawToDepth code to dump a raw ROI to the /run 
 * directory given the proper metadata bit.
 * This test is DISABLED because there's no guarantee that the /run directory exists on
 * test machines.
 * 
 */
TEST_F(RawToDepthTests, DISABLED_raw_to_depth_tests_dumpetyDumpTest)
{
  RawToFovs rtf;
  std::string indir = "../unittest-artifacts/swnth_blks-f87-1_6linerois-bin2x2-dumpetyDump/";

  auto filenames = findBinFiles(indir);

  auto numRois = filenames.size();
  for (const auto &filepath : filenames)
  {
    auto roi = RawToDepthUtil<uint16_t>::load(filepath);
    rtf.processRoi(roi.data(), roi.size()*sizeof(uint16_t));

    std::string dumpedFp = "/run/cobra_accumulated_raw_rois_0000.bin";
    auto roiDumped = RawToDepthUtil<uint16_t>::load(dumpedFp);

    ASSERT_TRUE(roi == roiDumped);

  }
  rtf.shutdown();
}

#include "TemperatureCalibration.h"
#include "RawToDepthV2_float.h"
/**
 * @brief Run some data through the range temperature correction algorithms
 * to manually verify functionality.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_RangeTempCorrection)
{

// Testing the TemperatureCalibration class directly.
  const float_t M20_REF_RESISTANCE = 41200.0F;
  const float_t M20_EXTERNAL_VREF = 2.5F;
  const float_t M20_VLDA_SCALE = 29.70F*0.5F;
  const uint32_t M20_VLDA_ADC_IDX = 3;
  const uint32_t M20_LASER_THERM_ADC_IDX = 4;

  const float_t M25_REF_RESISTANCE = 7150.0F;
  const float_t M25_EXTERNAL_VREF = 1.22F;
  const float_t M25_VLDA_SCALE = 25.85F;
  const uint32_t M25_VLDA_ADC_IDX = 6;
  const uint32_t M25_LASER_THERM_ADC_IDX = 2;

  const float_t M30_REF_RESISTANCE = 7150.0F;
  const float_t M30_EXTERNAL_VREF = 1.22F;
  const float_t M30_VLDA_SCALE = 25.85F;
  const uint32_t M30_VLDA_ADC_IDX = 6;
  const uint32_t M30_LASER_THERM_ADC_IDX = 2;
  
  float_t REF_RESISTANCE;
  float_t EXTERNAL_VREF;
  float_t VLDA_SCALE;
  uint32_t VLDA_ADC_IDX;
  uint32_t LASER_THERM_ADC_IDX;

  TemperatureCalibration temperatureCalibration;
  std::vector<uint16_t> md_block(RtdMetadata::DEFAULT_METADATA);
  auto *mdPtr = (Metadata_t*)(md_block.data());
  auto mdat = RtdMetadata(md_block.data(), md_block.size()*sizeof(uint16_t));
  mdPtr->perFovMetadata[0].rtdAlgorithmCommon |= uint16_t(RTD_ALG_COMMON_ENABLE_TEMP_RANGE_ADJ<< 4U);

  mdPtr->system_type = (SYSTEM_TYPE_M25 << 4U);
  if (mdat.isM20())
  {
    REF_RESISTANCE = M20_REF_RESISTANCE;
    EXTERNAL_VREF = M20_EXTERNAL_VREF;
    VLDA_SCALE = M20_VLDA_SCALE;
    VLDA_ADC_IDX = M20_VLDA_ADC_IDX;
    LASER_THERM_ADC_IDX = M20_LASER_THERM_ADC_IDX;
  }
  if (mdat.isM25())
  {
    REF_RESISTANCE = M25_REF_RESISTANCE;
    EXTERNAL_VREF = M25_EXTERNAL_VREF;
    VLDA_SCALE = M25_VLDA_SCALE;
    VLDA_ADC_IDX = M25_VLDA_ADC_IDX;
    LASER_THERM_ADC_IDX = M25_LASER_THERM_ADC_IDX;
  }
  if (mdat.isM30())
  {
    REF_RESISTANCE = M30_REF_RESISTANCE;
    EXTERNAL_VREF = M30_EXTERNAL_VREF;
    VLDA_SCALE = M30_VLDA_SCALE;
    VLDA_ADC_IDX = M30_VLDA_ADC_IDX;
    LASER_THERM_ADC_IDX = M30_LASER_THERM_ADC_IDX;
  }

  LLogInfo("ADC gain " << mdat.getAdcCalGain() << ". ADC offset " << mdat.getAdcCalOffset() << ". mm per volt " <<  mdat.getRangeCalMmPerVolt(mdat.getF0ModulationIndex()));
  LLogInfo("Sweeping through values for vlda metadata.");
  LLogInfo(std::setw(10) << "Meta val" << std::setw(10) << "ADC (v)" << std::setw(10) << "VLDA (v)" << std::setw(10) << "mm_offset");
  const uint32_t vldaMetaStart = 1000;
  const uint32_t vldaMetaRange = 2500;
  const uint32_t vldaStep = 100;
  for (uint32_t vldaMetaValue = vldaMetaStart; vldaMetaValue <= vldaMetaRange; vldaMetaValue += vldaStep)
  {
    const uint32_t twelvebits = 0x3ff;
    mdPtr->adc[LASER_THERM_ADC_IDX] = twelvebits << MD_SHIFT;
    mdPtr->adc[VLDA_ADC_IDX] = vldaMetaValue << MD_SHIFT; // half-range
    temperatureCalibration.setAdcValues(mdat, 0);
    LLogInfo(LLF(10,5) << vldaMetaValue << LLF(10,5) << temperatureCalibration.getVldaAdcVoltage() << LLF(10,5) << temperatureCalibration.getVldaVoltage() << LLF(10,5) << temperatureCalibration.getVldaRangeOffsetMm());
  }

  LLogInfo("\n\n");
  LLogInfo("Sweeping through values for the temperature thermistor metadata. AdcCalGain " << mdat.getAdcCalGain() << 
           ". AdcCalOffset " << mdat.getAdcCalOffset() << ". mmperCelsius " << mdat.getRangeCalMmPerCelsius(mdat.getF0ModulationIndex()) << " \n\n");
  LLogInfo(std::setw(10) << "Meta val" << std::setw(10) << "val (v)" << std::setw(10) << "res" << std::setw(10) << "temp_c" << std::setw(10) <<"mm_offset");
  const uint32_t startAdcValue = 100;
  const uint32_t rangeAdcValue = 3200;
  const uint32_t stepAdcValue = 100;
  for (auto tempAdcValue = startAdcValue; tempAdcValue <= rangeAdcValue; tempAdcValue += stepAdcValue)
  {
    const uint32_t halfRange = 1500;
    mdPtr->adc[LASER_THERM_ADC_IDX] = tempAdcValue << 4U;
    mdPtr->adc[VLDA_ADC_IDX] = halfRange << MD_SHIFT; // half-range
    temperatureCalibration.setAdcValues(mdat, 0);
    LLogInfo(LLF(10,5) << tempAdcValue << LLF(10,5) << temperatureCalibration.getTempAdcValue() << LLF(10,5) <<
                  temperatureCalibration.getLaserThermRes() << LLF(10,5) << temperatureCalibration.getLaserTempCelsius() << LLF(10,5) << temperatureCalibration.getLaserTempRangeOffsetMm());

  }

}


static void pr(std::vector<float_t> &invec, std::vector<uint32_t> size)
{
  std::ostringstream msg;
  msg << "\n";
  auto idx = 0;
  for (auto rowidx = 0; rowidx < size[0]; rowidx++)
  {
    for (auto colidx = 0; colidx < size[1] * 3; colidx++)
    {
      const int precision = 5;
      const int group = 3;
      msg << std::setw(precision) << roundf(invec[idx++]) << " ";
      if (0 == idx % group)
      {
        msg << "-";
      }
    }
    msg << "\n";
  }
  LLogDebug(msg.str());
}

/**
 * @brief Test the FloatVectorPool class.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_FloatVectorPoolTests)
{
  const int vecSize = 1024;
  FloatVectorPool::clear();
  auto vec = FloatVectorPool::get(vecSize);
  LLogDebug("Received Float vector " << vec.get());
  ASSERT_TRUE(FloatVectorPool::exists(vec));
  ASSERT_TRUE(FloatVectorPool::size() == 1);
  ASSERT_TRUE(FloatVectorPool::numBusy() == 1);
  FloatVectorPool::release(vec);
  ASSERT_TRUE(FloatVectorPool::numBusy() == 0);

  const int numToGet = 10;
  std::vector<std::shared_ptr<std::vector<float_t>>> active_vecs;
  for (auto idx = 0; idx < numToGet; idx++)
  {
    vec = FloatVectorPool::get(vecSize);
    active_vecs.push_back(vec);
  }
  ASSERT_TRUE(FloatVectorPool::numBusy() == numToGet);

  {
    auto vec = FloatVectorPool::ScopedVector(vecSize);
    ASSERT_TRUE(FloatVectorPool::exists(vec.getVector()));
    ASSERT_TRUE(FloatVectorPool::numBusy() == 11);
  }

  ASSERT_TRUE(FloatVectorPool::numBusy() == numToGet);
  ASSERT_EQ(FloatVectorPool::size(), numToGet+1);
  FloatVectorPool::clear(); // only clears items that are not currently busy.
  ASSERT_EQ(FloatVectorPool::numBusy(), numToGet);
  ASSERT_EQ(FloatVectorPool::size(), numToGet);
  while (!active_vecs.empty())
  {
    auto &vec = active_vecs.back();
    FloatVectorPool::release(vec);
    active_vecs.pop_back();
  }
  ASSERT_EQ(FloatVectorPool::numBusy(), 0);
  ASSERT_EQ(FloatVectorPool::size(), numToGet);
  FloatVectorPool::clear();
  ASSERT_EQ(FloatVectorPool::size(), 0);

  {
    const int len = 100;
    auto vec = std::vector<uint16_t>(len);
    for (auto idx = 0; idx < vec.size(); idx++)
    {
      vec[idx] = uint16_t(rand());
    }
    SCOPED_VEC_F(v_f, len);
    const uint32_t scale = 0xffff;
    RawToDepthDsp::sh2f(vec.data(), v_f, len, 0, scale);

    ASSERT_EQ(FloatVectorPool::numBusy(), 1);
    ASSERT_EQ(FloatVectorPool::size(), 1);

    for (auto idx = 0; idx < vec.size(); idx++)
    {
      ASSERT_EQ(float_t(vec[idx]), v_f.at(idx));
    }
  }
  ASSERT_EQ(FloatVectorPool::numBusy(), 0);
  ASSERT_EQ(FloatVectorPool::size(), 1);

}

/**
 * @brief test the MAKEVECTOR macros. Features: 
 * 1. Creates a vector of the given size and type.
 * 2. Creating the same vector smaller does not result in realloc, just std::vector::resize()
 * 3. Creating the same vector larger does reallocate on the heap.
 * 4. Creating the same vector the same size does nothing.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_MAKEVECTOR_tests)
{
  bool changed = false;
  const int testVecSize = 51;
  std::vector<float_t> vec; 
  MAKE_VECTOR(vec, float_t, testVecSize);
  ASSERT_EQ(vec.size(), testVecSize);
  ASSERT_TRUE(changed);
  changed = false;

  auto v_val = reinterpret_cast<std::uintptr_t>(vec.data());

  MAKE_VECTOR(vec, float_t, testVecSize);
  ASSERT_EQ(vec.size(), testVecSize);
  ASSERT_FALSE(changed);

  auto v_val2 = reinterpret_cast<std::uintptr_t>(vec.data());
  ASSERT_EQ(v_val, v_val2);

  const int newTestVecSize = 52;
  MAKE_VECTOR(vec, float_t, newTestVecSize);
  ASSERT_EQ(vec.size(), newTestVecSize);
  ASSERT_TRUE(changed);
  auto v_val3 = reinterpret_cast<std::uintptr_t>(vec.data());
  // Note: v_val3 == v_val if the array is shrunk due to the use of resize.
  ASSERT_NE(v_val, v_val3);

}

/**
 * @brief Tests the use of the MAKEVECTOR2 macro, which allocates a pair of
 * std::vectors for storing two-component data.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_MAKEVECTOR2_tests)
{
  bool changed = false;
  const int testVecSize = 97;
  std::vector<std::vector<float_t>> vec;
  MAKE_VECTOR2(vec, float_t, testVecSize);

  ASSERT_EQ(vec.size(), 2);
  ASSERT_EQ(vec[0].size(), testVecSize);
  ASSERT_EQ(vec[1].size(), testVecSize);

  auto v_val_0 = reinterpret_cast<std::uintptr_t>(vec.data());
  auto v_val_1 = reinterpret_cast<std::uintptr_t>(vec[0].data());
  auto v_val_2 = reinterpret_cast<std::uintptr_t>(vec[1].data());
  
  MAKE_VECTOR2(vec, float_t, testVecSize);
  ASSERT_EQ(vec.size(), 2);
  ASSERT_EQ(vec[0].size(), testVecSize);
  ASSERT_EQ(vec[1].size(), testVecSize);

  auto v_val1_0 = reinterpret_cast<std::uintptr_t>(vec.data());
  auto v_val1_1 = reinterpret_cast<std::uintptr_t>(vec[0].data());
  auto v_val1_2 = reinterpret_cast<std::uintptr_t>(vec[1].data());

  // Test that reallocating the same variable name and size results in the same underlying pointer.
  ASSERT_EQ(v_val_0, v_val1_0);
  ASSERT_EQ(v_val_1, v_val1_1);
  ASSERT_EQ(v_val_2, v_val1_2);

  const int newTestVecSize = testVecSize*7;
  MAKE_VECTOR2(vec, float_t, newTestVecSize);
  ASSERT_EQ(vec.size(), 2);
  ASSERT_EQ(vec[0].size(), newTestVecSize);
  ASSERT_EQ(vec[1].size(), newTestVecSize);

  auto v_val2_0 = reinterpret_cast<std::uintptr_t>(vec.data());
  auto v_val2_1 = reinterpret_cast<std::uintptr_t>(vec[0].data());
  auto v_val2_2 = reinterpret_cast<std::uintptr_t>(vec[1].data());

  ASSERT_EQ(v_val_0, v_val2_0); // still 2-component data.
  // Reallocating the variable with a larger size results in a new
  // underlying pointer.
  ASSERT_NE(v_val_1, v_val2_1); 
  ASSERT_NE(v_val_2, v_val2_2);

}

/**
 * @brief This verifies that the fast and slow versions of smoothing produce
 * identical results.
 * 
 * The slow version of the code is general-purpose (for any kernel size).
 * The fast version of the code is specialized for a specific kernel size.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_SmoothingTest)
{
  const static float_t dataRange = 99.0F;
  const std::size_t width = 320;
  const std::size_t height = 240;
  auto inputData = std::vector<float_t>(width*height*3UL, 0.0F);
  for (auto idx=0; idx<inputData.size(); idx++)
  {
    float_t val = roundf(dataRange * float_t(std::rand()) / float_t(RAND_MAX));
    inputData.at(idx) = val;
  }

  auto outputData_fast = std::vector<float_t>(inputData.size());
  RawToDepthDsp::smoothSummedData(inputData, outputData_fast, {height,width}, 2, 3, true);

  auto outputData_good = std::vector<float_t>(inputData.size());
  RawToDepthDsp::smoothSummedData(inputData, outputData_good, {height,width}, 2, 3, false);

  for (auto idx=0; idx<outputData_fast.size(); idx++)
  {
    if (outputData_fast.at(idx) != outputData_good.at(idx))
    {
      LLogErr("Mismatch in smoothing at idx " << idx << " with values " <<  outputData_fast.at(idx) << outputData_good.at(idx));
    }
  }

}

TEST_F(RawToDepthTests, snr_weights_test)
{
  const std::vector<float_t> rawRoi {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,
                                     11,12,13,14,15,16,17,18,19,20,21,22,
                                     21,22,23,24,25,26,27,28,29,30,31,32,
                                     31,32,33,34,35,36,37,38,39,40,41,42};

  std::vector<float_t> snrWeights(rawRoi.size());

  const uint32_t roiWidth {4};
  const uint32_t roiHeight {4};
  float_t snrWeightsNumberOfSums;
  RawToDepthDsp::computeSnrSquaredWeights(rawRoi, rawRoi, snrWeights, snrWeightsNumberOfSums, roiHeight, roiWidth);

  // Verify that the columns are normalized.
  for (auto colIdx=0; colIdx<NUM_GPIXEL_PHASES*roiWidth; colIdx++)
  {
    float_t columnMax = 0.0F;
    for (auto rowIdx=0; rowIdx<roiHeight; rowIdx++)
    {
      auto val = snrWeights[colIdx + roiWidth*NUM_GPIXEL_PHASES*rowIdx];
      if (columnMax < val) 
      {
        columnMax = val;
      }
    }
    ASSERT_TRUE(fabs(1.0F-columnMax) < 1.0e-7);
  }
}


TEST_F(RawToDepthTests, test_median_1d)
{
  const std::vector<float_t> avec {20033,20085,20153,20200,20264,20317,147,135,70,33059,107,41,21,137};
  std::vector<float_t> bvec(avec.size());
  RawToDepthDsp::median1d(avec, bvec, 2);

  for (auto idx=0; idx<avec.size(); idx++)
  {
    auto aval = avec[idx];
    auto bval = bvec[idx];
    LLogInfo("a " << aval << " b " << bval);
  }
}

TEST_F(RawToDepthTests, test_lumotimers)
{
  /**
   * Measure a single event and report the elapsed time.
   * 
   */
  {
    auto tim = LumoTimers("test_lumotimers");
    tim.start("test_lumotimers_default");
    const auto delay = std::chrono::microseconds {10000};
    std::this_thread::sleep_for(delay);
    tim.stop("test_lumotimers_default");
    auto str = std::string(tim.report());
    auto pos = str.find_last_of(":");
    ASSERT_TRUE(pos != std::string::npos);
    auto timeUs =  std::stoi(str.substr(pos+1));\

    const auto tolerance = std::chrono::microseconds {1000};
    ASSERT_TRUE(timeUs >= delay.count() && timeUs < (delay+tolerance).count());
  }

  /**
   * Using the start/stop operations, show how to specify 13 consecutive timing
   * events and the output the average of all 13.
   * 
   * Note that calling report() before all 13 iterations are complete returns an empty string.
   * 
   */
  {
    auto tim = LumoTimers("test_lumotimers");
    const auto delay = std::chrono::microseconds { 500 };
    const auto tolerance = std::chrono::microseconds { 250 };

    std::string rep;
    const uint32_t numIters = 13;
    for (uint32_t iter=0; iter<numIters; iter++)
    {
      tim.start("test_lumotimers_13_iters", numIters);
      std::this_thread::sleep_for(delay);
      tim.stop("test_lumotimers_13_iters");
      rep = tim.report();
      if (iter < numIters-1)
      {
        ASSERT_TRUE(rep.empty());
      }
    }
    auto pos = rep.find_last_of(":");
    ASSERT_TRUE(pos != std::string::npos);
    auto timeUs = std::stoi(rep.substr(pos+1));

    ASSERT_TRUE(timeUs >= delay.count() && timeUs < (delay+tolerance).count());
  }

  /**
   * Using ScopedTimer, show how the timers can be used to measure 29 iterations, then
   * report the average of all 29 measurements.
   * 
   */
  {
    auto tim = LumoTimers("test_lumotimers_scoped_timer");
    const auto delay = std::chrono::microseconds { 500 };
    const auto tolerance = std::chrono::microseconds { 250 };

    std::string rep;
    const uint32_t numIters = 29;
    for (uint32_t iter=0; iter<numIters; iter++)
    {
      {
        auto timScoped = LumoTimers::ScopedTimer(tim, "scoped_timer_29_iters", numIters);
        std::this_thread::sleep_for(delay);
      }
      rep = tim.report();
    }
    
    auto pos = rep.find_last_of(":");
    ASSERT_TRUE(pos != std::string::npos);
    auto timeUs = std::stoi(rep.substr(pos+1));

    ASSERT_TRUE(timeUs >= delay.count() && timeUs < (delay+tolerance).count());
  }

  /**
   * If start/stop are called repeatedly between calls to report(), then the
   * time for all start/stop events are summed.
   * 
   */
  {
    auto tim = LumoTimers("test_lumotimers_accumulate_time");
    const auto firstDelay = std::chrono::microseconds {500};
    const auto secondDelay = std::chrono::microseconds {700};
    const auto tolerance = std::chrono::microseconds {260};

    std::string rep;
    const uint32_t numIters = 31;
    for (uint32_t iter=0; iter<numIters; iter++)
    {
      tim.start("accumulating timer", numIters);
      std::this_thread::sleep_for(firstDelay);
      tim.stop("accumulating timer");
      
      tim.start("accumulating timer", numIters);
      std::this_thread::sleep_for(secondDelay);
      tim.stop("accumulating timer");

      rep = tim.report();
      if (iter < numIters-1)
      {
        ASSERT_TRUE(rep.empty());
      }
    }

    // Show that the average time is the sum of firstDelay and secondDelay
    auto pos = rep.find_last_of(":");
    ASSERT_TRUE(pos != std::string::npos);
    auto timeUs = std::stoi(rep.substr(pos+1));
    ASSERT_TRUE(timeUs > (firstDelay+secondDelay).count() && timeUs <= (firstDelay+secondDelay+tolerance).count());
  }

}


#include <random>
/**
 * @brief Tests output logging strings.
 * 
 */
TEST_F(RawToDepthTests, raw_to_depth_tests_LogString)
{
  std::random_device randomDevice;
  std::default_random_engine randomEngine(randomDevice());
  // Enable all levels
  LLogSetLogLevel(LUMO_LOG_DEBUG);

  auto key = randomEngine();
  auto keyString = std::to_string(key);
  std::cout << "LoggingTest key is " << keyString << "\n";

  LLogDebug("Debug level log string key[" << keyString << "]");
  LLogWarning("Warning level log string key[" << keyString << "]");
  LLogErr("Err level log string key[" << keyString << "]");
  LLogDebug("Info level log string key[%s]" << keyString << "]");

  LLog(LUMO_LOG_INFO, "LoggingTest key[%" << keyString << "] with log level " << LUMO_LOG_INFO << ", LogString with an int " << 1 << " and a float " << M_PI);
  LLog(LUMO_LOG_WARNING, "LoggingTest key[" << keyString << "] with log level " << LUMO_LOG_WARNING << ", LogString with an int " << 1 << " and a float " << M_PI); 
  LLog(LUMO_LOG_ERR, "LoggingTest key[" << keyString << "] with log level " << LUMO_LOG_ERR << ", LogString with an int " << 1 << " and a float " << M_PI);
  // Enable all but debug level
  LLogSetLogLevel(LUMO_LOG_INFO);
  LLog(LUMO_LOG_DEBUG, "LoggingTest key[" << LUMO_LOG_DEBUG << "] with log level " << LUMO_LOG_DEBUG << ", LogString with an int " << 1 << " and a float " << M_PI);

  LumoLogger::setId("abcdefghijklmnopqrstuvwxyz0123456789");
  LLog(LUMO_LOG_ERR, "Logging to err with oversized name.");
  LumoLogger::setId("RawToDepth_0");

#if defined(__unix__) && !defined(LOG_TO_CONSOLE)
#include <thread>
  std::this_thread::sleep_for(std::chrono::seconds(1));
  auto numWords = LumoUtil::countWordsInFile("/var/log/syslog", keyString);
  // std::cout << "Number of " << keyString << " found in /var/log/syslog is " << numWords << "\n";
  EXPECT_EQ(numWords, 7);
#endif
  LLogSetLogLevel(LUMO_LOG_INFO);
}

/**
 * @brief This is the main entry point for Google Tests.
 * 
 */
int main(int argc, char *argv[])
{
  // Log all levels
  LLogSetLogLevel(LUMO_LOG_DEBUG);

  std::cout << "Hello world.\n";
  
#ifdef DEBUG
  std::cout << "Debug build.\n";
#endif

#ifdef NDEBUG
  std::cout << "Release build.\n";
#endif  

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
