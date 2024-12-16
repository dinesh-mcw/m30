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
num_rois = 24                # Number of ROIs per frame
num_rows_full_frame = 480
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
path = os.path.join('..', '..', '..', 'rm_generated_datasets',
                    'rm_dataset_1_to_25_meters_physical_SNR_1mstep_high_solar')
use_old_metadata_format = False
fov_num_to_use = 0
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_rm_data(path, step=1)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs_stripe_mode = {'perform_tap_add': perform_tap_add,
                       'correct_strips': False,
                       'weighted_sum_en': True, # TODO: False is not yet supported (need to define what it means for the weighting to be disabled)
                       'weight_by': 'illumination',
                       'weight_by_SNR_max_points': 20,
                       'weight_by_SNR_auto_mode': 'akaike',
                       'weight_by_matched_filter_num_points': 20,
                       'weight_by_matched_filter_mean': 10,
                       'weight_by_matched_filter_std': 1.6,
                       'weight_by_illumination_num_points': 3,
                       'binning': (1, 1),
                       'SNR_voting_combined': True,
                       'SNR_voting_thres_enable': False,
                       'temporal_boxcar_length': 1,  # Set this to 1 to disable it
                       'enable_convolution': True,
                       'enable_phase_correction': True,
                       'use_1d_convolutions': False,
                       'convolution_kernel_x_size': 7,
                       'convolution_kernel_y_size': 1,
                       'M_filter': True,
                       'M_filter_loc': 0,
                       'M_filter_type': 3,
                       'M_filter_size': (1, 3),
                       'M_filter_shape': None,
                       'range_edge_filter_en': False,
                       'range_median_filter_en': True,
                       'range_median_filter_size': [1, 5],
                       'range_median_filter_shape': '+',
                       'NN_enable': False,
                       'NN_filter_level': 5,
                       'NN_min_neighbors': 6,
                       'NN_patch_size': 3,
                       'NN_range_tolerance': 0.7,
                       'SNR_threshold_enable': False,
                       'SNR_threshold': 0.1,
                       'pixel_mask_path': os.path.join('..', '..', '..', 'pixel_mask_SN88.bin'),
                       'invalid_pixel_mask_enable': True}

configs_stripe_mode_1x2 = configs_stripe_mode.copy()
configs_stripe_mode_1x2['binning'] = (1, 2)
configs_stripe_mode_1x2['convolution_kernel_x_size'] = 5
configs_stripe_mode_1x4 = configs_stripe_mode.copy()
configs_stripe_mode_1x4['binning'] = (1, 4)
configs_stripe_mode_1x4['convolution_kernel_x_size'] = 3

configs_normal_mode = configs_stripe_mode.copy()
configs_normal_mode['binning'] = (1, 1)
configs_normal_mode['convolution_kernel_x_size'] = 7
configs_normal_mode['convolution_kernel_y_size'] = 15
configs_normal_mode['M_filter_type'] = 8
configs_normal_mode['M_filter_size'] = (3, 3)
configs_normal_mode['range_median_filter_size'] = [5, 5]
configs_normal_mode_2x2 = configs_normal_mode.copy()
configs_normal_mode_2x2['binning'] = (2, 2)
configs_normal_mode_2x2['convolution_kernel_x_size'] = 5
configs_normal_mode_2x2['convolution_kernel_y_size'] = 7
configs_normal_mode_4x4 = configs_normal_mode.copy()
configs_normal_mode_4x4['binning'] = (4, 4)
configs_normal_mode_4x4['convolution_kernel_x_size'] = 3
configs_normal_mode_4x4['convolution_kernel_y_size'] = 5

phase_unwrapping_error_range_bracket = (device.dsp._c / (2*device.dsp.freq[1])) / 2
ground_truth = []

# Stripe mode
stripe_mode_range_full = []
stripe_mode_range = []
stripe_mode_range_full_1x2 = []
stripe_mode_range_1x2 = []
stripe_mode_range_full_1x4 = []
stripe_mode_range_1x4 = []

