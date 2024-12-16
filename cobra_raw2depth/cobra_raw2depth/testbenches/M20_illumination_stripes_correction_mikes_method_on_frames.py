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
           'correct_strips_frame': False,
           'binning': (1, 1),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 7,
           'convolution_kernel_y_size': 15,
           'convolution_kernel_x': np.asarray([124, 411, 1129, 2580, 4908, 7769, 10233, 11218,
                                               10233, 7769, 4908, 2580, 1129, 411, 124]),
           'convolution_kernel_y': np.asarray([240, 2918, 13079, 21564, 13079, 2918, 240]),
           'M_filter': False,
           'M_filter_loc': 0,
           'M_filter_type': 7,
           'M_median_filter_size': 5,
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
# No convolution
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
no_correction_depth_map = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# ROIs strip correction
configs['correct_strips'] = True
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
ROI_correction_depth_map = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# frames strip correction
configs['correct_strips'] = False
configs['correct_strips_frame'] = True
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
Frame_correction_depth_map = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# Display data
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))

fig = plt.figure()
ax11 = fig.add_subplot(1, 3, 1)
im11 = ax11.imshow(np.flipud(no_correction_depth_map), cmap=ListedColormap(colormap))
ax11.set_title('Range map - Do nothing')
ax12 = fig.add_subplot(1, 3, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(ROI_correction_depth_map), cmap=ListedColormap(colormap))
ax12.set_title('Range map - Replacing bad with averaged good at ROI level')
ax13 = fig.add_subplot(1, 3, 3, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(Frame_correction_depth_map), cmap=ListedColormap(colormap))
ax13.set_title('Range map - Replacing bad with averaged good at frame level (smoothed phase only)')
plt.colorbar(im13, ax=(ax11, ax12, ax13), location='bottom')

plt.show()
