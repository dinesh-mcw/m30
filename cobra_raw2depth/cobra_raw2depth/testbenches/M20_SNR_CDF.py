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
path = os.path.join('..', '..', '..', 'strips_investiguation', 'wall_tests_20220228')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'targets_ci22_angular10_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'correct_strips': False,
           'binning': (1, 1),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 7,
           'convolution_kernel_y_size': 15,
           'M_filter': False,
           'M_filter_loc': 1,
           'M_filter_type': 2,
           'M_median_filter_size': 3,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 0,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': True,
           'SNR_threshold': 2,
           'SNR_threshold_invalid_val': np.nan}

input_data = data_gen.data[input_data_name]
SNR_thresholds = np.concatenate([np.arange(0, 10, 0.1), np.arange(10, 50, 1), np.arange(50, 400, 20)])
num_points_per_SNR_1x1 = []
for SNR_thres in SNR_thresholds:
    configs['SNR_threshold'] = SNR_thres
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    num_points_per_SNR_1x1.append(np.count_nonzero(~np.isnan(np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0))))
    SNRs = np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0)
    print('SNR min: ' + str(np.nanmin(SNRs.flatten())) + ', SNR max: ' + str(np.nanmax(SNRs.flatten())) +
          ' (SNR thres: ' + str(SNR_thres) + ')')
    device.clear(force_clear=True)

num_points_per_SNR_2x2 = []
configs['binning'] = (2, 2)
for SNR_thres in SNR_thresholds:
    configs['SNR_threshold'] = SNR_thres
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    num_points_per_SNR_2x2.append(np.count_nonzero(~np.isnan(np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0))))
    SNRs = np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0)
    print('SNR min: ' + str(np.nanmin(SNRs.flatten())) + ', SNR max: ' + str(np.nanmax(SNRs.flatten())) +
          ' (SNR thres: ' + str(SNR_thres) + ')')
    device.clear(force_clear=True)

num_points_per_SNR_4x4 = []
configs['binning'] = (4, 4)
for SNR_thres in SNR_thresholds:
    configs['SNR_threshold'] = SNR_thres
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    num_points_per_SNR_4x4.append(np.count_nonzero(~np.isnan(np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0))))
    SNRs = np.nanmean(device.dsp.data['SNR_frame'].flt, axis=0)
    print('SNR min: ' + str(np.nanmin(SNRs.flatten())) + ', SNR max: ' + str(np.nanmax(SNRs.flatten())) +
          ' (SNR thres: ' + str(SNR_thres) + ')')
    device.clear(force_clear=True)

num_points_per_SNR_1x1 = np.asarray(num_points_per_SNR_1x1)
num_points_per_SNR_2x2 = np.asarray(num_points_per_SNR_2x2)
num_points_per_SNR_4x4 = np.asarray(num_points_per_SNR_4x4)

import matplotlib.pyplot as plt
plt.plot(SNR_thresholds, 100*num_points_per_SNR_1x1 / num_points_per_SNR_1x1[0], label='1x1 binning')
plt.plot(SNR_thresholds, 100*num_points_per_SNR_2x2 / num_points_per_SNR_2x2[0], label='2x2 binning')
plt.plot(SNR_thresholds, 100*num_points_per_SNR_4x4 / num_points_per_SNR_4x4[0], label='4x4 binning')
plt.xlabel("SNR threshold value")
plt.ylabel("Number of points left [%]")
plt.ylim([0, 101])
plt.title("Number of points remaining after SNR threshold")
plt.legend()
plt.show()


