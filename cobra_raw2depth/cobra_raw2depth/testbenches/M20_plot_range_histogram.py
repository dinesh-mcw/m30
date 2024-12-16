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
path = os.path.join('..', '..', '..', 'ghost_6m')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ghost_6m_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

configs = {'perform_tap_add': perform_tap_add,
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
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 2,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': True,
           'SNR_threshold': 0.03}

input_data = data_gen.data[input_data_name]
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)

# Crop to keep region of interest in final frame
row_top = 90
row_bottom = 150
col_left = 140
col_right = 240
save = np.copy(device.dsp.data["phase_frame"].flt[:, row_top:row_bottom, col_left:col_right])
device.dsp.data["phase_frame"].flt[:, :, :] = 0
device.dsp.data["phase_frame"].flt[:, row_top:row_bottom, col_left:col_right] = save
save = np.copy(device.dsp.data["smoothed_phase"].flt[:, row_top:row_bottom, col_left:col_right])
device.dsp.data["smoothed_phase"].flt[:, :, :] = 0
device.dsp.data["smoothed_phase"].flt[:, row_top:row_bottom, col_left:col_right] = save
save = np.copy(device.dsp.data["ranges_SNR_filtered"].flt[row_top:row_bottom, col_left:col_right])
device.dsp.data["ranges_SNR_filtered"].flt[:, :] = 0
device.dsp.data["ranges_SNR_filtered"].flt[row_top:row_bottom, col_left:col_right] = save

# 3: Range histogram
import matplotlib.pyplot as plt
M_range = (device.dsp._c / (2 * device.dsp.freq[0])) * np.arange(np.amin(device.dsp.data["M"].flt),
                                                                 np.amax(device.dsp.data["M"].flt)+1, 1, dtype=np.int)

plt.figure()
plt.subplot(1,2,1)
plt.imshow(device.dsp.data["ranges_1"].flt, cmap='Greys_r')
plt.subplot(1,2,2)
plt.imshow(device.dsp.data["ranges_2"].flt, cmap='Greys_r')
plt.show()

plt.figure()
plt.hist(device.dsp.data["ranges_SNR_filtered"].flt.flatten(), bins=100)
for m_val in M_range:
    plt.axvline(x=m_val, linestyle='--', color='r')

plt.axvline(x=device.dsp.max_range, linestyle='--', color='g')
plt.xlabel('Range')
plt.ylabel('Count')
plt.title('Range histogram')

save = np.copy(device.dsp.data["M"].flt[row_top:row_bottom, col_left:col_right])
device.dsp.data["M"].flt[:, :] = 0
device.dsp.data["M"].flt[row_top:row_bottom, col_left:col_right] = save

indices = (device.dsp.data["M"].flt == 2)
flattened_phase_0 = device.dsp.data["phase_frame"].flt[0, indices]
flattened_smoothed_phase_0 = device.dsp.data["smoothed_phase"].flt[0, indices]
flattened_phase_1 = device.dsp.data["phase_frame"].flt[1, indices]
flattened_smoothed_phase_1 = device.dsp.data["smoothed_phase"].flt[1, indices]

print('Done')

