"""

file: verifyAgainstLsrClient.py

Copyright 2023 (C) Lumotive, Inc. All rights reserved.

This script executes the RawToDepth algorithms twice and compares the outputs.
The first execution uses the raw-to-depth-tests Google test suite to process a
directory of raw ROIs and put the results into the ../../tmp directory.

The second execution calls the frontend in mock mode to process the same directory
of input data, passes the data through the network stack, then the result is received
by the Lumotive Stream Receiver sample client application. 

The sample client has been modified to save the received data.

With two sets of processed FOVs in the ../../tmp directory, this python script then 
loads and compares the files processed through the different pipelines.

This test does not verify RawToDepth execution, but what it does do is verify the connection
from frontend to RawToDepth, then the interface between RawToDepth and the networking code,
and the network protocols as the data is passed to the Lumotive Stream Receiver client.

Since it's possible for the input data to include multiple virtual sensors (IOW, multiple output
FOVs), then this code spins up enough instances of the Lumotive Stream Receiver sample client
to receive each one and dump the results independently to the ../../tmp directory.

The following assumptions are necessary for this test to execute properly.
# Assumes debug mode build for RawToDepth.
# Assumes that cwd is cobra_raw2depth/build
# Assumes that cobra_raw2depth is built both for raw-to-depth-tests and frontend executables
  inside the cobra_raw2depth/build directory.
# Assumes that lumotive_stream_receiver is built for the cpp_client executable.
  inside the lumotive_stream_receiver/cpp_client/build directory. Synced from "develop."
# Assumes this command has been executed
  - sudo sysctl -w net.core.wmem_max=181632000
# Assumes that the PYTHONPATH environment points to the src directory. If running these tests from
  the cobra_raw2depth/build directory, set PYTHONPATH as :
  - export PYTHONPATH=../cobra_raw2depth/src
# Assumes that the directory structure has all of these repos/dirs in parallel:
  - cobra_raw2depth
  - lumotive_stream_receiver
  - tmp

"""


import numpy as np
from os import system
import os
import fnmatch
import argparse
import json
import sys
import subprocess
import time
import shlex
import m30_verification as v
import M30Metadata as md
import argparse
from pathlib import Path



