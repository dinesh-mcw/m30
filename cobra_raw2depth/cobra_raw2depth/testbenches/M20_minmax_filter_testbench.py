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
num_frames = 1  # The device processes 1 frame at a time

num_rows_per_roi = 20  # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640  # Number of columns per ROI at the input (before any binning)
num_rois = 89  # Number of ROIs per frame
num_rows_full_frame = 450

data_gen_1 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_1 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                             num_rois, num_rows_full_frame)

num_rows_per_roi = 20  # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640  # Number of columns per ROI at the input (before any binning)
num_rois = 91  # Number of ROIs per frame
num_rows_full_frame = 450

data_gen_2 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_2 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_rows_per_roi = 20  # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640  # Number of columns per ROI at the input (before any binning)
num_rois = 91  # Number of ROIs per frame
num_rows_full_frame = 460

data_gen_3 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_3 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_rows_per_roi = 20  # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640  # Number of columns per ROI at the input (before any binning)
num_rois = 91  # Number of ROIs per frame
num_rows_full_frame = 460

data_gen_4 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_4 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_rows_per_roi = 20  # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640  # Number of columns per ROI at the input (before any binning)
num_rois = 91  # Number of ROIs per frame
num_rows_full_frame = 460

data_gen_5 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_5 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

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
input_data_name1, input_data_shape1, perform_tap_add1 = \
    data_gen_1.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

path = os.path.join('..', '..', '..', 'ghosting_raw_data-20211222T184525Z-001', 'ghosting_raw_data')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ghost_0_05_'
input_data_name2, input_data_shape2, perform_tap_add2 = \
    data_gen_2.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

path = os.path.join('..', '..', '..', 'raw_data_20220517', '10_percent_test')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = '10_percent_0_01_'
input_data_name3, input_data_shape3, perform_tap_add3 = \
    data_gen_3.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

path = os.path.join('..', '..', '..', 'raw_data_20220517', 'pedestrian_subset')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'pedestrian_0_01_'
input_data_name4, input_data_shape4, perform_tap_add4 = \
    data_gen_4.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

path = os.path.join('..', '..', '..', 'raw_data_20220517', 'retro')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'retro_0_01_'
input_data_name5, input_data_shape5, perform_tap_add5 = \
    data_gen_5.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
# 2: Process
configs1 = {'perform_tap_add': perform_tap_add1,
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
           'M_filter_loc': 0,
           'M_filter_type': 3,
           'M_median_filter_size': 3,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 0,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 1/16,
           'SNR_threshold_enable': False,
           'SNR_threshold': 2}

configs2 = configs1.copy()
configs2['perform_tap_add'] = perform_tap_add2
configs3 = configs1.copy()
configs3['perform_tap_add'] = perform_tap_add3
configs4 = configs1.copy()
configs4['perform_tap_add'] = perform_tap_add4
configs5 = configs1.copy()
configs5['perform_tap_add'] = perform_tap_add5

first_input_data = data_gen_1.data[input_data_name1]
first_start_vector = data_gen_1.data["ROI_start_vector"]
second_input_data = data_gen_2.data[input_data_name2]
second_start_vector = data_gen_2.data["ROI_start_vector"]
third_input_data = data_gen_3.data[input_data_name3]
third_start_vector = data_gen_3.data["ROI_start_vector"]
fourth_input_data = data_gen_4.data[input_data_name4]
fourth_start_vector = data_gen_4.data["ROI_start_vector"]
fifth_input_data = data_gen_5.data[input_data_name5]
fifth_start_vector = data_gen_5.data["ROI_start_vector"]

depthmaps_first_output = []
depthmaps_second_output = []
depthmaps_third_output = []
depthmaps_fourth_output = []
depthmaps_fifth_output = []
titles = ['Vanilla minmax', 'Recursive minmax', '7x7 minmax w STD test + 3x3 minmax on M']

