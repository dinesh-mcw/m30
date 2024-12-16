import numpy as np
import os

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

data_gen_low_light = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
data_gen_high_light = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

# -----------------------------------------------------------------------------------------
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_dataset_1_to_25_meters_physical_SNR_1mstep_low_solar')
use_old_metadata_format = False
fov_num_to_use = 0
input_data_name, input_data_shape, perform_tap_add =\
    data_gen_low_light.load_rm_data(path, step=1)

path = os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_dataset_1_to_25_meters_physical_SNR_1mstep_high_solar')
data_gen_high_light.load_rm_data(path, step=1)

###############################################################################################
start_vector = data_gen_low_light.data["ROI_start_vector"]
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                           num_rois, num_rows_full_frame)

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
           'M_filter_type': 0,
           'M_median_filter_size': 7,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 2,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0.2}

distances = []
raw_data = {}

raw_data['1x1'] = {}
raw_data['1x1']['low'] = {}
raw_data['1x1']['high'] = {}
raw_data['1x1']['low']['signal'] = []
raw_data['1x1']['low']['temp_signal'] = []
raw_data['1x1']['low']['noise'] = []
raw_data['1x1']['high']['signal'] = []
raw_data['1x1']['high']['temp_signal'] = []
raw_data['1x1']['high']['noise'] = []
for dataset_idx in range(len(data_gen_low_light.data[input_data_name])):
    print('1x1: Processing target at distance: ' + str(data_gen_low_light.data['target_distance'][dataset_idx]))
    distances.append(data_gen_low_light.data['target_distance'][dataset_idx])

    input_data = data_gen_low_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['1x1']['low']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['1x1']['low']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['1x1']['low']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

    input_data = data_gen_high_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['1x1']['high']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['1x1']['high']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['1x1']['high']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

raw_data['2x2'] = {}
raw_data['2x2']['low'] = {}
raw_data['2x2']['high'] = {}
raw_data['2x2']['low']['signal'] = []
raw_data['2x2']['low']['temp_signal'] = []
raw_data['2x2']['low']['noise'] = []
raw_data['2x2']['high']['signal'] = []
raw_data['2x2']['high']['temp_signal'] = []
raw_data['2x2']['high']['noise'] = []
configs['binning'] = (2, 2)
for dataset_idx in range(len(data_gen_low_light.data[input_data_name])):
    print('2x2: Processing target at distance: ' + str(data_gen_low_light.data['target_distance'][dataset_idx]))

    input_data = data_gen_low_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['2x2']['low']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['2x2']['low']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['2x2']['low']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

    input_data = data_gen_high_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['2x2']['high']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['2x2']['high']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['2x2']['high']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

raw_data['4x4'] = {}
raw_data['4x4']['low'] = {}
raw_data['4x4']['high'] = {}
raw_data['4x4']['low']['signal'] = []
raw_data['4x4']['low']['temp_signal'] = []
raw_data['4x4']['low']['noise'] = []
raw_data['4x4']['high']['signal'] = []
raw_data['4x4']['high']['temp_signal'] = []
raw_data['4x4']['high']['noise'] = []
configs['binning'] = (4, 4)
for dataset_idx in range(len(data_gen_low_light.data[input_data_name])):
    print('4x4: Processing target at distance: ' + str(data_gen_low_light.data['target_distance'][dataset_idx]))

    input_data = data_gen_low_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['4x4']['low']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['4x4']['low']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['4x4']['low']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

    input_data = data_gen_high_light.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)
    raw_data['4x4']['high']['signal'].append(device.dsp.data["signal_frame"].flt)
    raw_data['4x4']['high']['temp_signal'].append(device.dsp.data["temp_signal_frame"].flt)
    raw_data['4x4']['high']['noise'].append(device.dsp.data["background_frame"].flt)
    device.clear(force_clear=True)

# 3: Try different SNR definitions
print('Computing SNRs')
bin_tested = ['1x1', '2x2', '4x4']
intensities = ['low', 'high']
types = ['signal', 'noise', 'temp_signal']
for i, bin in enumerate(bin_tested):
    for intensity in intensities:
        for type in types:
            raw_data[bin][intensity][type] = np.asarray(raw_data[bin][intensity][type])

# # Add FPN
# fpn_data = np.load(os.path.join('..', '..', '..', '20220209_122303_fpn_img_stack_binned_1_frame.npy.npz'))
# for i, bin in enumerate(bin_tested):
#     for intensity in intensities:
#         raw_data[bin][intensity]['noise'] += fpn_data['bin_'+bin]

SNR = {}
# SNR_0 = signal / (2C)
type = '0'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / (2*raw_data[bin][intensity]['noise']), axis=(1, 2, 3))

# SNR_1 = signal / np.sqrt(2C)
type = '1'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / np.sqrt(2*raw_data[bin][intensity]['noise']), axis=(1, 2, 3))