def run_test(tag, file_prefix, number_of_fovs=1, number_of_frames=1, omit_end_to_end=False) :
    """ Verifies execution of the RawToDepth unit test against the end-to-end test from frontend
        through Lumotive Stream Receiver.

        Executes a test against a single directory full of raw ROIs.
    """
    print(f'RUNNING TEST TO COMPARE RTD WITH LSR ON THE TEST DIRECTORY {tag}')
    cwd = os.getcwd()
    os.makedirs('../../tmp', exist_ok=True)
    system('rm -f ../../tmp/*.*')
    system('killall -q frontend')

    # Look into the input directory and count the number of raw files to be processed.
    number_of_rois = len(fnmatch.filter(os.listdir(f'../unittest-artifacts/{tag}/'), '*.bin'))
    if number_of_rois == 0 :
        exit(-1)
    print(f'Number of rois in ../unittest-artifacts/{tag} is {number_of_rois}')

    # Run the python algorithms
    input_dir = os.path.join('..', 'unittest-artifacts', tag)
    print(f'{md.currentFile(__file__)} - Running the python computation on the rois in {input_dir}')
    dsp_config = {
      'output_intermediate_results' : False,
      'input_dir': input_dir,
      'tag' : os.path.basename(input_dir),
      'output_dir': os.path.join('..', '..', 'tmp')
      }
    v.process_rois(dsp_config)

    print(f'Running the unit test against the directory {tag}')
    test_name = tag.replace('-', '_') # The Google Test name matches the directory name, but '-' is an invalid character in a C++ identifier.
    subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.' + test_name], cwd=cwd)
   
    if not omit_end_to_end :
        # Start up as many instances of the Lumotive Stream Receiver sample client application as there are
        # FOVs specified by the input data.
        lsr_processes = []

        for fov_idx in range(int(number_of_fovs)) :
            net_range_fpath = f'../../tmp/net_range_float_as_float_' + tag + f'_fov{fov_idx}_frame'
            net_signal_fpath = f'../../tmp/net_signal_float_as_short_' + tag + f'_fov{fov_idx}_frame'
            net_snr_fpath = f'../../tmp/net_snr_float_as_short_' + tag + f'_fov{fov_idx}_frame'
            net_bkg_fpath = f'../../tmp/net_bkg_float_as_short_' + tag + f'_fov{fov_idx}_frame'

            print(f'Running lumotive_stream_receiver client from {cwd} for fov {fov_idx}')
            lsr_process = subprocess.Popen(['../../lumotive_stream_receiver/cpp_client/build/cpp_client', '127.0.0.1', f'{12566 + fov_idx}', str(num_frames),  
                net_range_fpath,
                net_signal_fpath,
                net_snr_fpath,
                net_bkg_fpath],
                cwd=cwd
            )
            lsr_processes.append(lsr_process)

        # This sleep is added to make sure the receivers are started prior to the starting of the data transfer.
        time.sleep(0.1)

        fe_process_options = shlex.split(f'front-end-cpp/frontend --base-port=12566 ' + 
                f'--mock-prefix="../unittest-artifacts/{tag}/{file_prefix}" ' + 
                f'--mock-delay=100 --num-heads=1 ' + 
                f'-c "../unittest-artifacts/mapping_table/supersampled_mapping_table.csv" ' +
                f'-p "../unittest-artifacts/mapping_table/pixel_mask_A.bin"'
                )
        
        print(f'Running cobra frontend from {cwd} with {fe_process_options}')
        fe_process = subprocess.Popen(fe_process_options, cwd=cwd)
        
        for lsr_process in lsr_processes :
            lsr_process.wait()

        fe_process.kill()
        fe_process.wait()
    

    success = True
    for frame_idx in range(int(number_of_frames)) :
        for fov_idx in range(int(number_of_fovs)):
            # This file is created by raw-to-depth-tests
            coords_fpath = '../../tmp/cpp_coords_float_as_short_'+ tag + f'_fov{fov_idx}_frame{frame_idx:04}.json'
            with open(coords_fpath) as coords_file:
                coords = json.load(coords_file)
            
            datashape = coords['size']

            cpp_range_fpath = f'../../tmp/cpp_range_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'
            python_range_fpath = f'../../tmp/python_range_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'

            cpp_signal_fpath = f'../../tmp/cpp_signal_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'
            python_signal_fpath = f'../../tmp/python_signal_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'

            cpp_snr_fpath = f'../../tmp/cpp_snr_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'
            python_snr_fpath = f'../../tmp/python_snr_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'

            cpp_bkg_fpath = f'../../tmp/cpp_bkg_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'
            python_bkg_fpath = f'../../tmp/python_bkg_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.bin'

            cpp_coords_fpath = f'../../tmp/cpp_coords_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.json'
            python_coords_fpath = f'../../tmp/python_coords_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx:04}.json'

            with open(cpp_coords_fpath) as cpp_file:
                cpp_coords = json.load(cpp_file)

            with open(python_coords_fpath) as py_file:
                py_coords = json.load(py_file)
            
            if cpp_coords != py_coords:
                print(f'Failure. Comparing coordinates from python and cpp for FPV {fov_idx} and frame {frame_idx}.')
                print(f'cpp coords: {cpp_coords}')
                print(f'python coords: {py_coords}')
                success = False

            cpp_range = np.reshape(np.fromfile(cpp_range_fpath, dtype=np.uint16), datashape)
            python_range = np.reshape(np.fromfile(python_range_fpath, dtype=np.uint16), datashape)
            numDiffs = np.count_nonzero(cpp_range-python_range)
            if numDiffs != 0 :
                print(f'Failure. {numDiffs} diffs comparing cpp and python range for FOV {fov_idx} and frame {frame_idx}.')
                success = False
            
            cpp_signal = np.reshape(np.fromfile(cpp_signal_fpath, dtype=np.uint16), datashape)
            python_signal = np.reshape(np.fromfile(python_signal_fpath, dtype=np.uint16), datashape)
            numDiffs = np.count_nonzero(cpp_signal - python_signal)
            if numDiffs != 0 :
                print(f'Failure. {numDiffs} diffs comparing cpp and python signal for FOV {fov_idx} and frame {frame_idx}.')
                success = False


            cpp_snr = np.reshape(np.fromfile(cpp_snr_fpath, dtype=np.uint16), datashape)
            python_snr = np.reshape(np.fromfile(python_snr_fpath, dtype=np.uint16), datashape)
            numDiffs = np.count_nonzero(cpp_snr - python_snr)
            if numDiffs != 0:
                print(f'Failure. {numDiffs} diffs comparing cpp and python snr for FOV {fov_idx} and frame {frame_idx}.')
                success = False
            

            cpp_bkg = np.reshape(np.fromfile(cpp_bkg_fpath, dtype=np.uint16), datashape)
            python_bkg = np.reshape(np.fromfile(python_bkg_fpath, dtype=np.uint16), datashape)
            numDiffs = np.count_nonzero(cpp_bkg-python_bkg)
            if numDiffs != 0:
                print(f'Failure. {numDiffs} diffs comparing cpp and python background for FOV {fov_idx} and frame {frame_idx}.')
                success = False


            if not omit_end_to_end :
                net_bkg_fpath = f'../../tmp/net_bkg_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx}.bin'
                net_bkg = np.flipud(np.reshape(np.fromfile(net_bkg_fpath, dtype=np.uint16), datashape))
                cpp_bkg[cpp_range==0] = 0
                numDiffs = np.count_nonzero(net_bkg-cpp_bkg)
                if numDiffs != 0:
                    print(f'Failure. {numDiffs} diffs comparing net and cpp background for FOV {fov_idx}.')
                    success = False

                net_snr_fpath = f'../../tmp/net_snr_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx}.bin'
                net_snr = np.flipud(np.reshape(np.fromfile(net_snr_fpath, dtype=np.uint16), datashape))
                cpp_snr[cpp_range==0] = 0
                numDiffs = np.count_nonzero(net_snr-cpp_snr)
                if numDiffs != 0:
                    print(f'Failure. {numDiffs} diffs comparing net and cpp snr for FOV {fov_idx}.')
                    success = False
                
                net_signal_fpath = f'../../tmp/net_signal_float_as_short_' + tag + f'_fov{fov_idx}_frame{frame_idx}.bin'
                net_signal = np.flipud(np.reshape(np.fromfile(net_signal_fpath, dtype=np.uint16), datashape))
                cpp_signal[cpp_range==0] = 0
                numDiffs = np.count_nonzero(net_signal-cpp_signal)
                if numDiffs != 0:
                    print(f'Failure. {numDiffs} diffs comparing net and cpp signal for FOV {fov_idx}.')
                    success = False

                net_range_fpath = f'../../tmp/net_range_float_as_float_' + tag + f'_fov{fov_idx}_frame{frame_idx}.bin'
                net_range = np.flipud(np.reshape(np.fromfile(net_range_fpath, dtype=np.float32)*1024.0, datashape).astype(np.uint16))
                numDiffs = np.count_nonzero(cpp_range-net_range)
                if numDiffs != 0:
                    print(f'Failure. {numDiffs} diffs comparing cpp and net range for FOV {fov_idx}.')
                    success = False

    print(f'Test {tag} success: {success}')
    if (not success) :
        sys.exit(-1)


