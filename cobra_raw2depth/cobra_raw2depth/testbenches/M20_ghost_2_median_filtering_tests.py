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
num_rois = 89                # Number of ROIs per frame
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
           'correct_strips': False,
           'binning': (2, 2),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 5,
           'convolution_kernel_y_size': 7,
           #'convolution_kernel_x': [0.003325727090904847, 0.023817922039352332, 0.09719199228354419, 0.22597815248035666, 0.2993724122116841, 0.22597815248035666, 0.09719199228354419, 0.023817922039352332, 0.003325727090904847],
           #'convolution_kernel_y': [0.0036553302113411516, 0.044531038224040448, 0.19957426729948921, 0.32904233958106088, 0.19957426729948921, 0.044531038224040448, 0.0036553302113411516],
           'M_filter': True,
           'M_filter_loc': 1,
           'M_filter_type': 7,
           'M_median_filter_size': 3,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 0,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 2}

input_data = data_gen.data[input_data_name]

# No median filter
print('No median filter')
configs['M_filter'] = False
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
no_median_filter = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)
#
# Small median filter
print('Small median filter')
configs['M_filter'] = True
configs['M_filter_type'] = 1
configs['M_median_filter_size'] = 5
input_data = data_gen.data[input_data_name]
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
small_median_filter_range = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# Large median filter
print('Large median filter')
configs['M_filter_type'] = 1
configs['M_median_filter_size'] = 11
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
large_median_filter_range = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# Adaptive median filter
print('Adaptive median filter - low threshold')
configs['M_filter_type'] = 7
configs['M_median_filter_size'] = 30
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
adaptive_median_filter_range_low = np.copy(device.dsp.data[output_range_array_name].flt)

# # Adaptive median filter
# print('Adaptive median filter - medium threshold')
# configs['M_filter_type'] = 7
# configs['M_median_filter_size'] = 20
# output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
# adaptive_median_filter_range_medium = np.copy(device.dsp.data[output_range_array_name].flt)
# device.clear(force_clear=True)
#
# # Adaptive median filter
# print('Adaptive median filter - high threshold')
# configs['M_filter_type'] = 7
# configs['M_median_filter_size'] = 10
# output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
# adaptive_median_filter_range_high = np.copy(device.dsp.data[output_range_array_name].flt)
# device.clear(force_clear=True)

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

fig1 = plt.figure(1)
ax11 = fig1.add_subplot(1, 4, 1)
im11 = ax11.imshow(np.flipud(no_median_filter), cmap=ListedColormap(colormap))
ax11.set_title('No median filter')
ax12 = fig1.add_subplot(1, 4, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(small_median_filter_range), cmap=ListedColormap(colormap))
ax12.set_title('Small median filter (5x5 kernel)')
ax13 = fig1.add_subplot(1, 4, 3, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(adaptive_median_filter_range_low), cmap=ListedColormap(colormap))
ax13.set_title('Adaptive median filter')
ax14 = fig1.add_subplot(1, 4, 4, sharex=ax11, sharey=ax11)
im14 = ax14.imshow(np.flipud(large_median_filter_range), cmap=ListedColormap(colormap))
ax14.set_title('Large median filter (11x11 kernel)')
plt.colorbar(im14, ax=(ax11, ax12, ax13, ax14), location='bottom')

# fig1 = plt.figure(2)
# ax11 = fig1.add_subplot(1, 5, 1)
# im11 = ax11.imshow(np.flipud(small_median_filter_range), cmap=ListedColormap(colormap))
# ax11.set_title('Small median filter (5x5 kernel)')
# ax12 = fig1.add_subplot(1, 5, 2, sharex=ax11, sharey=ax11)
# im12 = ax12.imshow(np.flipud(adaptive_median_filter_range_high), cmap=ListedColormap(colormap))
# ax12.set_title('Adaptive median filter - low threshold for edges')
# ax13 = fig1.add_subplot(1, 5, 3, sharex=ax11, sharey=ax11)
# im13 = ax13.imshow(np.flipud(adaptive_median_filter_range_medium), cmap=ListedColormap(colormap))
# ax13.set_title('Adaptive median filter - medium threshold for edges')
# ax14 = fig1.add_subplot(1, 5, 4, sharex=ax11, sharey=ax11)
# im14 = ax14.imshow(np.flipud(adaptive_median_filter_range_low), cmap=ListedColormap(colormap))
# ax14.set_title('Adaptive median filter - high threshold for edges')
# ax15 = fig1.add_subplot(1, 5, 5, sharex=ax11, sharey=ax11)
# im15 = ax15.imshow(np.flipud(large_median_filter_range), cmap=ListedColormap(colormap))
# ax15.set_title('Large median filter (11x11  kernel)')
# plt.colorbar(im14, ax=(ax11, ax12, ax13, ax14, ax15), location='bottom')
# plt.show()

# 3: Data analysis
active_region_1x1 = (200, 280, 380, 460)
active_region_2x2 = tuple(int(ti/2) for ti in active_region_1x1)
active_region_4x4 = tuple(int(ti/4) for ti in active_region_1x1)
active_region = None
data_viewer.assign_dict(device.dsp.data)
data_viewer.plot_snr_simple(save_figure=False, flip_ud=True, active_region=active_region)
# data_viewer.plot_snr_histogram(active_region=active_region, bins=100)
# data_viewer.plot_M(active_region=active_region)
# data_viewer.plot_M_histogram(active_region=active_region)
# data_viewer.plot_range_simple(range_array_name=output_range_array_name, flip_ud=False, save_figure=False,
                              # active_region=active_region)
# data_viewer.plot_range_histogram(range_array_name=output_range_array_name, superimpose_M_values=False,
#                                  active_region=active_region)
# data_viewer.plot_snr_vs_range()
# data_viewer.dump_frames_to_binary_files()
# data_viewer.dump_range_arrays()

