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
num_rows_full_frame = 460

data_gen_1p0 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_1p0 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 220                # Number of ROIs per frame
num_rows_full_frame = 460

data_gen_0p2 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_0p2 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_frames = 2              # The device processes 1 frame at a time
data_viewer = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'strips_investiguation', 'angular_resolution_tests', '1p0')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ang_res_1p0_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen_1p0.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

path = os.path.join('..', '..', '..', 'strips_investiguation', 'angular_resolution_tests', '0p2')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ang_res_0p2_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen_0p2.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector_1p0 = data_gen_1p0.data["ROI_start_vector"]
start_vector_0p2 = data_gen_0p2.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'binning': (1, 1),
           'SNR_voting_combined': True,
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

SNR_1p0 = []
SNR_1p0_mike = []
SNR_1p0_thres = []
SNR_0p2 = []
signal_1p0 = []
signal_0p2 = []
background_1p0 = []
background_0p2 = []
total_intensity_1p0 = []
total_intensity_0p2 = []
RANGE_1p0 = []
RANGE_1p0_mike = []
RANGE_1p0_thres = []
RANGE_0p2 = []
for frame_num in range(data_gen_1p0.num_loaded_frames):

    print('Do nothing')
    configs['correct_strips'] = False
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0[frame_num], configs=configs)
    range_map_standard = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    RANGE_1p0.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))
    SNR_1p0.append(np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0)))
    signal_1p0.append(np.copy(np.mean(device_1p0.dsp.data["signal_frame"].flt, axis=0)))
    background_1p0.append(np.copy(np.mean(device_1p0.dsp.data["background_frame"].flt, axis=0)))
    total_intensity_1p0.append(np.copy(np.mean(np.sum(device_1p0.dsp.data["combined_data_frame"].flt, axis=0), axis=0)))
    device_1p0.clear(force_clear=True)

    print('Mikes method')
    configs['correct_strips'] = True
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0[frame_num], configs=configs)
    range_map_mike = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    RANGE_1p0_mike.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))
    SNR_1p0_mike.append(np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0)))
    device_1p0.clear(force_clear=True)

    print('Thresholding')
    configs['correct_strips'] = False
    configs['SNR_voting_thres_enable'] = True
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0[frame_num], configs=configs)
    range_map_thres = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    RANGE_1p0_thres.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))
    SNR_1p0_thres.append(np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0)))
    device_1p0.clear(force_clear=True)

    print('0.2 deg')
    configs['correct_strips'] = False
    configs['SNR_voting_thres_enable'] = False
    input_data = data_gen_0p2.data[input_data_name][frame_num]
    output_range_array_name = device_0p2(input_data=input_data, roi_start_vector=start_vector_0p2[frame_num], configs=configs)
    range_map_02 = np.copy(device_0p2.dsp.data[output_range_array_name].flt)
    RANGE_0p2.append(np.copy(device_0p2.dsp.data[output_range_array_name].flt))
    SNR_0p2.append(np.copy(np.mean(device_0p2.dsp.data["SNR_frame"].flt, axis=0)))
    signal_0p2.append(np.copy(np.mean(device_0p2.dsp.data["signal_frame"].flt, axis=0)))
    background_0p2.append(np.copy(np.mean(device_0p2.dsp.data["background_frame"].flt, axis=0)))
    total_intensity_0p2.append(np.copy(np.mean(np.sum(device_0p2.dsp.data["combined_data_frame"].flt, axis=0), axis=0)))
    device_0p2.clear(force_clear=True)
    break


import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))

SNR_1p0 = np.mean(np.asarray(SNR_1p0), axis=0) # take the mean across all frames
SNR_1p0_mike = np.mean(np.asarray(SNR_1p0_mike), axis=0) # take the mean across all frames
SNR_1p0_thres = np.mean(np.asarray(SNR_1p0_thres), axis=0) # take the mean across all frames
SNR_0p2 = np.mean(np.asarray(SNR_0p2), axis=0) # take the mean across all frames
signal_1p0 = np.mean(np.asarray(signal_1p0), axis=0) # take the mean across all frames
signal_0p2 = np.mean(np.asarray(signal_0p2), axis=0) # take the mean across all frames
background_1p0 = np.mean(np.asarray(background_1p0), axis=0) # take the mean across all frames
background_0p2 = np.mean(np.asarray(background_0p2), axis=0) # take the mean across all frames
total_intensity_1p0 = np.mean(np.asarray(total_intensity_1p0), axis=0) # take the mean across all frames
total_intensity_0p2 = np.mean(np.asarray(total_intensity_0p2), axis=0) # take the mean across all
RANGE_1p0 = np.mean(np.asarray(RANGE_1p0), axis=0) # take the mean across all frames
RANGE_1p0_mike = np.mean(np.asarray(RANGE_1p0_mike), axis=0) # take the mean across all frames
RANGE_1p0_thres = np.mean(np.asarray(RANGE_1p0_thres), axis=0) # take the mean across all frames
RANGE_0p2 = np.mean(np.asarray(RANGE_0p2), axis=0) # take the mean across all

