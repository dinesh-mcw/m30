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
num_columns_per_roi = 640    # Number of columns per ROI at the input (before any binning)
num_rois = 89                # Number of ROIs per frame
num_rows_full_frame = 450
num_frames = 2               # The device processes 1 frame at a time

data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'strips_investiguation', 'normal', 'normal_ci15')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'normal_ci15_0sbr_0nn_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'binning': (1, 1),
           'SNR_voting_combined': False,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 7,
           'convolution_kernel_y_size': 7,
           #'convolution_kernel_x': [0.003325727090904847, 0.023817922039352332, 0.09719199228354419, 0.22597815248035666, 0.2993724122116841, 0.22597815248035666, 0.09719199228354419, 0.023817922039352332, 0.003325727090904847],
           #'convolution_kernel_y': [0.0036553302113411516, 0.044531038224040448, 0.19957426729948921, 0.32904233958106088, 0.19957426729948921, 0.044531038224040448, 0.0036553302113411516],
           'M_filter': False,
           'M_filter_type': 5,
           'M_median_filter_size': 7,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 2,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0}

# High frequency
phase_unwrapping_error_range_bracket = (device.dsp._c / (2*device.dsp.freq[1])) / 2

M = []
M_median_filter = []
M_std_improv = []
M_mad_improv = []
M_custom_mean_SNR = []

RANGE = []
RANGE_median_filter = []
RANGE_std_improv = []
RANGE_mad_improv = []
RANGE_custom_mean_SNR = []

# active_region_1x1 = (200, 270, 220, 360)
# active_region_2x2 = tuple(int(ti/2) for ti in active_region_1x1)
# active_region_4x4 = tuple(int(ti/4) for ti in active_region_1x1)
# active_region = None

for frame_num in range(len(data_gen.data[input_data_name])):

    print('Convolution alone')
    configs['enable_convolution'] = True
    input_data = data_gen.data[input_data_name][frame_num]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame_num], configs=configs)
    range_map = np.copy(device.dsp.data[output_range_array_name].flt)
    RANGE.append(np.copy(device.dsp.data[output_range_array_name].flt))
    M.append(np.copy(device.dsp.data["M"].flt))
    device.clear(force_clear=True)

    print('Median filter')
    configs['M_filter'] = True
    configs['M_filter_type'] = 1
    configs['M_median_filter_size'] = 11
    input_data = data_gen.data[input_data_name][frame_num]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame_num], configs=configs)
    range_map = np.copy(device.dsp.data[output_range_array_name].flt)
    RANGE_median_filter.append(np.copy(device.dsp.data[output_range_array_name].flt))
    M_median_filter.append(np.copy(device.dsp.data["M"].flt))
    device.clear(force_clear=True)

    print('Std improv')
    configs['M_filter'] = True
    configs['M_filter_type'] = 4
    configs['M_median_filter_size'] = 11
    input_data = data_gen.data[input_data_name][frame_num]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame_num], configs=configs)
    range_map = np.copy(device.dsp.data[output_range_array_name].flt)
    RANGE_std_improv.append(np.copy(device.dsp.data[output_range_array_name].flt))
    M_std_improv.append(np.copy(device.dsp.data["M"].flt))
    device.clear(force_clear=True)

    print('MAD improv')
    configs['M_filter'] = True
    configs['M_filter_type'] = 5
    configs['M_median_filter_size'] = 11
    input_data = data_gen.data[input_data_name][frame_num]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame_num], configs=configs)
    range_map = np.copy(device.dsp.data[output_range_array_name].flt)
    RANGE_mad_improv.append(np.copy(device.dsp.data[output_range_array_name].flt))
    M_mad_improv.append(np.copy(device.dsp.data["M"].flt))
    device.clear(force_clear=True)

    print('Custom SNR filter')
    configs['M_filter'] = True
    configs['M_filter_type'] = 6
    configs['M_median_filter_size'] = 11
    input_data = data_gen.data[input_data_name][frame_num]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame_num], configs=configs)
    range_map = np.copy(device.dsp.data[output_range_array_name].flt)
    RANGE_custom_mean_SNR.append(np.copy(device.dsp.data[output_range_array_name].flt))
    M_custom_mean_SNR.append(np.copy(device.dsp.data["M"].flt))
    device.clear(force_clear=True)

    break


