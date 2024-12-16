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
num_rows_per_roi = 480       # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 1               # Number of ROIs per frame
num_rows_full_frame = 480
num_frames = 1              # The device processes 1 frame at a time

data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois)
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                           num_rois, num_rows_full_frame)
data_viewer_bin = M20_iTOF_viewer()
data_viewer_conv = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'cobra_raw2depth_data', 'synth_noisy_blk-f87-1rois-bin0')
file_name_base = 'synth_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_tap_acc_roi_data(path, file_name_base, num_frames, np.uint16)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

binnings_values = [(1, 1), (2, 2), (4, 4)]
bin_data = []
conv_stepping_data = []
conv_nn_interp_data = []
conv_bilinear_interp_data = []
conv_cubic_interp_data = []
conv_averaging_data = []

for bin in binnings_values:
    # 2: Process
    binning = bin
    configs_stepping = {'perform_tap_add': perform_tap_add,
                        'binning': binning,
                        'SNR_voting_combined': True,
                        'use_1d_convolutions': True,
                        'ROI_convolution_kernel_x_size': binning[0],     # boxcar filter to mimic binning
                        'ROI_convolution_kernel_y_size': binning[1],
                        'convolution_kernel_x_size': 7,                  # Full frame convolution (gaussian kernel)
                        'convolution_kernel_y_size': 14,
                        'downsample_interp_type': "point-sampling",            # point-sampling, NN-interp, binning
                        'NN_enable': False,
                        'NN_min_neighbors': 6,
                        'NN_patch_size': 5,
                        'NN_range_tolerance': 0.1,
                        'SNR_threshold_enable': False,
                        'SNR_threshold': 0.1}

    configs_nn_interp = {'perform_tap_add': perform_tap_add,
                        'binning': binning,
                        'SNR_voting_combined': True,
                        'use_1d_convolutions': True,
                        'ROI_convolution_kernel_x_size': binning[0],     # boxcar filter to mimic binning
                        'ROI_convolution_kernel_y_size': binning[1],
                        'convolution_kernel_x_size': 7,                  # Full frame convolution (gaussian kernel)
                        'convolution_kernel_y_size': 14,
                        'downsample_interp_type': "NN-interp",            # point-sampling, NN-interp, binning
                        'NN_enable': False,
                        'NN_min_neighbors': 6,
                        'NN_patch_size': 5,
                        'NN_range_tolerance': 0.1,
                        'SNR_threshold_enable': False,
                        'SNR_threshold': 0.1}

    configs_bilinear_interp = {'perform_tap_add': perform_tap_add,
                        'binning': binning,
                        'SNR_voting_combined': True,
                        'use_1d_convolutions': True,
                        'ROI_convolution_kernel_x_size': binning[0],     # boxcar filter to mimic binning
                        'ROI_convolution_kernel_y_size': binning[1],
                        'convolution_kernel_x_size': 7,                  # Full frame convolution (gaussian kernel)
                        'convolution_kernel_y_size': 14,
                        'downsample_interp_type': "bilinear-interp",            # point-sampling, NN-interp, binning
                        'NN_enable': False,
                        'NN_min_neighbors': 6,
                        'NN_patch_size': 5,
                        'NN_range_tolerance': 0.1,
                        'SNR_threshold_enable': False,
                        'SNR_threshold': 0.1}

    configs_cubic_interp = {'perform_tap_add': perform_tap_add,
                        'binning': binning,
                        'SNR_voting_combined': True,
                        'use_1d_convolutions': True,
                        'ROI_convolution_kernel_x_size': binning[0],     # boxcar filter to mimic binning
                        'ROI_convolution_kernel_y_size': binning[1],
                        'convolution_kernel_x_size': 7,                  # Full frame convolution (gaussian kernel)
                        'convolution_kernel_y_size': 14,
                        'downsample_interp_type': "bicubic-interp",            # point-sampling, NN-interp, binning
                        'NN_enable': False,
                        'NN_min_neighbors': 6,
                        'NN_patch_size': 5,
                        'NN_range_tolerance': 0.1,
                        'SNR_threshold_enable': False,
                        'SNR_threshold': 0.1}

    configs_averaging = {'perform_tap_add': perform_tap_add,
                        'binning': binning,
                        'SNR_voting_combined': True,
                        'use_1d_convolutions': True,
                        'ROI_convolution_kernel_x_size': binning[0],     # boxcar filter to mimic binning
                        'ROI_convolution_kernel_y_size': binning[1],
                        'convolution_kernel_x_size': 7,                  # Full frame convolution (gaussian kernel)
                        'convolution_kernel_y_size': 14,
                        'downsample_interp_type': "binning",            # point-sampling, NN-interp, binning
                        'NN_enable': False,
                        'NN_min_neighbors': 6,
                        'NN_patch_size': 5,
                        'NN_range_tolerance': 0.1,
                        'SNR_threshold_enable': False,
                        'SNR_threshold': 0.1}

    device.clear()
    # Using binning on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs_stepping)
    bin_data.append(copy.deepcopy(device.dsp.data))

    device.clear()
    # Using convolution on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device.process_at_full_resolution_with_conv(input_data=input_data,
                                                                          roi_start_vector=start_vector,
                                                                          configs=configs_stepping)
    conv_stepping_data.append(copy.deepcopy(device.dsp.data))

    device.clear()
    # Using convolution on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device.process_at_full_resolution_with_conv(input_data=input_data,
                                                                          roi_start_vector=start_vector,
                                                                          configs=configs_nn_interp)
    conv_nn_interp_data.append(copy.deepcopy(device.dsp.data))

    device.clear()
    # Using convolution on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device.process_at_full_resolution_with_conv(input_data=input_data,
                                                                          roi_start_vector=start_vector,
                                                                          configs=configs_bilinear_interp)
    conv_bilinear_interp_data.append(copy.deepcopy(device.dsp.data))

    device.clear()
    # Using convolution on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device.process_at_full_resolution_with_conv(input_data=input_data,
                                                                          roi_start_vector=start_vector,
                                                                          configs=configs_cubic_interp)
    conv_cubic_interp_data.append(copy.deepcopy(device.dsp.data))

    device.clear()
    # Using convolution on ROIs
    input_data = data_gen.data[input_data_name]
    output_range_array_name = device.process_at_full_resolution_with_conv(input_data=input_data,
                                                                          roi_start_vector=start_vector,
                                                                          configs=configs_averaging)
    conv_averaging_data.append(copy.deepcopy(device.dsp.data))