'''
brief: Executes the test_lumotimers test and verifies successful execution.
'''
def test_lumotimers() :
    print(f'Running test_lumotimers')
    res = subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.test_lumotimers'], capture_output=True, text=True)
    if 0 != res.returncode :
        print(f'test_lumotimers test failed with return code = {res.returncode}')
        exit(-1)
    print(f'test_lumotimers test succeeded with return code = {res.returncode}')
    


'''
brief: Executes two tests in the raw-to-depth-tests.cpp file that verify the functionality of the scan_table and random_fov
tags. 

Checks the stderr to verify that the proper LLogErr() methods have been executed.
'''
def random_tags_tests() :
    print(f'Running random_tags_tests')
    res = subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.changing_scan_table_tag'], capture_output=True, text=True)
    if ('Skipping ROI. Scan table tag changed in the middle of an FOV.' not in res.stderr  or 
        'Skipping whole-frame processing. Incomplete FOV received.' not in res.stderr ):
        print(f'Test Failure in changing_scan_table_tag stderr:{res.stderr}')
        exit(-1)

    res = subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.changing_fov_tag'], capture_output=True, text=True)
    if ('Skipping ROI. FOV tag changed in the middle of an FOV.' not in res.stderr or 
        'Skipping whole-frame processing. Incomplete FOV received.' not in res.stderr) :
        print(f'Test Failure in changing_fov_tag. stderr: {res.stderr}')
        exit(-1)
    print(f'random_tags_tests passed')