import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))

M = np.mean(np.asarray(M), axis=0) # take the mean across all frames
M_median_filter = np.mean(np.asarray(M_median_filter), axis=0) # take the mean across all frames
M_std_improv = np.mean(np.asarray(M_std_improv), axis=0) # take the mean across all frames
M_mad_improv = np.mean(np.asarray(M_mad_improv), axis=0) # take the mean across all frames
M_custom_mean_SNR = np.mean(np.asarray(M_custom_mean_SNR), axis=0) # take the mean across all frames

RANGE = np.mean(np.asarray(RANGE), axis=0) # take the mean across all frames
RANGE_median_filter = np.mean(np.asarray(RANGE_median_filter), axis=0) # take the mean across all frames
RANGE_std_improv = np.mean(np.asarray(RANGE_std_improv), axis=0)
RANGE_mad_improv = np.mean(np.asarray(RANGE_mad_improv), axis=0)
RANGE_custom_mean_SNR = np.mean(np.asarray(RANGE_custom_mean_SNR), axis=0) # take the mean across all frames

# M
fig1 = plt.figure(1)
ax11 = fig1.add_subplot(1, 5, 1)
im11 = ax11.imshow(np.flipud(M), cmap=ListedColormap(colormap))
ax11.set_title('M - Convolution only')
ax12 = fig1.add_subplot(1, 5, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(M_median_filter), cmap=ListedColormap(colormap))
ax12.set_title('M - With Median filter')
ax15 = fig1.add_subplot(1, 5, 3, sharex=ax11, sharey=ax11)
im15 = ax15.imshow(np.flipud(M_std_improv), cmap=ListedColormap(colormap))
ax15.set_title('M - With STD improv')
ax16 = fig1.add_subplot(1, 5, 4, sharex=ax11, sharey=ax11)
im16 = ax16.imshow(np.flipud(M_mad_improv), cmap=ListedColormap(colormap))
ax16.set_title('M - With MAD improv')
ax13 = fig1.add_subplot(1, 5, 5, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(M_custom_mean_SNR), cmap=ListedColormap(colormap))
ax13.set_title('M - Custom SNR filter')
plt.colorbar(im16, ax=(ax11, ax12, ax13, ax15, ax16), location='bottom')

# Range
fig3 = plt.figure(2)
ax11 = fig3.add_subplot(1, 5, 1)
im11 = ax11.imshow(np.flipud(RANGE), cmap=ListedColormap(colormap))
ax11.set_title('Range - Convolution only')
ax12 = fig3.add_subplot(1, 5, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(RANGE_median_filter), cmap=ListedColormap(colormap))
ax12.set_title('Range - With Median filter')
ax15 = fig3.add_subplot(1, 5, 3, sharex=ax11, sharey=ax11)
im15 = ax15.imshow(np.flipud(RANGE_std_improv), cmap=ListedColormap(colormap))
ax15.set_title('Range - With STD improv')
ax13 = fig3.add_subplot(1, 5, 4, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(RANGE_mad_improv), cmap=ListedColormap(colormap))
ax13.set_title('Range - With MAD improv')
ax14 = fig3.add_subplot(1, 5, 5, sharex=ax11, sharey=ax11)
im14 = ax14.imshow(np.flipud(RANGE_custom_mean_SNR), cmap=ListedColormap(colormap))
ax14.set_title('Range - Custom SNR filter')
plt.colorbar(im14, ax=(ax11, ax12, ax13, ax14, ax15), location='bottom')

plt.show()