# indices_to_mask = np.logical_or(SNR_1p0 < 1, SNR_0p2 < 1)
# SNR_1p0[indices_to_mask] = np.nan
# SNR_0p2[indices_to_mask] = np.nan

average_SNR_gain = np.nanmean(SNR_0p2 - SNR_1p0)
std_SNR_gain = np.nanstd(SNR_0p2 - SNR_1p0)
print("Average SNR gain: "+str(average_SNR_gain)+", STD SNR gain: "+str(std_SNR_gain))

# SNR/intensity/background map
fig1 = plt.figure(1)
ax1 = fig1.add_subplot(1, 4, 1)
im1 = ax1.imshow(np.flipud(SNR_1p0), cmap=ListedColormap(colormap))
ax1.set_title('SNR map (1.0 deg angular res) - Do nothing')
ax2 = fig1.add_subplot(1, 4, 2, sharex=ax1, sharey=ax1)
im2 = ax2.imshow(np.flipud(SNR_1p0_mike), cmap=ListedColormap(colormap))
ax2.set_title('SNR map (1.0 deg angular res) - Replacing bad with averaged good')
ax3 = fig1.add_subplot(1, 4, 3, sharex=ax1, sharey=ax1)
im3 = ax3.imshow(np.flipud(SNR_1p0_thres), cmap=ListedColormap(colormap))
ax3.set_title('SNR map (1.0 deg angular res) - Thresholding (z-scoring)')
ax4 = fig1.add_subplot(1, 4, 4, sharex=ax1, sharey=ax1)
im4 = ax4.imshow(np.flipud(SNR_0p2), cmap=ListedColormap(colormap))
ax4.set_title('SNR map (0.2 deg angular res)')
plt.colorbar(im4, ax=(ax1, ax2, ax3, ax4), location='bottom')

# # SNR/intensity/background map
# fig1 = plt.figure(2)
# ax1 = fig1.add_subplot(1, 2, 1)
# im1 = ax1.imshow(np.flipud(signal_1p0), cmap=ListedColormap(colormap))
# ax1.set_title('signal map (1.0 deg angular res)')
# ax2 = fig1.add_subplot(1, 2, 2, sharex=ax1, sharey=ax1)
# im2 = ax2.imshow(np.flipud(signal_0p2), cmap=ListedColormap(colormap))
# ax2.set_title('signal map (0.2 deg angular res)')
# plt.colorbar(im2, ax=(ax1, ax2), location='bottom')
#
# # SNR/intensity/background map
# fig1 = plt.figure(3)
# ax1 = fig1.add_subplot(1, 2, 1)
# im1 = ax1.imshow(np.flipud(background_1p0), cmap=ListedColormap(colormap))
# ax1.set_title('background map (1.0 deg angular res)')
# ax2 = fig1.add_subplot(1, 2, 2, sharex=ax1, sharey=ax1)
# im2 = ax2.imshow(np.flipud(background_0p2), cmap=ListedColormap(colormap))
# ax2.set_title('background map (0.2 deg angular res)')
# im1.set_clim(0, np.mean(background_1p0.flatten()) + 2*np.std(background_1p0.flatten()))
# im2.set_clim(0, np.mean(background_0p2.flatten()) + 2*np.std(background_0p2.flatten()))
# plt.colorbar(im2, ax=(ax1, ax2), location='bottom')
#
# # SNR/intensity/background map
# fig1 = plt.figure(4)
# ax1 = fig1.add_subplot(1, 2, 1)
# im1 = ax1.imshow(np.flipud(total_intensity_1p0), cmap=ListedColormap(colormap))
# ax1.set_title('total intensity map (1.0 deg angular res)')
# ax2 = fig1.add_subplot(1, 2, 2, sharex=ax1, sharey=ax1)
# im2 = ax2.imshow(np.flipud(total_intensity_0p2), cmap=ListedColormap(colormap))
# ax2.set_title('total intensity map (0.2 deg angular res)')
# plt.colorbar(im2, ax=(ax1, ax2), location='bottom')


# SNR
# active_region = (100, 140, 190, 230)
# saved = np.copy(snr_standard[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_standard[:, :] = np.nan
# snr_standard[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(snr_bifurcated[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_bifurcated[:, :] = np.nan
# snr_bifurcated[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# SNR_1p0 = SNR_1p0[~np.isnan(SNR_1p0)]
# SNR_0p2 = SNR_0p2[~np.isnan(SNR_0p2)]

# fig2 = plt.figure(5)
# plt.hist(SNR_1p0.flatten(), bins=100, density=False, alpha=0.5)
# plt.hist(SNR_0p2.flatten(), bins=100, density=False, alpha=0.5)
# plt.legend(['1.0 deg angular res, Mean='+str(np.nanmean(SNR_1p0.flatten()))+', STD='+str(np.nanstd(SNR_1p0.flatten())),
#             '0.2 deg angular res, Mean='+str(np.nanmean(SNR_0p2.flatten()))+', STD='+str(np.nanstd(SNR_0p2.flatten()))])
# plt.title('SNR histograms')
# plt.xlabel('SNR')
# plt.ylabel('Count')