normal_mode_range_full = []
normal_mode_range = []
normal_mode_range_full_2x2 = []
normal_mode_range_2x2 = []
normal_mode_range_full_4x4 = []
normal_mode_range_4x4 = []

# stripe_mode_SNR = []
# normal_mode_SNR = []
for dataset_idx in range(len(data_gen.data[input_data_name])):

    # if dataset_idx > 1:
    #     break

    print('Processing target distance of ' + str(data_gen.data['target_distance'][dataset_idx]))
    ground_truth.append(data_gen.data['target_distance'][dataset_idx])
    input_data = data_gen.data[input_data_name][dataset_idx]

    # Stripe mode processing
    output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode,
                                                         start_vector=start_vector)
    stripe_mode_range_full.append(device.dsp.data[output_range_array_name].flt)
    stripe_mode_range.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # stripe_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

    output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_1x2,
                                                         start_vector=start_vector)
    stripe_mode_range_full_1x2.append(device.dsp.data[output_range_array_name].flt)
    stripe_mode_range_1x2.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # stripe_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

    output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_1x4,
                                                         start_vector=start_vector)
    stripe_mode_range_full_1x4.append(device.dsp.data[output_range_array_name].flt)
    stripe_mode_range_1x4.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # stripe_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

    # Normal mode processing
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs_normal_mode)
    normal_mode_range_full.append(device.dsp.data[output_range_array_name].flt)
    normal_mode_range.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # normal_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs_normal_mode_2x2)
    normal_mode_range_full_2x2.append(device.dsp.data[output_range_array_name].flt)
    normal_mode_range_2x2.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # normal_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs_normal_mode_4x4)
    normal_mode_range_full_4x4.append(device.dsp.data[output_range_array_name].flt)
    normal_mode_range_4x4.append(device.dsp.data[output_range_array_name].flt[~np.isnan(device.dsp.data[output_range_array_name].flt)])
    # normal_mode_SNR.append(device.dsp.data['SNR_frame'].flt[:, ~np.isnan(device.dsp.data[output_range_array_name].flt)])
    device.clear(force_clear=True)

ground_truth = np.asarray(ground_truth)
stripe_mode_range_full = np.asarray(stripe_mode_range_full)
stripe_mode_range = np.asarray(stripe_mode_range)
stripe_mode_range_full_1x2 = np.asarray(stripe_mode_range_full_1x2)
stripe_mode_range_1x2 = np.asarray(stripe_mode_range_1x2)
stripe_mode_range_full_1x4 = np.asarray(stripe_mode_range_full_1x4)
stripe_mode_range_1x4 = np.asarray(stripe_mode_range_1x4)

normal_mode_range_full = np.asarray(normal_mode_range_full)
normal_mode_range = np.asarray(normal_mode_range)
normal_mode_range_full_2x2 = np.asarray(normal_mode_range_full_2x2)
normal_mode_range_2x2 = np.asarray(normal_mode_range_2x2)
normal_mode_range_full_4x4 = np.asarray(normal_mode_range_full_4x4)
normal_mode_range_4x4 = np.asarray(normal_mode_range_4x4)
# stripe_mode_SNR = np.asarray(stripe_mode_SNR)
# normal_mode_SNR = np.asarray(normal_mode_SNR)

tp_stripe = []
fp_stripe = []
stripe_mean_range = []
tp_stripe_1x2 = []
fp_stripe_1x2 = []
stripe_mean_range_1x2 = []
tp_stripe_1x4 = []
fp_stripe_1x4 = []
stripe_mean_range_1x4 = []

tp_normal = []
fp_normal = []
normal_mean_range = []
tp_normal_2x2 = []
fp_normal_2x2 = []
normal_mean_range_2x2 = []
tp_normal_4x4 = []
fp_normal_4x4 = []
normal_mean_range_4x4 = []

