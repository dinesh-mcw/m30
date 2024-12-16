##
import numpy as np
import os
import copy

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

"""
    Simple testbench to use M20-GPixel with generated/imported data and visualize the depth map at the output
"""


# -----------------------------------------------------------------------------------------
# 1: Import input data
# General params
num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 24                # Number of ROIs per frame
num_rows_full_frame = 480
num_frames = 1              # The device processes 1 frame at a time

data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

# -----------------------------------------------------------------------------------------
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
# -----------------------------------------------------------------------------------------

# -----------------------------------------------------------------------------------------
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_dataset_1_to_25_meters_physical_SNR_1mstep_high_solar')
use_old_metadata_format = False
fov_num_to_use = 0
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_rm_data(path, step=1)

# -----------------------------------------------------------------------------------------
start_vector = data_gen.data["ROI_start_vector"]
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                           num_rois, num_rows_full_frame)

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'binning': (2, 2),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': False,
           'enable_phase_correction': False,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 5,
           'convolution_kernel_y_size': 7,
           'M_filter': False,
           'M_filter_type': 3,
           'M_median_filter_size': 15,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 2,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0.5,
           'SNR_threshold_invalid_val': np.nan
           }

# High frequency
phase_unwrapping_error_range_bracket = (device.dsp._c / (2*device.dsp.freq[1])) / 2

# No convolution
print('No convolution')
snr_mean = []
snr_std = []
distances = []
output_error_no_conv = []
precision_no_conv = []
phase_unwrapping_error_no_conv = []
for dataset_idx in range(len(data_gen.data[input_data_name])):
    print('Processing target at distance: ' + str(data_gen.data['target_distance'][dataset_idx]))
    input_data = data_gen.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)

    range_array = device.dsp.data[output_range_array_name].flt.flatten() # constant range array
    range_array = range_array[~np.isnan(range_array)]
    # snr_mean.append(np.mean(device.dsp.data["SNR_frame"].flt.flatten()))
    # snr_std.append(np.std(device.dsp.data["SNR_frame"].flt.flatten()))

    # data_viewer.assign_dict(device.dsp.data)
    # data_viewer.plot_range_histogram(range_array_name=output_range_array_name, superimpose_M_values=True,
    #                                  target_range=data_gen.data['target_distance'][dataset_idx],
    #                                  dump_path=os.path.join('..', 'output', 'no_convolution'),
    #                                  filename=os.path.join('hist_'+str(dataset_idx)),
    #                                  save_figure=True, show_figure=False)

    error = np.average(np.abs(np.clip(range_array, a_min=0, a_max=device.dsp.max_range) - data_gen.data['target_distance'][dataset_idx])/device.dsp.max_range)
    unwrap_errors_mask = np.abs(range_array - data_gen.data['target_distance'][dataset_idx]) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / len(range_array)
    precision_range_array = range_array[np.invert(unwrap_errors_mask)].flatten()
    precision = 100 * np.std(precision_range_array) / data_gen.data['target_distance'][dataset_idx]
    distances.append(data_gen.data['target_distance'][dataset_idx])
    output_error_no_conv.append(error)
    precision_no_conv.append(precision)
    phase_unwrapping_error_no_conv.append(phase_unwrapping_error)
    device.clear(force_clear=True)

# # Enable convolution, no correct_phase
# print('With convolution')
# configs['enable_convolution'] = True
# output_error_w_conv_no_correct_phase = []
# for dataset_idx in range(len(data_gen.data[input_data_name])):
#     print('Processing target at distance: ' + str(data_gen.data['target_distance'][dataset_idx]))
#     input_data = data_gen.data[input_data_name][dataset_idx]
#     output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
#
#     range_array = device.dsp.data[output_range_array_name].flt.flatten() # constant range array
#     range_array = range_array[~np.isnan(range_array)]
#
#     # data_viewer.assign_dict(device.dsp.data)
#     # data_viewer.plot_range_histogram(range_array_name=output_range_array_name, superimpose_M_values=True,
#     #                                  target_range=data_gen.data['target_distance'][dataset_idx],
#     #                                  dump_path=os.path.join('..', 'output', 'with_convolution'),
#     #                                  filename=os.path.join('hist_'+str(dataset_idx)),
#     #                                  save_figure=True, show_figure=False)
#
#     error = np.average(np.abs(np.clip(range_array, a_min=0, a_max=device.dsp.max_range) - data_gen.data['target_distance'][dataset_idx])/device.dsp.max_range)
#     output_error_w_conv_no_correct_phase.append(error)
#     device.clear(force_clear=True)