# 3: Data analysis
# data_viewer_bin.plot_range_simple(range_array_name=output_range_array_name)
# data_viewer_conv.plot_range_simple(range_array_name=output_range_array_name)
import matplotlib.pyplot as plt
plt.figure(1)
# Line 1
plt.subplot(3, 6, 1)
plt.ylabel("VGA")
plt.imshow(bin_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("BIN")
plt.subplot(3, 6, 2)
plt.imshow(conv_stepping_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("CONV - point-sampling")
plt.subplot(3, 6, 3)
plt.imshow(conv_nn_interp_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("CONV - NN interpolation")
plt.subplot(3, 6, 4)
plt.imshow(conv_bilinear_interp_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("CONV - Bilinear interpolation")
plt.subplot(3, 6, 5)
plt.imshow(conv_cubic_interp_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("CONV - Bicubic interpolation")
plt.subplot(3, 6, 6)
plt.imshow(conv_averaging_data[0]['ranges'].flt, cmap='Greys_r')
plt.title("CONV - Binning")

# line 2
plt.subplot(3, 6, 7)
plt.ylabel("QVGA")
plt.imshow(bin_data[1]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 8)
plt.imshow(conv_stepping_data[1]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 9)
plt.imshow(conv_nn_interp_data[1]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 10)
plt.imshow(conv_bilinear_interp_data[1]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 11)
plt.imshow(conv_cubic_interp_data[1]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 12)
plt.imshow(conv_averaging_data[1]['ranges'].flt, cmap='Greys_r')

# line 3
plt.subplot(3, 6, 13)
plt.ylabel("QQVGA")
plt.imshow(bin_data[2]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 14)
plt.imshow(conv_stepping_data[2]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 15)
plt.imshow(conv_nn_interp_data[2]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 16)
plt.imshow(conv_bilinear_interp_data[2]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 17)
plt.imshow(conv_cubic_interp_data[2]['ranges'].flt, cmap='Greys_r')
plt.subplot(3, 6, 18)
plt.imshow(conv_averaging_data[2]['ranges'].flt, cmap='Greys_r')

plt.show()