nn_level = 0
# 1: Vanilla minmax
configs1['range_median_filter_en'] = True
configs1['range_median_filter_size'] = [5, 5]
configs1['range_median_filter_shape'] = '+'
configs2['range_median_filter_en'] = True
configs2['range_median_filter_size'] = [5, 5]
configs2['range_median_filter_shape'] = '+'
configs3['range_median_filter_en'] = True
configs3['range_median_filter_size'] = [5, 5]
configs3['range_median_filter_shape'] = '+'
configs4['range_median_filter_en'] = True
configs4['range_median_filter_size'] = [5, 5]
configs4['range_median_filter_shape'] = '+'
configs5['range_median_filter_en'] = True
configs5['range_median_filter_size'] = [5, 5]
configs5['range_median_filter_shape'] = '+'

configs1['NN_enable'] = False
configs1['NN_filter_level'] = nn_level
configs2['NN_enable'] = False
configs2['NN_filter_level'] = nn_level
configs3['NN_enable'] = False
configs3['NN_filter_level'] = nn_level
configs4['NN_enable'] = False
configs4['NN_filter_level'] = nn_level
configs5['NN_enable'] = False
configs5['NN_filter_level'] = nn_level

configs1['M_filter'] = True
configs1['M_filter_type'] = 3
configs2['M_filter'] = True
configs2['M_filter_type'] = 3
configs3['M_filter'] = True
configs3['M_filter_type'] = 3
configs4['M_filter'] = True
configs4['M_filter_type'] = 3
configs5['M_filter'] = True
configs5['M_filter_type'] = 3
output_range_array_name = device_1(input_data=first_input_data, roi_start_vector=first_start_vector, configs=configs1)
depthmaps_first_output.append(device_1.dsp.data[output_range_array_name].flt)
output_range_array_name = device_2(input_data=second_input_data, roi_start_vector=second_start_vector, configs=configs2)
depthmaps_second_output.append(device_2.dsp.data[output_range_array_name].flt)
output_range_array_name = device_3(input_data=third_input_data, roi_start_vector=third_start_vector, configs=configs3)
depthmaps_third_output.append(device_3.dsp.data[output_range_array_name].flt)
output_range_array_name = device_4(input_data=fourth_input_data, roi_start_vector=fourth_start_vector, configs=configs4)
depthmaps_fourth_output.append(device_4.dsp.data[output_range_array_name].flt)
output_range_array_name = device_5(input_data=fifth_input_data, roi_start_vector=fifth_start_vector, configs=configs5)
depthmaps_fifth_output.append(device_5.dsp.data[output_range_array_name].flt)

device_1.clear(force_clear=True)
device_2.clear(force_clear=True)
device_3.clear(force_clear=True)
device_4.clear(force_clear=True)
device_5.clear(force_clear=True)

# 2: Recursive minmax with forward/backward compensation
configs1['M_filter'] = True
configs1['M_filter_type'] = 8
configs2['M_filter'] = True
configs2['M_filter_type'] = 8
configs3['M_filter'] = True
configs3['M_filter_type'] = 8
configs4['M_filter'] = True
configs4['M_filter_type'] = 8
configs5['M_filter'] = True
configs5['M_filter_type'] = 8

output_range_array_name = device_1(input_data=first_input_data, roi_start_vector=first_start_vector, configs=configs1)
depthmaps_first_output.append(device_1.dsp.data[output_range_array_name].flt)
output_range_array_name = device_2(input_data=second_input_data, roi_start_vector=second_start_vector, configs=configs2)
depthmaps_second_output.append(device_2.dsp.data[output_range_array_name].flt)
output_range_array_name = device_3(input_data=third_input_data, roi_start_vector=third_start_vector, configs=configs3)
depthmaps_third_output.append(device_3.dsp.data[output_range_array_name].flt)
output_range_array_name = device_4(input_data=fourth_input_data, roi_start_vector=fourth_start_vector, configs=configs4)
depthmaps_fourth_output.append(device_4.dsp.data[output_range_array_name].flt)
output_range_array_name = device_5(input_data=fifth_input_data, roi_start_vector=fifth_start_vector, configs=configs5)
depthmaps_fifth_output.append(device_5.dsp.data[output_range_array_name].flt)

