import numpy as np
import os

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

"""
    Simple testbench to use M20-GPixel with generated/imported data and visualize the depth map at the output
"""

##############################################
# 1: Import input data
# General params
num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 91                # Number of ROIs per frame
num_rows_full_frame = 460
num_frames = 1              # The device processes 1 frame at a time

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
file_name_base = 'hdr_scene_thresh20_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs_stripe_mode = {'perform_tap_add': perform_tap_add,
                       'correct_strips': False,
                       'HDR_mode_en': True,
                       'HDR_override_threshold': 6140,
                       'HDR_blooming_corr_en': False,
                       'HDR_blooming_alg_select': 0,
                       'HDR_blooming_id_threshold': 250,  # To identify bloomed pixels based on intensity
                       'HDR_blooming_id_hdr_thres': 500,
                       # To identify pixels belong to retroreflectors based on intensity
                       'HDR_min_num_points_in_connected_region': 100,  # Alg 0 only
                       'HDR_pixel_radius': 25,  # Alg 1 only
                       'weighted_sum_en': True,
                       'weight_by': 'peak',
                       'weight_by_SNR_max_points': 0,
                       'weight_by_SNR_auto_mode': 'percent',
                       'weight_by_matched_filter_num_points': 20,
                       'weight_by_matched_filter_mean': 10,
                       'weight_by_matched_filter_std': 1.6,
                       'weight_by_peak_num_points': 3,
                       'weight_by_peak_type': 'intensity',
                       'binning': (1, 1),
                       'SNR_voting_combined': True,
                       'SNR_voting_thres_enable': False,
                       'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                       'enable_convolution': True,
                       'enable_phase_correction': True,
                       'use_1d_convolutions': False,
                       'convolution_kernel_x_size': 7,
                       'convolution_kernel_y_size': 1,
                       'M_filter': True,
                       'M_filter_loc': 0,
                       'M_filter_type': 3,
                       'M_filter_size': (1, 3),
                       'M_filter_shape': None,
                       'range_edge_filter_en': False,
                       'range_median_filter_en': False,
                       'range_median_filter_size': [1, 5],
                       'range_median_filter_shape': None,
                       'NN_enable': True,
                       'NN_filter_level': 0,
                       'NN_min_neighbors': 4,
                       'NN_patch_size': [1, 5],
                       'NN_range_tolerance': 0.7,
                       'SNR_threshold_enable': False,
                       'SNR_threshold': 1.5,
                       'pixel_mask_path': os.path.join('..', '..', '..', 'pixel_mask_SN88.bin'),
                       'invalid_pixel_mask_enable': True}

input_data = data_gen.data[input_data_name]
output_range_array_name = device.stripe_mode_process(input_data=input_data,
                                                     start_vector=start_vector,
                                                     configs=configs_stripe_mode,
                                                     HDR=(data_gen.data['hdr_thresholds'],
                                                          data_gen.data['hdr_retries']))

# 3: Data analysis
mapping_table_path = os.path.join('..', '..', '..', 'mapping_table_SN88.csv')
data_viewer.assign_dict(device.dsp.data)
# data_viewer.plot_snr_simple(save_figure=True, flip_ud=True)
data_viewer.plot_range_simple(range_array_name=output_range_array_name, save_figure=True, flip_ud=True)
data_viewer.plot_3d_range(mapping_table_path,
                          configs_stripe_mode['binning'],
                          device.frame_start_row,
                          device.frame_stop_row,
                          start_vector=start_vector,
                          stripe_mode_en=configs_stripe_mode['weighted_sum_en'],
                          max_rgb_range=10.0,
                          range_multiplicator=1.0,  # set range to mm for comparison with chronoptics pt
                          range_array_name=output_range_array_name,
                          show_pointcloud=True,
                          save_pointcloud=False)
data_viewer.dump_frames_to_binary_files()
data_viewer.dump_range_arrays()
