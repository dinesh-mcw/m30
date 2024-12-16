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
num_rois = 40                # Number of ROIs per frame
num_rows_full_frame = 222

data_gen_1p0 = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
device_1p0 = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)

num_frames = 1              # The device processes 1 frame at a time
data_viewer = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'multi_distances_measurements', '20220311_122827')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = '20220311_122827_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen_1p0.load_sensor_data_from_npy(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector_1p0 = data_gen_1p0.data["ROI_start_vector"]

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
           'convolution_kernel_y_size': 15,
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
phase_unwrapping_error_range_bracket = (device_1p0.dsp._c / (2*device_1p0.dsp.freq[1])) / 2

SNR_1p0 = []
SNR_1p0_mike = []
SNR_1p0_thres = []
SNR_0p2 = []
SNR_1p0_smoothed = []
SNR_1p0_mike_smoothed = []
SNR_1p0_thres_smoothed = []
SNR_0p2_smoothed = []

phase_unwrapping_errors_1p0_no_convolution = []
phase_unwrapping_errors_1p0 = []
phase_unwrapping_errors_1p0_mike = []
phase_unwrapping_errors_1p0_thres = []
phase_unwrapping_errors_0p2 = []

precision_1p0_no_convolution = []
precision_1p0 = []
precision_mike = []
precision_thres = []
precision_0p2 = []

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

# active_region_1x1 = (200, 270, 220, 360) # Use this one with "wall_tests_20220228" dataset
active_region_1x1 = (200, 280, 380, 460) # Use this one with "normal" dataset
active_region_2x2 = tuple(int(ti/2) for ti in active_region_1x1)
active_region_4x4 = tuple(int(ti/4) for ti in active_region_1x1)
active_region_5x5 = tuple(int(ti/5) for ti in active_region_1x1)

active_region = active_region_1x1

