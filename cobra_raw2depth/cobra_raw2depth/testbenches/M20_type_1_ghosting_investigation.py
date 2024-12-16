import numpy as np
import os
import copy

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
num_rows_full_frame = 450
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
path = os.path.join('..', '..', '..', 'ghosting_raw_data-20211222T184525Z-001', 'ghosting_raw_data')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ghost_0_05_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]
output = []

# 2: Investigating best options for filtering ghosts
# Option 1: Remove convolution, use various NN filter levels to try and remove ghosts (fixed SNR threshold)
for NN_filter_level in [4, 5, 6, 7]:
    configs_opt1 = {'perform_tap_add': perform_tap_add,
                    'binning': (2, 2),
                    'SNR_voting_combined': True,
                    'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                    'enable_convolution': False,
                    'use_1d_convolutions': False,
                    'convolution_kernel_x_size': 5,
                    'convolution_kernel_y_size': 7,
                    'M_filter': False,
                    'M_filter_type': 0,
                    'M_median_filter_size': 5,
                    'NN_enable': True,
                    'NN_filter_level': NN_filter_level,
                    'NN_min_neighbors': 5,
                    'NN_patch_size': 5,
                    'NN_range_tolerance': 0.1,
                    'SNR_threshold_enable': True,
                    'SNR_threshold': 0.01}

    input_data = data_gen.data[input_data_name]
    device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt1)
    output.append(copy.deepcopy(device.dsp.data))
    device.clear(force_clear=True)

# Option 2: No convolution, median filter 2 sizes (fixed SNR threshold)
for median_size in [5, 7]:
    configs_opt2 = {'perform_tap_add': perform_tap_add,
                    'binning': (2, 2),
                    'SNR_voting_combined': True,
                    'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                    'enable_convolution': False,
                    'use_1d_convolutions': False,
                    'convolution_kernel_x_size': 5,
                    'convolution_kernel_y_size': 7,
                    'M_filter': True,
                    'M_filter_type': 1,
                    'M_median_filter_size': median_size,
                    'NN_enable': False,
                    'NN_filter_level': 0,
                    'NN_min_neighbors': 5,
                    'NN_patch_size': 5,
                    'NN_range_tolerance': 0.1,
                    'SNR_threshold_enable': True,
                    'SNR_threshold': 0.01}

    input_data = data_gen.data[input_data_name]
    device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt2)
    output.append(copy.deepcopy(device.dsp.data))
    device.clear(force_clear=True)

# Option 3: Convolution enabled, use various NN filter levels to try and remove ghosts (fixed SNR threshold)
for NN_filter_level in [1, 2, 3, 4, 5, 6, 7]:
    configs_opt3 = {'perform_tap_add': perform_tap_add,
                    'binning': (2, 2),
                    'SNR_voting_combined': True,
                    'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                    'enable_convolution': True,
                    'use_1d_convolutions': False,
                    'convolution_kernel_x_size': 5,
                    'convolution_kernel_y_size': 7,
                    'M_filter': False,
                    'M_filter_type': 1,
                    'M_median_filter_size': 5,
                    'NN_enable': True,
                    'NN_filter_level': NN_filter_level,
                    'NN_min_neighbors': 5,
                    'NN_patch_size': 5,
                    'NN_range_tolerance': 0.1,
                    'SNR_threshold_enable': True,
                    'SNR_threshold': 0.01}

    input_data = data_gen.data[input_data_name]
    device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt3)
    output.append(copy.deepcopy(device.dsp.data))
    device.clear(force_clear=True)

# Option 4: Convolution enabled, median filter 5x5 (fixed SNR threshold)
configs_opt4 = {'perform_tap_add': perform_tap_add,
                'binning': (2, 2),
                'SNR_voting_combined': True,
                'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                'enable_convolution': True,
                'use_1d_convolutions': False,
                'convolution_kernel_x_size': 5,
                'convolution_kernel_y_size': 7,
                'M_filter': True,
                'M_filter_type': 1,
                'M_median_filter_size': 5,
                'NN_enable': False,
                'NN_filter_level': 0,
                'NN_min_neighbors': 5,
                'NN_patch_size': 5,
                'NN_range_tolerance': 0.1,
                'SNR_threshold_enable': True,
                'SNR_threshold': 0.01}

input_data = data_gen.data[input_data_name]
device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt4)
output.append(copy.deepcopy(device.dsp.data))
device.clear(force_clear=True)

# Option 5: Convolution enabled, median filter 5x5 plus shaped (fixed SNR threshold)
configs_opt5 = {'perform_tap_add': perform_tap_add,
                'binning': (2, 2),
                'SNR_voting_combined': True,
                'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                'enable_convolution': True,
                'use_1d_convolutions': False,
                'convolution_kernel_x_size': 5,
                'convolution_kernel_y_size': 7,
                'M_filter': True,
                'M_filter_type': 1,
                'M_median_filter_size': 5,
                'M_median_filter_shape': '+',
                'NN_enable': False,
                'NN_filter_level': 0,
                'NN_min_neighbors': 5,
                'NN_patch_size': 5,
                'NN_range_tolerance': 0.1,
                'SNR_threshold_enable': True,
                'SNR_threshold': 0.01}

