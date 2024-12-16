import numpy as np
import os

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

"""
    Time domain boxcar testbench
"""

##############################################
# 1: Import input data
# General params
num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640    # Number of columns per ROI at the input (before any binning)
num_rois = 91                # Number of ROIs per frame
num_rows_full_frame = 460
num_frames = 5               # The device processes 1 frame at a time

data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                           num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'targets_merging', 'scene_data')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'hdr_scene_thresh2000_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

if data_gen.num_loaded_frames <= 1:
    raise Exception('There needs to be more than 1 loaded frame to use this testbench.')

###############################################################################################
# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'correct_strips': False,
           'binning': (2, 2),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'HDR_mode_en': True,
           'HDR_override_threshold': 6140,
           'HDR_blooming_corr_en': True,
           'HDR_blooming_alg_select': 0,
           'HDR_blooming_id_threshold': 500, # To identify bloomed pixels based on intensity
           'HDR_blooming_id_hdr_thres': 500, # To identify pixels belong to retroreflectors based on intensity
           'HDR_min_num_points_in_connected_region': 100,   # Alg 0 only
           'HDR_pixel_radius': 25,                          # Alg 1 only
           'temporal_boxcar_length': 2,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 5,
           'convolution_kernel_y_size': 7,
           'M_filter': True,
           'M_filter_loc': 0,
           'M_filter_type': 8,
           'M_filter_size': (3, 3),
           'M_filter_shape': None,
           'range_edge_filter_en': False,
           'range_median_filter_en': True,
           'range_median_filter_size': [5, 5],
           'range_median_filter_shape': '+',
           'NN_enable': False,
           'NN_filter_level': 5,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0.1,
           'pixel_mask_path': os.path.join('..', '..', '..', 'pixel_mask_SN88.bin'),
           'invalid_pixel_mask_enable': True}

for frame in range(data_gen.num_loaded_frames):
    # 3: Process through device
    start_vector = data_gen.data["ROI_start_vector"][frame]
    input_data = data_gen.data[input_data_name][frame]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs,
                                     HDR=(data_gen.data['hdr_thresholds'][frame], data_gen.data['hdr_retries'][frame]))

    # 4: Analyze (plot and vector match)
    data_viewer.assign_dict(device.dsp.data)
    data_viewer.plot_snr_simple(save_figure=True, flip_ud=True)
    data_viewer.plot_range_simple(range_array_name=output_range_array_name, save_figure=True, flip_ud=True)
    data_viewer.dump_frames_to_binary_files()
    data_viewer.dump_range_arrays()

    device.clear(force_clear=False)