for frame_num in range(len(data_gen_1p0.data[input_data_name])):

    print('Do nothing - no convolution')
    configs['enable_convolution'] = False
    configs['correct_strips'] = False
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0, configs=configs)
    range_map_standard = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    range_map_standard_active = np.copy(range_map_standard)

    saved = np.copy(range_map_standard_active[active_region[0]:active_region[1], active_region[2]:active_region[3]])
    range_map_standard_active[:, :] = np.nan
    range_map_standard_active[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

    unwrap_errors_mask = np.abs(range_map_standard_active[~np.isnan(range_map_standard_active)] - 6.617894288788537) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / max(len(range_map_standard_active[~np.isnan(range_map_standard_active)]), 1)
    phase_unwrapping_errors_1p0_no_convolution.append(phase_unwrapping_error)
    device_1p0.clear(force_clear=True)

    print('Thresholding')
    configs['enable_convolution'] = True
    configs['correct_strips'] = False
    configs['SNR_voting_thres_enable'] = True
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0, configs=configs)
    range_map_thres = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    range_map_thres_active = np.copy(range_map_thres)
    RANGE_1p0_thres.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))
    SNR_thresholding_mask = np.copy(device_1p0.dsp.data["SNR_mask"].flt)

    saved = np.copy(SNR_thresholding_mask[active_region[0]:active_region[1], active_region[2]:active_region[3]])
    SNR_thresholding_mask[:, :] = True
    SNR_thresholding_mask[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

    saved = np.copy(range_map_thres_active[active_region[0]:active_region[1], active_region[2]:active_region[3]])
    range_map_thres_active[:, :] = np.nan
    range_map_thres_active[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

    range_map_thres_active[SNR_thresholding_mask] = np.nan

    snr_map_1p0_thres = np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0))
    snr_map_1p0_thres_smoothed = np.copy(np.mean(device_1p0.dsp.data["SNR_frame_smoothed"].flt, axis=0))
    snr_map_1p0_thres_active = np.copy(snr_map_1p0_thres)
    snr_map_1p0_thres_smoothed_active = np.copy(snr_map_1p0_thres_smoothed)

    snr_map_1p0_thres_active[SNR_thresholding_mask] = np.nan
    snr_map_1p0_thres_smoothed_active[SNR_thresholding_mask] = np.nan

    unwrap_errors_mask = np.abs(range_map_thres_active[~np.isnan(range_map_thres_active)] - 6.617894288788537) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / max(len(range_map_thres_active[~np.isnan(range_map_thres_active)]), 1)
    phase_unwrapping_errors_1p0_thres.append(phase_unwrapping_error)

    range_map_thres_active = range_map_thres_active[~np.isnan(range_map_thres_active)].flatten()
    precision_range_array = range_map_thres_active[np.invert(unwrap_errors_mask)].flatten()
    precision = 100 * np.std(precision_range_array) / 6.617894288788537
    precision_thres.append(precision)

    SNR_1p0_thres.append(snr_map_1p0_thres_active[~np.isnan(snr_map_1p0_thres_active)])
    SNR_1p0_thres_smoothed.append(snr_map_1p0_thres_smoothed_active[~np.isnan(snr_map_1p0_thres_smoothed_active)])
    # SNR_1p0_thres.append(snr_map_1p0_thres_active)
    # SNR_1p0_thres_smoothed.append(snr_map_1p0_thres_smoothed_active)
    device_1p0.clear(force_clear=True)

    print('Do nothing - except convolution')
    configs['correct_strips'] = False
    configs['SNR_voting_thres_enable'] = False
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0, configs=configs)
    range_map_standard = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    range_map_standard_active = np.copy(range_map_standard)
    RANGE_1p0.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))

    saved = np.copy(range_map_standard_active[active_region[0]:active_region[1], active_region[2]:active_region[3]])
    range_map_standard_active[:, :] = np.nan
    range_map_standard_active[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

    range_map_standard_active[SNR_thresholding_mask] = np.nan

    snr_map_1p0 = np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0))
    snr_map_1p0_smoothed = np.copy(np.mean(device_1p0.dsp.data["SNR_frame_smoothed"].flt, axis=0))
    snr_map_1p0_active = np.copy(snr_map_1p0)
    snr_map_1p0_smoothed_active = np.copy(snr_map_1p0_smoothed)

    snr_map_1p0_active[SNR_thresholding_mask] = np.nan
    snr_map_1p0_smoothed_active[SNR_thresholding_mask] = np.nan

    unwrap_errors_mask = np.abs(range_map_standard_active[~np.isnan(range_map_standard_active)] - 6.617894288788537) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / max(len(range_map_standard_active[~np.isnan(range_map_standard_active)]), 1)
    phase_unwrapping_errors_1p0.append(phase_unwrapping_error)

    range_map_standard_active = range_map_standard_active[~np.isnan(range_map_standard_active)].flatten()
    precision_range_array = range_map_standard_active[np.invert(unwrap_errors_mask)].flatten()
    precision = 100 * np.std(precision_range_array) / 6.617894288788537
    precision_1p0.append(precision)

    SNR_1p0.append(snr_map_1p0_active[~np.isnan(snr_map_1p0_active)])
    SNR_1p0_smoothed.append(snr_map_1p0_smoothed_active[~np.isnan(snr_map_1p0_smoothed_active)])
    # SNR_1p0.append(snr_map_1p0_active)
    # SNR_1p0_smoothed.append(snr_map_1p0_smoothed_active)
    signal_1p0.append(np.copy(np.mean(device_1p0.dsp.data["signal_frame"].flt, axis=0)))
    background_1p0.append(np.copy(np.mean(device_1p0.dsp.data["background_frame"].flt, axis=0)))
    total_intensity_1p0.append(np.copy(np.mean(np.sum(device_1p0.dsp.data["combined_data_frame"].flt, axis=0), axis=0)))
    device_1p0.clear(force_clear=True)

    print('Mikes method')
    configs['correct_strips'] = True
    input_data = data_gen_1p0.data[input_data_name][frame_num]
    output_range_array_name = device_1p0(input_data=input_data, roi_start_vector=start_vector_1p0, configs=configs)
    range_map_mike = np.copy(device_1p0.dsp.data[output_range_array_name].flt)
    range_map_mike_active = np.copy(range_map_mike)
    RANGE_1p0_mike.append(np.copy(device_1p0.dsp.data[output_range_array_name].flt))

    saved = np.copy(range_map_mike_active[active_region[0]:active_region[1], active_region[2]:active_region[3]])
    range_map_mike_active[:, :] = np.nan
    range_map_mike_active[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

    range_map_mike_active[SNR_thresholding_mask] = np.nan

    snr_map_1p0_mike = np.copy(np.mean(device_1p0.dsp.data["SNR_frame"].flt, axis=0))
    snr_map_1p0_mike_smoothed = np.copy(np.mean(device_1p0.dsp.data["SNR_frame_smoothed"].flt, axis=0))
    snr_map_1p0_mike_active = np.copy(snr_map_1p0_mike)
    snr_map_1p0_mike_smoothed_active = np.copy(snr_map_1p0_mike_smoothed)

    snr_map_1p0_mike_active[SNR_thresholding_mask] = np.nan
    snr_map_1p0_mike_smoothed_active[SNR_thresholding_mask] = np.nan

    unwrap_errors_mask = np.abs(range_map_mike_active[~np.isnan(range_map_mike_active)] - 6.617894288788537) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / max(len(range_map_mike_active[~np.isnan(range_map_mike_active)]), 1)
    phase_unwrapping_errors_1p0_mike.append(phase_unwrapping_error)

    range_map_mike_active = range_map_mike_active[~np.isnan(range_map_mike_active)].flatten()
    precision_range_array = range_map_mike_active[np.invert(unwrap_errors_mask)].flatten()
    precision = 100 * np.std(precision_range_array) / 6.617894288788537
    precision_mike.append(precision)

    SNR_1p0_mike.append(snr_map_1p0_mike_active[~np.isnan(snr_map_1p0_mike_active)])
    SNR_1p0_mike_smoothed.append(snr_map_1p0_mike_smoothed_active[~np.isnan(snr_map_1p0_mike_smoothed_active)])
    # SNR_1p0_mike.append(snr_map_1p0_mike)
    # SNR_1p0_mike_smoothed.append(snr_map_1p0_mike_smoothed)
    device_1p0.clear(force_clear=True)



import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap

colormap = np.flipud(np.load(os.path.join(os.path.join('..', 'colormaps'), "turbo_colormap_data.npy")))

SNR_1p0 = np.mean(np.asarray(SNR_1p0), axis=0) # take the mean across all frames
SNR_1p0_mike = np.mean(np.asarray(SNR_1p0_mike), axis=0) # take the mean across all frames
SNR_1p0_thres = np.mean(np.asarray(SNR_1p0_thres), axis=0) # take the mean across all frames
SNR_1p0_smoothed = np.mean(np.asarray(SNR_1p0_smoothed), axis=0) # take the mean across all frames
SNR_1p0_mike_smoothed = np.mean(np.asarray(SNR_1p0_mike_smoothed), axis=0) # take the mean across all frames
SNR_1p0_thres_smoothed = np.mean(np.asarray(SNR_1p0_thres_smoothed), axis=0) # take the mean across all frames

phase_unwrapping_errors_1p0_no_convolution = np.mean(np.asarray(phase_unwrapping_errors_1p0_no_convolution), axis=0) # take the mean across all fra
phase_unwrapping_errors_1p0 = np.mean(np.asarray(phase_unwrapping_errors_1p0), axis=0) # take the mean across all frames
phase_unwrapping_errors_1p0_mike = np.mean(np.asarray(phase_unwrapping_errors_1p0_mike), axis=0) # take the mean across all frames
phase_unwrapping_errors_1p0_thres = np.mean(np.asarray(phase_unwrapping_errors_1p0_thres), axis=0) # take the mean across all frames
# phase_unwrapping_errors_0p2 = np.mean(np.asarray(phase_unwrapping_errors_0p2), axis=0) # take the mean across all frames

precision_1p0 = np.mean(np.asarray(precision_1p0), axis=0) # take the mean across all fra
precision_mike = np.mean(np.asarray(precision_mike), axis=0) # take the mean across all frames
precision_thres = np.mean(np.asarray(precision_thres), axis=0) # take the mean across all frames
# precision_0p2 = np.mean(np.asarray(precision_0p2), axis=0) # take the mean across all frames

RANGE_1p0 = np.mean(np.asarray(RANGE_1p0), axis=0) # take the mean across all frames
RANGE_1p0_mike = np.mean(np.asarray(RANGE_1p0_mike), axis=0) # take the mean across all frames
RANGE_1p0_thres = np.mean(np.asarray(RANGE_1p0_thres), axis=0) # take the mean across all frames

# indices_to_mask = np.logical_or(SNR_1p0 < 1, SNR_0p2 < 1)
# SNR_1p0[indices_to_mask] = np.nan
# SNR_0p2[indices_to_mask] = np.nan

# SNR
# saved = np.copy(SNR_1p0[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0[:, :] = np.nan
# SNR_1p0[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_1p0_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0_smoothed[:, :] = np.nan
# SNR_1p0_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_1p0_mike[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0_mike[:, :] = np.nan
# SNR_1p0_mike[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_1p0_mike_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0_mike_smoothed[:, :] = np.nan
# SNR_1p0_mike_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_1p0_thres[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0_thres[:, :] = np.nan
# SNR_1p0_thres[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_1p0_thres_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_1p0_thres_smoothed[:, :] = np.nan
# SNR_1p0_thres_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

# saved = np.copy(SNR_0p2[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_0p2[:, :] = np.nan
# SNR_0p2[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_0p2_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_0p2_smoothed[:, :] = np.nan
# SNR_0p2_smoothed[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

SNR_1p0 = SNR_1p0[~np.isnan(SNR_1p0)]
SNR_1p0_mike = SNR_1p0_mike[~np.isnan(SNR_1p0_mike)]
SNR_1p0_thres = SNR_1p0_thres[~np.isnan(SNR_1p0_thres)]
# SNR_0p2 = SNR_0p2[~np.isnan(SNR_0p2)]
SNR_1p0_smoothed = SNR_1p0_smoothed[~np.isnan(SNR_1p0_smoothed)]
SNR_1p0_mike_smoothed = SNR_1p0_mike_smoothed[~np.isnan(SNR_1p0_mike_smoothed)]
SNR_1p0_thres_smoothed = SNR_1p0_thres_smoothed[~np.isnan(SNR_1p0_thres_smoothed)]
# SNR_0p2_smoothed = SNR_0p2_smoothed[~np.isnan(SNR_0p2_smoothed)]

# # fig4 = plt.figure(4)
# x = ['with convolution - nothing else', 'replacing bad pixels by averaged good', 'SNR thresholding']
# heights = [phase_unwrapping_errors_1p0,
#            phase_unwrapping_errors_1p0_mike,
#            phase_unwrapping_errors_1p0_thres]
# improvement = np.asarray(heights[1] - heights[2:])

# # Phase unwrap error graphs
# plt.bar(x, heights)
# plt.ylabel('Phase unwrap errors (%)')
# plt.title('Phase unwrap errors under different methods')

# # Precision bar graph
# x = ['with convolution - nothing else', 'replacing bad pixels by averaged good', 'SNR thresholding']
# plt.figure(8)
# plt.bar(x, [precision_1p0, precision_mike, precision_thres])
# plt.ylabel('Precision [%]')
# plt.title('Precision for high SNR pixels on the wall at 6 meters')

# # Phase unwrap error graphs
# max_hist_range = max(np.nanmax(SNR_1p0.flatten()), np.nanmax(SNR_1p0_smoothed.flatten()),
#                 np.nanmax(SNR_1p0_mike.flatten()), np.nanmax(SNR_1p0_mike_smoothed.flatten()),
#                 np.nanmax(SNR_1p0_thres.flatten()), np.nanmax(SNR_1p0_thres_smoothed.flatten()))
#                 #np.nanmax(SNR_0p2.flatten()), np.nanmax(SNR_0p2_smoothed.flatten()))
# fig5 = plt.figure(5)
# ax1 = fig5.add_subplot(3, 1, 1)
# ax1.hist(SNR_1p0.flatten(), bins=100, density=False, alpha=0.5, label='raw SNR, mean: ' + str(np.nanmean(SNR_1p0.flatten())))
# ax1.hist(SNR_1p0_smoothed.flatten(), bins=100, density=False, alpha=0.5, label='smoothed SNR, mean: ' + str(np.nanmean(SNR_1p0_smoothed.flatten())))
# ax1.set_title('Do nothing - Convolution only')
# ax1.set_ylabel('Count')
# ax1.set_xlim(0, max_hist_range)
# ax1.legend()
# #plt.xlim(0, 0.5)
# ax2 = fig5.add_subplot(3, 1, 2, sharex=ax1, sharey=ax1)
# ax2.hist(SNR_1p0_mike.flatten(), bins=100, density=False, alpha=0.5, label='raw SNR, mean: ' + str(np.nanmean(SNR_1p0_mike.flatten())))
# ax2.hist(SNR_1p0_mike_smoothed.flatten(), bins=100, density=False, alpha=0.5, label='smoothed SNR, mean: ' + str(np.nanmean(SNR_1p0_mike_smoothed.flatten())))
# ax2.set_title('Replacing bad pixels with averaged good')
# #plt.xlim(0, 0.5)
# ax2.set_ylabel('Count')
# ax2.set_xlim(0, max_hist_range)
# ax2.legend()
# ax3 = fig5.add_subplot(3, 1, 3, sharex=ax1, sharey=ax1)
# ax3.hist(SNR_1p0_thres.flatten(), bins=100, density=False, alpha=0.5, label='raw SNR, mean: ' + str(np.nanmean(SNR_1p0_thres.flatten())))
# ax3.hist(SNR_1p0_thres_smoothed.flatten(), bins=100, density=False, alpha=0.5, label='smoothed SNR, mean: ' + str(np.nanmean(SNR_1p0_thres_smoothed.flatten())))
# ax3.set_title('SNR thresholding')
# #plt.xlim(0, 0.5)
# ax3.set_ylabel('Count')
# ax3.set_xlim(0, max_hist_range)
# ax3.legend()
# ax4 = fig5.add_subplot(4, 1, 4)
# ax4.hist(SNR_0p2.flatten(), bins=100, density=False, alpha=0.5, label='raw SNR, mean: ' + str(np.nanmean(SNR_0p2.flatten())))
# ax4.hist(SNR_0p2_smoothed.flatten(), bins=100, density=False, alpha=0.5, label='smoothed SNR, mean: ' + str(np.nanmean(SNR_0p2_smoothed.flatten())))
# ax4.set_title('0.2 deg angular resolution')
# #plt.xlim(0, 0.5)
# ax4.set_xlabel('SNR')
# ax4.set_ylabel('Count')
# ax4.set_xlim(0, max_hist_range)
# ax4.legend()

# Range
fig3 = plt.figure(6)
ax11 = fig3.add_subplot(1, 3, 1)
im11 = ax11.imshow(np.flipud(range_map_standard), cmap=ListedColormap(colormap))
ax11.set_title('Range map (1.0 deg angular res) - Do nothing')
ax12 = fig3.add_subplot(1, 3, 2, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(range_map_mike), cmap=ListedColormap(colormap))
ax12.set_title('Range map (1.0 deg angular res) - Replacing bad with averaged good')
ax13 = fig3.add_subplot(1, 3, 3, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(range_map_thres), cmap=ListedColormap(colormap))
ax13.set_title('Range map (1.0 deg angular res) - Thresholding')
plt.colorbar(im13, ax=(ax11, ax12, ax13), location='bottom')

# saved = np.copy(snr_map_1p0[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_map_1p0[:, :] = np.nan
# snr_map_1p0[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(snr_map_1p0_mike[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_map_1p0_mike[:, :] = np.nan
# snr_map_1p0_mike[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(snr_map_1p0_thres[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_map_1p0_thres[:, :] = np.nan
# snr_map_1p0_thres[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(SNR_thresholding_mask[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# SNR_thresholding_mask[:, :] = np.nan
# SNR_thresholding_mask[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved
#
# saved = np.copy(snr_map_0p2[active_region[0]:active_region[1], active_region[2]:active_region[3]])
# snr_map_0p2[:, :] = np.nan
# snr_map_0p2[active_region[0]:active_region[1], active_region[2]:active_region[3]] = saved

# SNR
fig4 = plt.figure(7)

ax11 = fig4.add_subplot(3, 2, 1)
im11 = ax11.imshow(np.flipud(snr_map_1p0), cmap=ListedColormap(colormap))
ax11.set_title('SNR map (1.0 deg) - Do nothing')
ax21 = fig4.add_subplot(3, 2, 2, sharex=ax11, sharey=ax11)
im21 = ax21.imshow(np.flipud(snr_map_1p0_smoothed), cmap=ListedColormap(colormap))
ax21.set_title('SNR map (1.0 deg) - Do nothing - Smoothed')

ax12 = fig4.add_subplot(3, 2, 3, sharex=ax11, sharey=ax11)
im12 = ax12.imshow(np.flipud(snr_map_1p0_mike), cmap=ListedColormap(colormap))
ax12.set_title('SNR map (1.0 deg) - Replacing bad with averaged good')
ax22 = fig4.add_subplot(3, 2, 4, sharex=ax11, sharey=ax11)
im22 = ax22.imshow(np.flipud(snr_map_1p0_mike_smoothed), cmap=ListedColormap(colormap))
ax22.set_title('SNR map (1.0 deg) - Replacing bad with averaged good - Smoothed')

ax13 = fig4.add_subplot(3, 2, 5, sharex=ax11, sharey=ax11)
im13 = ax13.imshow(np.flipud(snr_map_1p0_thres), cmap=ListedColormap(colormap))
ax13.set_title('SNR map (1.0 deg) - Thresholding')
ax23 = fig4.add_subplot(3, 2, 6, sharex=ax11, sharey=ax11)
im23 = ax23.imshow(np.flipud(snr_map_1p0_thres_smoothed), cmap=ListedColormap(colormap))
ax23.set_title('SNR map (1.0 deg) - Thresholding - Smoothed')
# ax14 = fig4.add_subplot(1, 4, 4, sharex=ax11, sharey=ax11)
# im14 = ax14.imshow(np.flipud(snr_map_0p2), cmap=ListedColormap(colormap))
# ax14.set_title('SNR map (0.2 deg)')
# plt.colorbar(im23, ax=(ax11, ax21, ax12, ax22, ax13, ax23), location='bottom')


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