def test_mapping_table() :
    print(f'Running the test_bin_mapping_table.')
    subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.test_bin_mapping_table'])
    original_mapping_table = np.fromfile("../unittest-artifacts/mapping_table/mapping_table_A.bin", dtype=np.int32)
    rewritten_mapping_table = np.fromfile("../../tmp/mapping_table_frombin_Out.bin", dtype=np.int32)
    numDiffs = np.count_nonzero(original_mapping_table - rewritten_mapping_table)
    if numDiffs != 0 :
        print(f'Failure in mapping_table_test.')
        sys.exit(-1)
    print(f'test_bin_mapping_table passed.')


    print(f'Running the test_csv_mapping_table.')
    subprocess.run(['raw-to-depth-cpp-tests/raw-to-depth-tests', '--gtest_filter=RawToDepthTests.test_csv_mapping_table'])
    rewritten_mapping_table = np.fromfile("../../tmp/mapping_table_fromcsv_Out.bin", dtype=np.int32)

    print(f'reading rewritten mapping table with {rewritten_mapping_table.size} ints')
    original_mapping_table = np.zeros(rewritten_mapping_table.shape)
    
    idx = 0
    with open('../unittest-artifacts/mapping_table/supersampled_mapping_table.csv') as inf :

        line = 'dummy'
        while line != "" :
            line = inf.readline()
            elements = line.strip().split(',')
            if len(elements) != 4:
                break

            for inner_idx in range(4) :
                orig = int(elements[inner_idx])
                rewritten = rewritten_mapping_table[idx]
                if orig != rewritten :
                    print(f'Failure in mapping_table_test with mismatch at idx {idx}. original {orig} rewritten {rewritten}')
                    sys.exit(-1)
                idx += 1

    print(f'test_csv_mapping_table passed.')



if __name__ == '__main__' :
    """
    Runs tests to compare the output from the RawToDepth unit tests against the end-to-end execution
    from frontend through the Lumotive Stream Receiver.

    Executes the tests in the directories indicated in test_names.
    """

    # First check if this is an NCB
    if system('uname -a | grep imx8qmmek') == 0:
        system('systemctl stop frontend')
        time.sleep(3)

    parser = argparse.ArgumentParser(
        prog=Path(__file__).name,
        description='Runs comparisons between the c++ and python implementations of the RawToDepth algorithms and compares the outputs.'
    )
    parser.add_argument('-n', '--no-end-to-end', action='store_true', default=False, help='Bypasses the test that uses the mock frontend to pass results to the Lumotive Stream Receiver cpp_client')
    args = parser.parse_args()
    omit_end_to_end = args.no_end_to_end
    print(f'This test {("does not" if omit_end_to_end else "does")} include the mocked frontend through the network end-to-end tests.')

    test_names = [
                  'lidar_mode_plus_hdr'
                , 'snth_stripe_various_windows-f98-91_8linerois-bin222'
                , 'grid-mode-compare-office'
                , 'stripe-mode-compare-office'
                , 'stripe-mode'
                , 'grid-mode'
                , 'snth_stripe_simple-f98-68_8linerois-bin2'
                , 'snth_stripe_various_binning-f98-68_8linerois-bin124'
                , 'snth_nonzero_roi_start-f87-75_6linerois-bin124'
                , 'snth_various_binning-f87-91_6linerois-bin124'
                , 'snth_gaps-f98-68_8linerois-bin2'
                , 'snth_simple-f98-88_6linerois-bin2'
                , 'snth_simple-f98-91_8linerois-bin2'
                , 'snth_various_ghost-f87-89_6linerois-bin2222'
                ,  'snth_tiny-f87-1_6linerois-bin2'
                , 'snth_various_neighbor-f87-75_6linerois-bin2222'
                , 'snth_one_480-f87-1_480linerois-bin124'
                ]
    file_prefixes = [
        'lidar_mode_plus_hdr_0_01_'
        , 'snth_'
        , 'grid-mode-compare-office_0_01_'
        , 'stripe-mode-compare-office_0_01_'
        , 'stripe-mode_0_01_'
        , 'grid-mode_0_01_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
        , 'snth_'
    ]

    test_nums_fovs = [ 
         1
        ,3
        ,1
        ,1
        ,1
        ,1
        ,1
        ,3
        ,3
        ,3
        ,1
        ,1
        ,1
        ,4
        ,1
        ,4
        ,3
        ]
    
    test_nums_frames = [
        99
        ,91
        ,2
        ,87
        ,91
        ,1
        ,68
        ,68
        ,1
        ,1
        ,1
        ,1
        ,1
        ,1
        ,1
        ,1
        ,1
    ]

    for (test_name, file_prefix, num_fovs, num_frames) in zip(test_names, file_prefixes, test_nums_fovs, test_nums_frames) :
        # bug in frontend that won't allow for a single ROI in a directory. Q: why does it work for 'tiny?'
        run_test(test_name, file_prefix, num_fovs, num_frames, (True if 'one_480' in test_name else omit_end_to_end))

    random_tags_tests()
    test_mapping_table()
    test_lumotimers()

    print(f'Ran {len(test_names)+2} tests. Success')

    
    if system('uname -a | grep imx8qmmek') == 0:
        system('systemctl start frontend')
        time.sleep(3)

