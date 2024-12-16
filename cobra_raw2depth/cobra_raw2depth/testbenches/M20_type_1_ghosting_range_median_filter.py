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
           'M_filter': False,
           'M_filter_loc': 1,
           'M_filter_type': 2,
           'M_median_filter_size': 3,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'range_median_filter_en': True,
           'range_median_filter_size': None,
           'range_median_filter_shape': None,
           'range_median_filter_mask': False,
           'NN_enable': False,
           'NN_filter_level': 0,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': True,
           'SNR_threshold': 0}

range_array = []
input_data = data_gen.data[input_data_name]

# Different settings of range median filter, 2x2 binning
# 1: None
configs['range_edge_filter_en'] = False
configs['range_median_filter_en'] = False
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

# 2: Window shaped
configs['range_edge_filter_en'] = False
configs['range_median_filter_en'] = True
configs['range_median_filter_mask'] = False
configs['range_median_filter_size'] = (configs['convolution_kernel_y_size'], configs['convolution_kernel_x_size'])
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

# 3: + shaped
configs['range_median_filter_size'] = (configs['convolution_kernel_y_size'], configs['convolution_kernel_x_size'])
configs['range_median_filter_shape'] = '+'
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

# 4: x shaped
configs['range_median_filter_size'] = (configs['convolution_kernel_y_size'], configs['convolution_kernel_y_size'])
configs['range_median_filter_shape'] = 'x'
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

# 5: Sharpening mask (Laplace)
configs['range_median_filter_en'] = False
configs['range_edge_filter_en'] = True
configs['range_edge_filter_type'] = 'laplace'
configs['range_edge_filter_thres'] = 0.25

output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

# 6: Sharpening mask (Sobel)
configs['range_edge_filter_en'] = True
configs['range_edge_filter_type'] = 'sobel'
configs['range_edge_filter_thres'] = 0.25
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
range_array.append(np.copy(device.dsp.data[output_range_array_name].flt))
device.clear(force_clear=True)

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

titles = ['No filter', 'Window shaped median filter', '+ shaped median filter', 'x shaped median filter',
          'Laplace mask', 'Sobel Mask']
fig = plt.figure(1)
ax1 = fig.add_subplot(2, 3, 1)
im1 = ax1.imshow(np.flipud(range_array[0]), cmap=ListedColormap(colormap))
ax1.set_title(titles[0])
ax2 = fig.add_subplot(2, 3, 2)
im2 = ax2.imshow(np.flipud(range_array[1]), cmap=ListedColormap(colormap))
ax2.set_title(titles[1])
ax3 = fig.add_subplot(2, 3, 3)
im3 = ax3.imshow(np.flipud(range_array[2]), cmap=ListedColormap(colormap))
ax3.set_title(titles[2])
ax4 = fig.add_subplot(2, 3, 4)
im4 = ax4.imshow(np.flipud(range_array[3]), cmap=ListedColormap(colormap))
ax4.set_title(titles[3])
ax5 = fig.add_subplot(2, 3, 5)
im5 = ax5.imshow(np.flipud(range_array[4]), cmap=ListedColormap(colormap))
ax5.set_title(titles[4])
ax5 = fig.add_subplot(2, 3, 6)
im5 = ax5.imshow(np.flipud(range_array[5]), cmap=ListedColormap(colormap))
ax5.set_title(titles[5])
plt.show()