# Range
fig3 = plt.figure(6)
ax11 = fig3.add_subplot(1, 4, 1)
im11 = ax11.imshow(np.flipud(range_map_standard), cmap=ListedColormap(colormap))
ax11.set_title('Range map (1.0 deg angular res) - Do nothing')
ax12 = fig3.add_subplot(1, 4, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(range_map_mike), cmap=ListedColormap(colormap))
ax12.set_title('Range map (1.0 deg angular res) - Replacing bad with averaged good')
ax13 = fig3.add_subplot(1, 4, 3, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(range_map_thres), cmap=ListedColormap(colormap))
ax13.set_title('Range map (0.2 deg angular res) - Thresholding (z-scoring on SNR)')
ax14 = fig3.add_subplot(1, 4, 4, sharex=ax11, sharey=ax11)
im14 = ax14.imshow(np.flipud(range_map_02), cmap=ListedColormap(colormap))
ax14.set_title('Range map (0.2 deg angular res)')
plt.colorbar(im14, ax=(ax11, ax12, ax13, ax14), location='bottom')


# saved = np.copy(range_standard[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# range_standard[:, :] = np.nan
# range_standard[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(range_bifurcated[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# range_bifurcated[:, :] = np.nan
# range_bifurcated[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

# range_standard = range_standard[~np.isnan(range_standard)]
# range_bifurcated = range_bifurcated[~np.isnan(range_bifurcated)]

# _c = 299792458
# freq_indices = np.asarray([8,7])
# freq = 1.0e9 / (3 * freq_indices)  # Modulation frequencies in Hz.  [Low, High]
# coef_f0 = (_c / (2 * freq[0]))
# coef_f1 = (_c / (2 * freq[1]))
# gcf = 1e9 / (3 * freq_indices[0] * freq_indices[1])
# max_range = _c / (2 * gcf)
# M_vals_0 = np.arange(-1, int(max_range/coef_f0)+1, 1, dtype=np.int)
# M_vals_1 = np.arange(-1, int(max_range/coef_f1)+1, 1, dtype=np.int)
# M_range_0 = coef_f0 * M_vals_0
# M_range_1 = coef_f1 * M_vals_1
#
# fig4 = plt.figure(4)
# ax11 = fig4.add_subplot(1, 2, 1)
# im11 = ax11.hist(range_standard, bins=100)
# ax11.set_title('Range hist (standard LCM pattern)')
# ax11.set_xlabel('Range')
# ax11.set_ylabel('Count')
# ax11.set_xlim(np.amin(range_standard) - 1, np.amax(range_standard) + 1)
#
# ax12 = fig4.add_subplot(1, 2, 2, sharex=ax11, sharey=ax11)
# im12 = ax12.hist(range_bifurcated, bins=100)
# ax12.set_title('Range hist (bifurcated LCM pattern)')
# ax12.set_xlabel('Range')
# ax12.set_ylabel('Count')
# ax11.set_xlim(np.amin(range_bifurcated) - 1, np.amax(range_bifurcated) + 1)
#
# for idx,_ in enumerate(M_range_0):
#     ax11.axvline(x=M_range_0[idx], linestyle='--', color='r')
#     ax11.axvline(x=M_range_1[idx], linestyle='--', color='m')
#     ax12.axvline(x=M_range_0[idx], linestyle='--', color='r')
#     ax12.axvline(x=M_range_1[idx], linestyle='--', color='m')
#
# range_standard = range_standard.flatten()
# range_bifurcated = range_bifurcated.flatten()
# s_count = np.count_nonzero(np.where((range_standard > 0.99*6.12) & (range_standard < 1.01*6.12)))
# b_count = np.count_nonzero(np.where((range_bifurcated > 0.99*6.12) & (range_bifurcated < 1.01*6.12)))
# print("Standard: " + str(s_count) + " pixels representing " + str(s_count/len(range_standard.flatten())) + '%')
# print("Bifurcated: " + str(b_count) + " pixels representing " + str(b_count/len(range_bifurcated.flatten())) + '%')

plt.show()

# M = data["M"].flt
# SNR = np.mean(data["SNR_frame"].flt, axis=0)
# range_vals = data[output_range_array_name].flt
#
# M_list = []
# SNR_list = []
# for m in range(0, 7):
#     M_list.append(M == m)
#     SNR_list.append(SNR[M_list[-1]].flatten())
#
# import matplotlib.pyplot as plt
# plt.figure()
# for m in range(len(SNR_list)):
#     plt.hist(SNR_list[m], label='M = ' + str(m), bins=50)
#
# plt.ylabel('Count')
# plt.xlabel('SNR')
# plt.title('SNR histogram based on value of M for a fixed target')
# plt.legend()
# #plt.matshow(M)
# #plt.colorbar()
# plt.show()