device_1.clear(force_clear=True)
device_2.clear(force_clear=True)
device_3.clear(force_clear=True)
device_4.clear(force_clear=True)
device_5.clear(force_clear=True)

# 3: minmax w STD
configs1['M_filter'] = True
configs1['M_filter_type'] = 9
configs2['M_filter'] = True
configs2['M_filter_type'] = 9
configs3['M_filter'] = True
configs3['M_filter_type'] = 9
configs4['M_filter'] = True
configs4['M_filter_type'] = 9
configs5['M_filter'] = True
configs5['M_filter_type'] = 9
output_range_array_name = device_1(input_data=first_input_data, roi_start_vector=first_start_vector, configs=configs1)
depthmaps_first_output.append(device_1.dsp.data[output_range_array_name].flt)
output_range_array_name = device_2(input_data=second_input_data, roi_start_vector=second_start_vector, configs=configs2)
depthmaps_second_output.append(device_2.dsp.data[output_range_array_name].flt)
output_range_array_name = device_3(input_data=third_input_data, roi_start_vector=third_start_vector, configs=configs3)
depthmaps_third_output.append(device_3.dsp.data[output_range_array_name].flt)
output_range_array_name = device_4(input_data=fourth_input_data, roi_start_vector=fourth_start_vector, configs=configs4)
depthmaps_fourth_output.append(device_4.dsp.data[output_range_array_name].flt)
output_range_array_name = device_5(input_data=fifth_input_data, roi_start_vector=fifth_start_vector, configs=configs5)
depthmaps_fifth_output.append(device_5.dsp.data[output_range_array_name].flt)

device_1.clear(force_clear=True)
device_2.clear(force_clear=True)
device_3.clear(force_clear=True)
device_4.clear(force_clear=True)
device_5.clear(force_clear=True)


## Show results
colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

# fig = plt.figure(1, figsize=(30, 10))
# ax1 = fig.add_subplot(1, 3, 1)
# im1 = ax1.imshow(np.flipud(depthmaps_first_output[0]), cmap=ListedColormap(colormap))
# im1.set_clim(vmin=1, vmax=26)
# ax1.set_title(titles[0])
# ax2 = fig.add_subplot(1, 3, 2, sharex = ax1, sharey=ax1)
# im2 = ax2.imshow(np.flipud(depthmaps_first_output[1]), cmap=ListedColormap(colormap))
# im2.set_clim(vmin=1, vmax=26)
# ax2.set_title(titles[1])
# ax3 = fig.add_subplot(1, 3, 3, sharex = ax1, sharey=ax1)
# im3 = ax3.imshow(np.flipud(depthmaps_first_output[2]), cmap=ListedColormap(colormap))
# im3.set_clim(vmin=1, vmax=26)
# ax3.set_title(titles[2])
# plt.colorbar(im3, ax=(ax1, ax2, ax3), location='bottom')
#
# fig2 = plt.figure(2, figsize=(30, 10))
# ax1 = fig2.add_subplot(1, 3, 1)
# im1 = ax1.imshow(np.flipud(depthmaps_second_output[0]), cmap=ListedColormap(colormap))
# ax1.set_title(titles[0])
# ax2 = fig2.add_subplot(1, 3, 2)
# im2 = ax2.imshow(np.flipud(depthmaps_second_output[1]), cmap=ListedColormap(colormap))
# ax2.set_title(titles[1])
# ax3 = fig2.add_subplot(1, 3, 3)
# im3 = ax3.imshow(np.flipud(depthmaps_second_output[2]), cmap=ListedColormap(colormap))
# ax3.set_title(titles[2])
# plt.colorbar(im3, ax=(ax1, ax2, ax3), location='bottom')

