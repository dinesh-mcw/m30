import numpy as np
import os
import time
import matplotlib.pyplot as plt

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
num_frames = 5              # The device processes 1 frame at a time

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
path = os.path.join('..', '..', '..', 'chronoptics_data', 'scene_h', 'raw_roi_bin', 'eol36amb')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'scene_h_0_13_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs_stripe_mode = {'perform_tap_add': perform_tap_add,
                       'correct_strips': False,
                       'weighted_sum_en': True,
                       'weight_by': 'SNR',
                       'weight_by_illumination_num_points': 20,
                       'weight_by_illumination_mean': 10,
                       'weight_by_illumination_std': 1.6,
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
                       'range_median_filter_en': True,
                       'range_median_filter_size': [1, 5],
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

configs_stripe_mode_1x2 = configs_stripe_mode.copy()
configs_stripe_mode_1x2['binning'] = (1, 2)
configs_stripe_mode_1x2['convolution_kernel_x_size'] = 5
configs_stripe_mode_1x4 = configs_stripe_mode.copy()
configs_stripe_mode_1x4['binning'] = (1, 4)
configs_stripe_mode_1x4['convolution_kernel_x_size'] = 3

configs_normal_mode = configs_stripe_mode.copy()
configs_normal_mode['binning'] = (1, 1)
configs_normal_mode['convolution_kernel_x_size'] = 7
configs_normal_mode['convolution_kernel_y_size'] = 15
configs_normal_mode['M_filter_type'] = 8
configs_normal_mode['M_filter_size'] = (3, 3)
configs_normal_mode['range_median_filter_size'] = [5, 5]
configs_normal_mode_2x2 = configs_normal_mode.copy()
configs_normal_mode_2x2['binning'] = (2, 2)
configs_normal_mode_2x2['convolution_kernel_x_size'] = 5
configs_normal_mode_2x2['convolution_kernel_y_size'] = 7
configs_normal_mode_4x4 = configs_normal_mode.copy()
configs_normal_mode_4x4['binning'] = (4, 4)
configs_normal_mode_4x4['convolution_kernel_x_size'] = 3
configs_normal_mode_4x4['convolution_kernel_y_size'] = 5

max_range = 10.0
for dataset_idx in range(len(data_gen.data[input_data_name])):

    print('Processing idx: ' + str(dataset_idx))
    input_data = data_gen.data[input_data_name][dataset_idx]

    # Stripe mode processing
    # output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode,
    #                                                      start_vector=start_vector[dataset_idx])
    # stripe_mode_range.append(device.dsp.data[output_range_array_name].flt)
    # device.clear(force_clear=True)

    output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_1x2,
                                                         start_vector=start_vector[dataset_idx])
    stripe_mode_range_1x2 = device.dsp.data[output_range_array_name].flt
    device.clear(force_clear=True)

    # output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_1x4,
    #                                                      start_vector=start_vector[dataset_idx])
    # stripe_mode_range_1x4.append(device.dsp.data[output_range_array_name].flt)
    # device.clear(force_clear=True)

    # Normal mode processing
    # output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[dataset_idx], configs=configs_normal_mode)
    # normal_mode_range.append(device.dsp.data[output_range_array_name].flt)
    # device.clear(force_clear=True)

    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[dataset_idx], configs=configs_normal_mode_2x2)
    normal_mode_range_2x2 = device.dsp.data[output_range_array_name].flt
    device.clear(force_clear=True)

    # output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[dataset_idx], configs=configs_normal_mode_4x4)
    # normal_mode_range_4x4.append(device.dsp.data[output_range_array_name].flt)
    # device.clear(force_clear=True)

    dump_path = os.path.join('..', 'output')
    fig = plt.figure(figsize=(20, 10))
    plt.subplot(1, 2, 1)
    plt.imshow(stripe_mode_range_1x2, cmap='jet', vmin=0, vmax=max_range)
    plt.title('Stripe mode QVGA')
    plt.subplot(1, 2, 2)
    plt.imshow(np.flipud(normal_mode_range_2x2), cmap='jet', vmin=0, vmax=max_range)
    plt.title('Normal mode QVGA')
    filename = 'stripe_mode_vs_normal_mode_' + time.strftime("%Y%m%d-%H%M%S") + '.png'
    plt.savefig(os.path.join(dump_path, filename))
    plt.close(fig)