# Enable convolution and correct_phase
print('With convolution, w correct phase')
configs['enable_convolution'] = True
configs['enable_phase_correction'] = True
output_error_w_conv_w_correct_phase = []
precision_w_conv_w_correct_phase = []
phase_unwrapping_error_w_conv_w_correct_phase = []
for dataset_idx in range(len(data_gen.data[input_data_name])):
    print('Processing target at distance: ' + str(data_gen.data['target_distance'][dataset_idx]))
    input_data = data_gen.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)

    range_array = device.dsp.data[output_range_array_name].flt.flatten() # constant range array
    range_array = range_array[~np.isnan(range_array)]

    # data_viewer.assign_dict(device.dsp.data)
    # data_viewer.plot_range_histogram(range_array_name=output_range_array_name, superimpose_M_values=True,
    #                                  target_range=data_gen.data['target_distance'][dataset_idx],
    #                                  dump_path=os.path.join('..', 'output', 'with_convolution_with_correction'),
    #                                  filename=os.path.join('hist_'+str(dataset_idx)),
    #                                  save_figure=True, show_figure=False)

    error = np.average(np.abs(np.clip(range_array, a_min=0, a_max=device.dsp.max_range) - data_gen.data['target_distance'][dataset_idx])/device.dsp.max_range)
    unwrap_errors_mask = np.abs(range_array - data_gen.data['target_distance'][dataset_idx]) > phase_unwrapping_error_range_bracket
    phase_unwrapping_error = 100*np.count_nonzero(unwrap_errors_mask) / len(range_array)
    precision_range_array = range_array[np.invert(unwrap_errors_mask)].flatten()
    precision = 100 * np.std(precision_range_array) / data_gen.data['target_distance'][dataset_idx]
    output_error_w_conv_w_correct_phase.append(error)
    precision_w_conv_w_correct_phase.append(precision)
    phase_unwrapping_error_w_conv_w_correct_phase.append(phase_unwrapping_error)
    device.clear(force_clear=True)

# # Enable convolution and correct_phase
# print('With convolution, w correct phase, zero out raw data before conv')
# configs['SNR_voting_thres_enable'] = True
# output_error_w_conv_w_correct_phase_w_snr_thresvote = []
# for dataset_idx in range(len(data_gen.data[input_data_name])):
#     print('Processing target at distance: ' + str(data_gen.data['target_distance'][dataset_idx]))
#     input_data = data_gen.data[input_data_name][dataset_idx]
#     output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
#
#     range_array = device.dsp.data[output_range_array_name].flt.flatten() # constant range array
#     range_array = range_array[~np.isnan(range_array)]
#
#     # data_viewer.assign_dict(device.dsp.data)
#     # data_viewer.plot_range_histogram(range_array_name=output_range_array_name, superimpose_M_values=True,
#     #                                  target_range=data_gen.data['target_distance'][dataset_idx],
#     #                                  dump_path=os.path.join('..', 'output', 'with_convolution_with_correction'),
#     #                                  filename=os.path.join('hist_'+str(dataset_idx)),
#     #                                  save_figure=True, show_figure=False)
#
#     error = np.average(np.abs(np.clip(range_array, a_min=0, a_max=device.dsp.max_range) - data_gen.data['target_distance'][dataset_idx])/device.dsp.max_range)
#     output_error_w_conv_w_correct_phase_w_snr_thresvote.append(error)
#     device.clear(force_clear=True)

## Display
import matplotlib.pyplot as plt