# SNR_2 = signal / std(2C) over N = 5x5
window_dim = 3
type = '2'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        std_for_single_distance = []
        for i_dist in range(len(distances)):
            stds = np.zeros((raw_data[bin][intensity]['noise'].shape[1], raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
            half_window_dim = int((window_dim - 1) / 2)
            for freq in range(raw_data[bin][intensity]['noise'].shape[1]):
                stds[freq, :, :] = np.std(raw_data[bin][intensity]['noise'][i_dist, freq, :, :]) * np.ones((raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
                # for row_idx in range(0, raw_data[bin][intensity]['noise'].shape[2]):
                #     for col_idx in range(0, raw_data[bin][intensity]['noise'].shape[3]):
                #         row_top = max(0, row_idx - half_window_dim)
                #         row_bot = min(raw_data[bin][intensity]['noise'].shape[2], row_idx + half_window_dim + 1)
                #         col_left = max(0, col_idx - half_window_dim)
                #         col_right = min(raw_data[bin][intensity]['noise'].shape[3], col_idx + half_window_dim + 1)
                #         sub_array = raw_data[bin][intensity]['noise'][i_dist, freq, row_top:row_bot, col_left:col_right]
                #         stds[freq, row_idx, col_idx] = np.std(2*sub_array.flatten())
                #         if stds[freq, row_idx, col_idx] == 0:
                #             stds[freq, row_idx, col_idx] = 0.1 # avoid division by 0
            std_for_single_distance.append(stds)

        std_for_single_distance = np.asarray(std_for_single_distance)
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / std_for_single_distance, axis=(1, 2, 3))

# SNR_3 = (A+B-2C) / std(A+B-2C) over N = 3x3
window_dim = 3
type = '3'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        std_for_single_distance = []
        for i_dist in range(len(distances)):
            stds = np.zeros((raw_data[bin][intensity]['noise'].shape[1], raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
            half_window_dim = int((window_dim - 1) / 2)
            for freq in range(raw_data[bin][intensity]['noise'].shape[1]):
                stds[freq, :, :] = np.std(raw_data[bin][intensity]['signal'][i_dist, freq, :, :]) * np.ones(
                    (raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
                # for row_idx in range(0, raw_data[bin][intensity]['noise'].shape[2]):
                #     for col_idx in range(0, raw_data[bin][intensity]['noise'].shape[3]):
                #         row_top = max(0, row_idx - half_window_dim)
                #         row_bot = min(raw_data[bin][intensity]['noise'].shape[2], row_idx + half_window_dim + 1)
                #         col_left = max(0, col_idx - half_window_dim)
                #         col_right = min(raw_data[bin][intensity]['noise'].shape[3], col_idx + half_window_dim + 1)
                #         sub_array_sig = raw_data[bin][intensity]['signal'][i_dist, freq, row_top:row_bot, col_left:col_right]
                #         stds[freq, row_idx, col_idx] = np.std(sub_array_sig.flatten())
            std_for_single_distance.append(stds)

        std_for_single_distance = np.asarray(std_for_single_distance)
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / std_for_single_distance, axis=(1, 2, 3))

# SNR_4 = (A+B-2C) / std(A+B) over N = 3x3
window_dim = 3
type = '4'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        std_for_single_distance = []
        for i_dist in range(len(distances)):
            stds = np.zeros((raw_data[bin][intensity]['noise'].shape[1], raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
            half_window_dim = int((window_dim - 1) / 2)
            for freq in range(raw_data[bin][intensity]['noise'].shape[1]):
                stds[freq, :, :] = np.std(raw_data[bin][intensity]['temp_signal'][i_dist, freq, :, :]) * np.ones(
                    (raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
                # for row_idx in range(0, raw_data[bin][intensity]['noise'].shape[2]):
                #     for col_idx in range(0, raw_data[bin][intensity]['noise'].shape[3]):
                #         row_top = max(0, row_idx - half_window_dim)
                #         row_bot = min(raw_data[bin][intensity]['noise'].shape[2], row_idx + half_window_dim + 1)
                #         col_left = max(0, col_idx - half_window_dim)
                #         col_right = min(raw_data[bin][intensity]['noise'].shape[3], col_idx + half_window_dim + 1)
                #         sub_array_sig = raw_data[bin][intensity]['temp_signal'][i_dist, freq, row_top:row_bot,
                #                         col_left:col_right]
                #         stds[freq, row_idx, col_idx] = np.std(sub_array_sig.flatten())
            std_for_single_distance.append(stds)

        std_for_single_distance = np.asarray(std_for_single_distance)
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / std_for_single_distance, axis=(1, 2, 3))

# SNR_5 = (A+B-2C) / std(A+B+C) over N = 3x3
window_dim = 3
type = '5'
SNR[type] = {}
for i, bin in enumerate(bin_tested):
    SNR[type][bin] = {}
    for intensity in intensities:
        std_for_single_distance = []
        for i_dist in range(len(distances)):
            stds = np.zeros((raw_data[bin][intensity]['noise'].shape[1], raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
            half_window_dim = int((window_dim - 1) / 2)
            for freq in range(raw_data[bin][intensity]['noise'].shape[1]):
                stds[freq, :, :] = np.std(raw_data[bin][intensity]['temp_signal'][i_dist, freq, :, :] + raw_data[bin][intensity]['noise'][i_dist, freq, :, :]) * np.ones(
                    (raw_data[bin][intensity]['noise'].shape[2], raw_data[bin][intensity]['noise'].shape[3]))
                # for row_idx in range(0, raw_data[bin][intensity]['noise'].shape[2]):
                #     for col_idx in range(0, raw_data[bin][intensity]['noise'].shape[3]):
                #         row_top = max(0, row_idx - half_window_dim)
                #         row_bot = min(raw_data[bin][intensity]['noise'].shape[2], row_idx + half_window_dim + 1)
                #         col_left = max(0, col_idx - half_window_dim)
                #         col_right = min(raw_data[bin][intensity]['noise'].shape[3], col_idx + half_window_dim + 1)
                #         sub_array_n = raw_data[bin][intensity]['noise'][i_dist, freq, row_top:row_bot, col_left:col_right]
                #         sub_array_sig = raw_data[bin][intensity]['temp_signal'][i_dist, freq, row_top:row_bot,
                #                         col_left:col_right]
                #         stds[freq, row_idx, col_idx] = np.std(sub_array_sig.flatten() + sub_array_n.flatten())
            std_for_single_distance.append(stds)

        std_for_single_distance = np.asarray(std_for_single_distance)
        SNR[type][bin][intensity] = np.mean(raw_data[bin][intensity]['signal'] / std_for_single_distance, axis=(1, 2, 3))

# 4: Show results
print('Displaying results')
definitions = ['signal/2C', 'signal/sqrt(2C)', 'signal/std(2C), 3x3', 'signal / std(sig), 3x3',
               'signal/std(A+B), 3x3', 'signal/std(A+B+C), 3x3']
types = ['0', '1', '2', '3', '4', '5']
bin_tested = ['1x1', '2x2', '4x4']
intensities = ['low', 'high']

low_solar_data = np.load(os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_true_SNR_1mstep', 'low_solar.npz'))
t_distances = low_solar_data['dist']
high_solar_data = np.load(os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_true_SNR_1mstep', 'high_solar.npz'))

import matplotlib.pyplot as plt

distances = np.asarray(distances)

fig = plt.figure(1)
for i, type in enumerate(types):
    ax = fig.add_subplot(2, 3, i+1, label='High solar noise: SNR (def: ' + definitions[i] +') vs distance')
    ax.semilogy(t_distances, high_solar_data['snr_1x1'], linestyle='--', label='1x1 binning, theoretical')
    ax.semilogy(distances, SNR[type]['1x1']['high'], label='1x1 binning, meas')
    ax.semilogy(t_distances, high_solar_data['snr_2x2'], linestyle='--', label='2x2 binning, theoretical')
    ax.semilogy(distances, SNR[type]['2x2']['high'], label='2x2 binning, meas')
    ax.semilogy(t_distances, high_solar_data['snr_4x4'], linestyle='--', label='4x4 binning, theoretical')
    ax.semilogy(distances, SNR[type]['4x4']['high'], label='4x4 binning, meas')
    ax.set_xlabel('Range [m]')
    ax.set_ylabel('SNR (log-scale)')
    ax.set_title('High solar noise: SNR (def: ' + definitions[i] +') vs distance')
    if i == 1:
        ax.legend()

fig2 = plt.figure(2)
for i, type in enumerate(types):
    ax = fig2.add_subplot(2, 3, i+1, label='No solar noise: SNR (def: ' + definitions[i] +') vs distance')
    ax.semilogy(t_distances, low_solar_data['snr_1x1'], linestyle='--', label='1x1 binning, theoretical')
    ax.semilogy(distances, SNR[type]['1x1']['low'], label='1x1 binning, meas')
    ax.semilogy(t_distances, low_solar_data['snr_2x2'], linestyle='--', label='2x2 binning, theoretical')
    ax.semilogy(distances, SNR[type]['2x2']['low'], label='2x2 binning, meas')
    ax.semilogy(t_distances, low_solar_data['snr_4x4'], linestyle='--', label='4x4 binning, theoretical')
    ax.semilogy(distances, SNR[type]['4x4']['low'], label='4x4 binning, meas')
    ax.set_xlabel('Range [m]')
    ax.set_ylabel('SNR (log-scale)')
    ax.set_title('No solar noise: SNR (def: ' + definitions[i] + ') vs distance')
    if i == 1:
        ax.legend()

plt.show()