# stripe_mean_SNR = []
# stripe_std_SNR = []
# normal_mean_SNR = []
# normal_std_SNR = []

# Remove PUEs from range
for idx in range(len(ground_truth)):
    # Stripe mode
    unwrap_errors_mask = np.abs(stripe_mode_range[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(stripe_mode_range[idx, np.nonzero(stripe_mode_range[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    stripe_mean_range.append(np.mean(stripe_mode_range[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_stripe.append(100 * tp / len(stripe_mode_range[0]))
    fp_stripe.append(100 * fp / len(stripe_mode_range[0]))

    unwrap_errors_mask = np.abs(stripe_mode_range_1x2[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(stripe_mode_range_1x2[idx, np.nonzero(stripe_mode_range_1x2[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    stripe_mean_range.append(np.mean(stripe_mode_range_1x2[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_stripe_1x2.append(100 * tp / len(stripe_mode_range_1x2[0]))
    fp_stripe_1x2.append(100 * fp / len(stripe_mode_range_1x2[0]))

    unwrap_errors_mask = np.abs(stripe_mode_range_1x4[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(stripe_mode_range_1x4[idx, np.nonzero(stripe_mode_range_1x4[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    stripe_mean_range.append(np.mean(stripe_mode_range_1x4[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_stripe_1x4.append(100 * tp / len(stripe_mode_range_1x4[0]))
    fp_stripe_1x4.append(100 * fp / len(stripe_mode_range_1x4[0]))

    # Normal mode
    unwrap_errors_mask = np.abs(normal_mode_range[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(normal_mode_range[idx, np.nonzero(normal_mode_range[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    normal_mean_range.append(np.mean(normal_mode_range[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_normal.append(100 * tp / len(normal_mode_range[0]))
    fp_normal.append(100 * fp / len(normal_mode_range[0]))

    unwrap_errors_mask = np.abs(normal_mode_range_2x2[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(normal_mode_range_2x2[idx, np.nonzero(normal_mode_range_2x2[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    normal_mean_range.append(np.mean(normal_mode_range_2x2[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_normal_2x2.append(100 * tp / len(normal_mode_range_2x2[0]))
    fp_normal_2x2.append(100 * fp / len(normal_mode_range_2x2[0]))

    unwrap_errors_mask = np.abs(normal_mode_range_4x4[idx, :] - ground_truth[idx]) < phase_unwrapping_error_range_bracket
    fp_mask = np.abs(normal_mode_range_4x4[idx, np.nonzero(normal_mode_range_4x4[idx, :])] - ground_truth[idx]) > phase_unwrapping_error_range_bracket
    normal_mean_range.append(np.mean(normal_mode_range_4x4[idx, unwrap_errors_mask]))
    tp = np.count_nonzero(unwrap_errors_mask)
    fp = np.count_nonzero(fp_mask)
    tp_normal_4x4.append(100 * tp / len(normal_mode_range_4x4[0]))
    fp_normal_4x4.append(100 * fp / len(normal_mode_range_4x4[0]))

tp_stripe = np.asarray(tp_stripe)
fp_stripe = np.asarray(fp_stripe)
stripe_mean_range = np.asarray(stripe_mean_range)
tp_stripe_1x2 = np.asarray(tp_stripe_1x2)
fp_stripe_1x2 = np.asarray(fp_stripe_1x2)
stripe_mean_range_1x2 = np.asarray(stripe_mean_range_1x2)
tp_stripe_1x4 = np.asarray(tp_stripe_1x4)
fp_stripe_1x4 = np.asarray(fp_stripe_1x4)
stripe_mean_range_1x4 = np.asarray(stripe_mean_range_1x4)

tp_normal = np.asarray(tp_normal)
fp_normal = np.asarray(fp_normal)
normal_mean_range = np.asarray(normal_mean_range)
tp_normal_2x2 = np.asarray(tp_normal_2x2)
fp_normal_2x2 = np.asarray(fp_normal_2x2)
normal_mean_range_2x2 = np.asarray(normal_mean_range_2x2)
tp_normal_4x4 = np.asarray(tp_normal_4x4)
fp_normal_4x4 = np.asarray(fp_normal_4x4)
normal_mean_range_4x4 = np.asarray(normal_mean_range_4x4)

# stripe_mean_SNR = np.asarray(stripe_mean_SNR)
# normal_mean_SNR = np.asarray(normal_mean_SNR)
# stripe_std_SNR = np.asarray(stripe_std_SNR)
# normal_std_SNR = np.asarray(normal_std_SNR)

import matplotlib.pyplot as plt

# Dump range maps to disk
for i in range(stripe_mode_range_full_1x2.shape[0]):
    dump_path = os.path.join('..', 'output')
    filename = 'stripe_mode_' + str(ground_truth[i]) + '_m.png'
    fig = plt.figure(figsize=(20, 10))
    plt.subplot(1, 2, 1)
    plt.imshow(stripe_mode_range_full_1x2[i], cmap='jet', vmin=0, vmax=np.max(ground_truth))
    plt.title('Stripe range - range: ' + str(ground_truth[i]) + ' m')
    plt.subplot(1, 2, 2)
    plt.imshow(normal_mode_range_full_2x2[i], cmap='jet', vmin=0, vmax=np.max(ground_truth))
    plt.title('Normal range - range: ' + str(ground_truth[i]) + ' m')
    plt.savefig(os.path.join(dump_path, filename))
    plt.close(fig)


fig, ax1 = plt.subplots()
ax2 = ax1.twinx()

ax1.plot(ground_truth, tp_normal, label='TP normal - 1x1 bin')
ax1.plot(ground_truth, tp_normal_2x2, label='TP normal - 2x2 bin')
ax1.plot(ground_truth, tp_normal_4x4, label='TP normal - 4x4 bin')
ax1.plot(ground_truth, tp_stripe, label='TP stripe - 20x1 bin')
ax1.plot(ground_truth, tp_stripe_1x2, label='TP stripe - 20x2 bin')
ax1.plot(ground_truth, tp_stripe_1x4, label='TP stripe - 20x4 bin')

ax2.plot(ground_truth, fp_normal, '--', label='FP normal - 1x1 bin')
ax2.plot(ground_truth, fp_normal_2x2, '--', label='FP normal - 2x2 bin')
ax2.plot(ground_truth, fp_normal_4x4, '--', label='FP normal - 4x4 bin')
ax2.plot(ground_truth, fp_stripe, '--', label='FP stripe - 20x1 bin')
ax2.plot(ground_truth, fp_stripe_1x2, '--', label='FP stripe - 20x2 bin')
ax2.plot(ground_truth, fp_stripe_1x4, '--', label='FP stripe - 20x4 bin')

# ask matplotlib for the plotted objects and their labels
lines, labels = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax2.legend(lines + lines2, labels + labels2, loc=0)

ax1.set_xlabel('Range [m]')
ax1.set_ylabel('TP / All pixels')
ax1.set_ylim([-1, 101])
ax2.set_ylim([-1, 101])
ax2.set_ylabel('FP / All pixels')
ax1.set_title('Comparing range performances for normal and stripe mode')
# ax1.legend(loc=1)
# ax2.legend(loc=0)

# plt.figure()
# plt.errorbar(ground_truth, stripe_mean_SNR, yerr=stripe_std_SNR, lolims=True, uplims=True, label='stripe mode AVG snr')
# plt.errorbar(ground_truth, normal_mean_SNR, yerr=normal_std_SNR, lolims=True, uplims=True, label='normal mode AVG snr')
# plt.xlabel('Range [m]')
# plt.ylabel('AVG SNR')
# plt.legend()

plt.show()