input_data = data_gen.data[input_data_name]
device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt5)
output.append(copy.deepcopy(device.dsp.data))
device.clear(force_clear=True)

# Option 6: Convolution enabled, median filter 5x5 cross shaped (fixed SNR threshold)
configs_opt6 = {'perform_tap_add': perform_tap_add,
                'binning': (2, 2),
                'SNR_voting_combined': True,
                'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                'enable_convolution': True,
                'use_1d_convolutions': False,
                'convolution_kernel_x_size': 5,
                'convolution_kernel_y_size': 7,
                'M_filter': True,
                'M_filter_type': 1,
                'M_median_filter_size': 5,
                'M_median_filter_shape': 'x',
                'NN_enable': False,
                'NN_filter_level': 0,
                'NN_min_neighbors': 5,
                'NN_patch_size': 5,
                'NN_range_tolerance': 0.1,
                'SNR_threshold_enable': True,
                'SNR_threshold': 0.01}

input_data = data_gen.data[input_data_name]
device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt6)
output.append(copy.deepcopy(device.dsp.data))
device.clear(force_clear=True)

# Option 7: Convolution enabled, pixel mask using median filter 5x5 cross shaped (fixed SNR threshold)
configs_opt7 = {'perform_tap_add': perform_tap_add,
                'binning': (2, 2),
                'SNR_voting_combined': True,
                'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                'enable_convolution': True,
                'use_1d_convolutions': False,
                'convolution_kernel_x_size': 5,
                'convolution_kernel_y_size': 7,
                'M_filter': True,
                'M_filter_type': 0,
                'M_median_filter_size': 5,
                'M_median_filter_shape': 'x',
                'NN_enable': False,
                'NN_filter_level': 0,
                'NN_min_neighbors': 5,
                'NN_patch_size': 5,
                'NN_range_tolerance': 0.1,
                'SNR_threshold_enable': True,
                'SNR_threshold': 0.01}

input_data = data_gen.data[input_data_name]
device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt7)
output.append(copy.deepcopy(device.dsp.data))
device.clear(force_clear=True)

# ption 8: Convolution enabled, Sobel filter to detect edges and subtract them (fixed SNR threshold)
configs_opt8 = {'perform_tap_add': perform_tap_add,
                'binning': (2, 2),
                'SNR_voting_combined': True,
                'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                'enable_convolution': True,
                'use_1d_convolutions': False,
                'convolution_kernel_x_size': 5,
                'convolution_kernel_y_size': 7,
                'M_filter': False,
                'M_filter_type': 0,
                'M_median_filter_size': 5,
                'M_median_filter_shape': 'x',
                'range_edge_filter_en': True,
                'NN_enable': False,
                'NN_filter_level': 0,
                'NN_min_neighbors': 5,
                'NN_patch_size': 5,
                'NN_range_tolerance': 0.1,
                'SNR_threshold_enable': True,
                'SNR_threshold': 0.01}

input_data = data_gen.data[input_data_name]
device(input_data=input_data, roi_start_vector=start_vector, configs=configs_opt8)
output.append(copy.deepcopy(device.dsp.data))
device.clear(force_clear=True)

# 3: Data dump
options = ['opt1_no_conv_NN_1',
           'opt1_no_conv_NN_2',
           'opt1_no_conv_NN_3',
           'opt1_no_conv_NN_4',
           'opt1_no_conv_NN_5',
           'opt1_no_conv_NN_6',
           'opt1_no_conv_NN_7',
           'opt2_no_conv_M_5',
           'opt2_no_conv_M_7',
           'opt3_with_conv_NN_1',
           'opt3_with_conv_NN_2',
           'opt3_with_conv_NN_3',
           'opt3_with_conv_NN_4',
           'opt3_with_conv_NN_5',
           'opt3_with_conv_NN_6',
           'opt3_with_conv_NN_7',
           'opt4_with_conv_M_5',
           'opt5_with_conv_M_5_+_shaped',
           'opt6_with_conv_M_5_x_shaped',
           'opt7_with_conv_pixel_mask',
           'opt8_with_conv_edge_filter']

if len(options) != len(output):
    raise Exception("Warning, number of outputs and number of options to print not the same.")

for name in options:
    data_viewer.assign_dict(output.pop(0))
    # data_viewer.plot_M(flip_ud=True)
    # data_viewer.plot_snr_simple(save_figure=True, flip_ud=True, figsize=(12, 9))
    data_viewer.plot_range_simple(range_array_name='ranges_SNR_filtered', save_figure=True, filename=name,
                                  flip_ud=True, figsize=(12, 9), show_figure=False)
    # data_viewer.plot_3d_range(os.path.join('..', '..', '..', 'm20_mapping_table.csv'),
    #                          range_array_name='ranges')

