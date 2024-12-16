# raw-to-depth-cpp-tests

The unit tests for exercising the RawToDepth numerical algorithms.

## Setup

Install google test from here
https://github.com/google/googletest/blob/main/googletest/README.md

Install the llvm tools and enable clang++ by adding the following to .profile

* export CC=/usr/bin/clang 
* export CXX=/usr/bin/clang++ 

### Create the following directory structure
Sync the following directories parallel to each other into the same subdirectory.
The cobra_raw2depth and lumotive_stream_receiver repos are available via git.
Create the tmp directory manually.

* cobra_raw2depth
* lumotive_stream_receiver
* tmp

Install git-lfs using "sudo apt-get install git-lfs"
Enable git-lfs by running "git lfs install"
Pull the RawToDepth unit test data by entering the cobra_raw2depth directory and executing "git lfs pull"

### Build RawToDepth
The unit tests for RawToDepth need to be run in Debug mode. Defining LOG_TO_CONSOLE prints the output of the logger to the screen, otherwise it gets written into the linux system log.

* cd cobra_raw2depth
* git lfs pull
* mkdir build
* cd build
* cmake .. -DDEFINE_LOG_TO_CONSOLE=ON -DCMAKE_BUILD_TYPE=DEBUG
* make -j8

### Build lumotive_stream_receiver

* cd lumotive_stream_receiver/cpp_client
* mkdir build
* cd build
* cmake ..
* make -j8

### Execute one RawToDepth test.
This test was created to simply test that the execution is working correctly.

* cd cobra_raw2depth/build
* raw-to-depth-cpp-tests/raw-to-depth-tests --gtest_filter=RawToDepthTests.yes_test

### Run a RawToDepth test that utilizes the unittest-artifacts data
This executes a single test that processes the data from ../unittest-artifacts/snth_gaps_f98_68_8linerois_bin2 and places the results into ../../tmp.

* cd cobra_raw2depth/build
* raw-to-depth-cpp-tests/raw-to-depth-tests --gtest_filter=RawToDepthTests.snth_gaps_f98_68_8linerois_bin2

### Run all RawToDepth standalone tests.

* cd cobra_raw2depth/build
* raw-to-depth-cpp-tests/raw-to-depth-tests

### Run the RawToDepth end-to-end test.
This is the preferred, and most thorough, way of running RawToDepth unit tests.

This test executes the RawToDepth algorithms 3 times: 
* Once using c++ code via the unit tests, 
* Once using python code to verify the numerical integrity of the c++ code, and
* Once using the mocked frontend that reads files from the unittest-artifacts directory, 
  passes the results onto the network, and received by the lumotive_stream_receiver cpp_client.

The results are all written to disk into the ../../tmp directory, and compared against each other.

For a more detailed description of this test, see the comments in raw-to-depth-cpp-tests/verifyAgainstLsrClient.py.
Examine the imports from that python file to pip install the proper libraries.

Build all RawToDepth executables as above.

If you're running on an NCB be sure to shut down the front end

* systemctl stop frontend

* cd cobra_raw2depth/build
* python3 ../raw-to-depth-cpp-tests/verifyAgainstLsrClient.py