low_solar_data = np.load(os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_true_SNR_1mstep', 'low_solar.npz'))
t_distances = low_solar_data['dist']
high_solar_data = np.load(os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_true_SNR_1mstep', 'high_solar.npz'))

plt.figure(figsize=(12,8))
plt.semilogy(t_distances, high_solar_data['snr_2x2'])
plt.xlabel('Rangers [meter]')
plt.ylabel('Theoretical SNR')

plt.figure(figsize=(12,8))
plt.plot(np.asarray(distances), np.asarray(output_error_no_conv), label='no convolution')
# plt.plot(np.asarray(distances), np.asarray(output_error_w_conv_no_correct_phase), label='with convolution, no correct_phase')
plt.plot(np.asarray(distances), np.asarray(output_error_w_conv_w_correct_phase), label='with_convolution, with correct_phase')
# plt.plot(np.asarray(distances), np.asarray(output_error_w_conv_w_correct_phase_w_snr_thresvote), label='with_convolution (excluding low SNR pixels), with correct_phase')
plt.xlabel('Ranges [meter]')
plt.ylabel('Normalized (with respect to max_range) range error)')
plt.title('Range error (dash-dot bars = tap boundaries, dash-dash bars = phase boundaries)')

_c = 299792458
freq_indices = np.asarray([8, 7])
freq = 1.0e9 / (3 * freq_indices)  # Modulation frequencies in Hz.  [Low, High]
gcf = 1e9 / (3 * freq_indices[0] * freq_indices[1])
max_range = _c / (2 * gcf)
coef_f0 = (_c / (2 * freq[0]))
coef_f1 = (_c / (2 * freq[1]))
M_vals_0 = np.arange(-1, int(max_range / coef_f0)+1, 1, dtype=np.int)
M_vals_1 = np.arange(-1, int(max_range / coef_f1)+1, 1, dtype=np.int)
M_range_0 = coef_f0 * M_vals_0
M_range_1 = coef_f1 * M_vals_1
taps_vals_0 = np.interp(np.arange(0, len(M_vals_0), 1/3), np.arange(0, len(M_vals_0)), M_vals_0)
taps_vals_1 = np.interp(np.arange(0, len(M_vals_1), 1/3), np.arange(0, len(M_vals_1)), M_vals_1)
taps_range_0 = coef_f0 * taps_vals_0
taps_range_1 = coef_f1 * taps_vals_1
for idx, _ in enumerate(M_range_0):
    plt.axvline(x=M_range_0[idx], linestyle='--', color='r')
    plt.axvline(x=M_range_1[idx], linestyle='--', color='m')

for idx, _ in enumerate(taps_range_0):
    plt.axvline(x=taps_range_0[idx], linestyle='-.', color='r')
    plt.axvline(x=taps_range_1[idx], linestyle='-.', color='m')

plt.axvline(x=max_range, linestyle='solid', color='c', label='max range')
plt.legend(loc='lower right')

fig, ax1 = plt.subplots(figsize=(12, 8))
ax2 = ax1.twinx()
lns1 = ax1.plot(np.asarray(distances), np.asarray(precision_no_conv), 'k', label='no convolution - precision')
lns2 = ax1.plot(np.asarray(distances), np.asarray(precision_w_conv_w_correct_phase), 'r', label='with convolution - precision')
lns3 = ax2.semilogy(np.asarray(distances), np.asarray(phase_unwrapping_error_no_conv), 'k', linestyle='--', label='no convolution - phase unwrap errors')
lns4 = ax2.semilogy(np.asarray(distances), np.asarray(phase_unwrapping_error_w_conv_w_correct_phase), 'r', linestyle='--', label='with convolution - phase unwrap errors')
lns = lns1+lns2+lns3+lns4
labs = [l.get_label() for l in lns]
ax1.legend(lns, labs, loc=2)
ax1.grid(visible=True, which='both', axis='both')
ax1.set_ylim(0, 10)
ax2.set_ylim(0.1, 100)
ax1.set_xlabel('Ranges [meters]')
ax1.set_ylabel('Range precision [%]')
ax2.set_ylabel('Unwrap error rate [%]')

plt.figure(figsize=(12,8))
plt.loglog(np.asarray(phase_unwrapping_error_no_conv), high_solar_data['snr_2x2'], label='no convolution')
plt.loglog(np.asarray(phase_unwrapping_error_w_conv_w_correct_phase), high_solar_data['snr_2x2'], label='with convolution')
plt.xlabel('Unwrap error rate [%]')
plt.ylabel('Minimum SNR to achieve unwrap error rate')
plt.title('Minimum SNR needed to achieve a certain unwrap error rate (r = 10%)')
plt.legend()

plt.show()