fig3 = plt.figure(3)
ax1 = fig3.add_subplot(3, 5, 1)
im1 = ax1.imshow(np.flipud(depthmaps_first_output[0]), cmap=ListedColormap(colormap))
ax1.set_title(titles[0])
im1.set_clim(vmin=0, vmax=26)
for row_idx in range(0, 3):
    ax = fig3.add_subplot(3, 5, row_idx*5 + 1, sharex=ax1, sharey=ax1)
    im = ax.imshow(np.flipud(depthmaps_first_output[row_idx]), cmap=ListedColormap(colormap))
    im.set_clim(vmin=0, vmax=26)
    ax.set_title(titles[row_idx])

    ax = fig3.add_subplot(3, 5, row_idx*5 + 2, sharex=ax1, sharey=ax1)
    im = ax.imshow(np.flipud(depthmaps_second_output[row_idx]), cmap=ListedColormap(colormap))
    im.set_clim(vmin=0, vmax=26)
    ax.set_title(titles[row_idx])

    ax = fig3.add_subplot(3, 5, row_idx*5 + 3, sharex=ax1, sharey=ax1)
    im = ax.imshow(np.flipud(depthmaps_third_output[row_idx]), cmap=ListedColormap(colormap))
    im.set_clim(vmin=0, vmax=26)
    ax.set_title(titles[row_idx])

    ax = fig3.add_subplot(3, 5, row_idx*5 + 4, sharex=ax1, sharey=ax1)
    im = ax.imshow(np.flipud(depthmaps_fourth_output[row_idx]), cmap=ListedColormap(colormap))
    im.set_clim(vmin=0, vmax=26)
    ax.set_title(titles[row_idx])

    ax = fig3.add_subplot(3, 5, row_idx*5 + 5, sharex=ax1, sharey=ax1)
    im = ax.imshow(np.flipud(depthmaps_fifth_output[row_idx]), cmap=ListedColormap(colormap))
    im.set_clim(vmin=0, vmax=26)
    ax.set_title(titles[row_idx])


# ax3 = fig3.add_subplot(3, 5, 3, sharex = ax1, sharey=ax1)
# im3 = ax3.imshow(np.flipud(depthmaps_third_output[0]), cmap=ListedColormap(colormap))
# im3.set_clim(vmin=0, vmax=26)
# ax3.set_title(titles[0])
# ax4 = fig3.add_subplot(3, 5, 4)
# im4 = ax4.imshow(np.flipud(depthmaps_fourth_output[0]), cmap=ListedColormap(colormap))
# im4.set_clim(vmin=0, vmax=26)
# ax4.set_title(titles[1])
# ax5 = fig3.add_subplot(3, 3, 5, sharex = ax1, sharey=ax1)
# im5 = ax5.imshow(np.flipud(depthmaps_fifth_output[0]), cmap=ListedColormap(colormap))
# im5.set_clim(vmin=0, vmax=26)
# ax5.set_title(titles[1])
# ax6 = fig3.add_subplot(3, 3, 6, sharex = ax1, sharey=ax1)
# im6 = ax6.imshow(np.flipud(depthmaps_first_output[1]), cmap=ListedColormap(colormap))
# im6.set_clim(vmin=0, vmax=26)
# ax6.set_title(titles[1])
# ax7 = fig3.add_subplot(3, 3, 7)
# im7 = ax7.imshow(np.flipud(depthmaps_second_output[2]), cmap=ListedColormap(colormap))
# im7.set_clim(vmin=0, vmax=26)
# ax7.set_title(titles[2])
# ax8 = fig3.add_subplot(3, 3, 8, sharex = ax1, sharey=ax1)
# im8 = ax8.imshow(np.flipud(depthmaps_third_output[2]), cmap=ListedColormap(colormap))
# im8.set_clim(vmin=0, vmax=26)
# ax8.set_title(titles[2])
# ax9 = fig3.add_subplot(3, 3, 9, sharex = ax1, sharey=ax1)
# im9 = ax9.imshow(np.flipud(depthmaps_fourth_output[2]), cmap=ListedColormap(colormap))
# im9.set_clim(vmin=0, vmax=26)
# ax9.set_title(titles[2])
# plt.colorbar(im3, ax=(ax1, ax2, ax3), location='bottom')

plt.show()