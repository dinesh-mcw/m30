"""
file: m30_execute_python_verification.py

This file is a wrapper that calls the dsp algorithms given a certain test data directory.
This file can be used for testing, and is used during development, however, the unit tests for 
the python code are executed by the python program at

../raw-to-depth-cpp-tests/verifyAgainstLsrClient.py

Notes:
dsp_config:
  'stripe_window': 'rect', 'Gaussian', 'snr-weighted',  only 'Gaussian' has been implemented
  'set_binning': int
  'enabled_stripe_mode' : boolean
  'set_snr_thresh: a MAX_ACTIVE_FOVS-long list of snr thresholds, one for each FOV.

Copyright 2023 (C) Lumotive, Inc. All rights reserved.
"""


import m30_verification as v
import M30Metadata as md
from m30_dsp import gauss
from m30_dsp import smooth_raw, kernel_5, kernel_7, cppround, cppround_scalar
import os
import numpy as np

def run_test(dsp_config) :
  v.process_rois(dsp_config)

if __name__ == "__main__" :

  input_dir = os.path.join('..', 'unittest-artifacts', 'lidar_mode_plus_hdr')

  dsp_config = {
    'output_intermediate_results' : False,
    'input_dir': input_dir,
    'tag' : os.path.basename(input_dir),
    'output_dir': os.path.join('..', '..', 'tmp')
    # , 'set_binning' : 2
    # , 'set_snr_thresh' : [0]*md.MAX_ACTIVE_FOVS #one per fov.
    # , 'stripe_window': { 'window' : 'Gaussian', 'std' : 2 }
    # , 'enable_stripe_median' : True
    # , 'process_frame_indices' : [17]
    # , 'process_roi_indices' : [17]
    }

  run_test(dsp_config)


  # Standalone smoothing tests, used for manual testing during development.

  #Test smoothing
  # raw_image_240x320 = np.random.randint(0, 99, 3*240*320).astype(np.float32)
  # smoothed5x7_raw_image_240x320 = np.zeros(raw_image_240x320.size, dtype=np.float32)
  # smooth_raw(raw_image_240x320, smoothed5x7_raw_image_240x320, [240,320], [7,5])

  # raw_image_240x320.tofile("../../tmp/python_smooth_raw_image_240x320_input.bin")
  # smoothed5x7_raw_image_240x320.tofile("../../tmp/python_smooth5x7_raw_image_240x320_output.bin")

  # smoothed3x5_raw_image_240x320 = np.zeros(raw_image_240x320.size, dtype=np.float32)
  # smooth_raw(raw_image_240x320, smoothed3x5_raw_image_240x320, [240,320], [5,3])
  # smoothed3x5_raw_image_240x320.tofile("../../tmp/python_smooth3x5_raw_image_240x320_output.bin");  

