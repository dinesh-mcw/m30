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
num_rows_full_frame = 460
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
path = os.path.join('..', '..', '..', 'targets_merging', 'scene_data')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'hdr_scene_thresh2000_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs_stripe_mode_snr_akaike = {'perform_tap_add': perform_tap_add,
                                  'correct_strips': False,
                                  'HDR_mode_en': True,
                                  'HDR_override_threshold': 6140,
                                  'HDR_blooming_alg_select': 0,
                                  'HDR_blooming_id_threshold': 250,  # To identify bloomed pixels based on intensity
                                  'HDR_blooming_id_hdr_thres': 500,
                                  # To identify pixels belong to retroreflectors based on intensity
                                  'HDR_min_num_points_in_connected_region': 100,  # Alg 0 only
                                  'HDR_pixel_radius': 25,  # Alg 1 only
                                  'weighted_sum_en': True,
                                  'weight_by': 'SNR',
                                  'weight_by_SNR_max_points': 0,
                                  'weight_by_SNR_auto_mode': 'akaike',
                                  'weight_by_matched_filter_num_points': 20,
                                  'weight_by_matched_filter_mean': 10,
                                  'weight_by_matched_filter_std': 1.6,
                                  'weight_by_peak_num_points': 3,
                                  'weight_by_peak_type': 'intensity',
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
                                  'range_median_filter_en': False,
                                  'range_median_filter_size': [5, 5],
                                  'range_median_filter_shape': '+',
                                  'NN_enable': True,
                                  'NN_filter_level': 0,
                                  'NN_min_neighbors': 5,
                                  'NN_patch_size': 3,
                                  'NN_range_tolerance': 0.7,
                                  'SNR_threshold_enable': False,
                                  'SNR_threshold': 1.0,
                                  'pixel_mask_path': os.path.join('..', '..', '..', 'pixel_mask_SN88.bin'),
                                  'invalid_pixel_mask_enable': True}

configs_stripe_mode_snr_percentage = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_snr_percentage['weight_by_SNR_auto_mode'] = 'percentage'

configs_stripe_mode_snr_20_points = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_snr_20_points['weight_by_SNR_max_points'] = 20

configs_stripe_mode_snr_3_points = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_snr_3_points['weight_by_SNR_max_points'] = 3

configs_stripe_mode_peak_snr_3_pts = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_peak_snr_3_pts['weight_by'] = 'peak'
configs_stripe_mode_peak_snr_3_pts['weight_by_peak_num_points'] = 3
configs_stripe_mode_peak_snr_3_pts['weight_by_peak_type'] = 'SNR'

configs_stripe_mode_peak_snr_5_pts = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_peak_snr_5_pts['weight_by'] = 'peak'
configs_stripe_mode_peak_snr_5_pts['weight_by_peak_num_points'] = 5
configs_stripe_mode_peak_snr_5_pts['weight_by_peak_type'] = 'SNR'

configs_stripe_mode_peak_intensity_3_pts = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_peak_intensity_3_pts['weight_by'] = 'peak'
configs_stripe_mode_peak_intensity_3_pts['weight_by_peak_num_points'] = 3
configs_stripe_mode_peak_intensity_3_pts['weight_by_peak_type'] = 'intensity'

configs_stripe_mode_peak_intensity_5_pts = configs_stripe_mode_snr_akaike.copy()
configs_stripe_mode_peak_intensity_5_pts['weight_by'] = 'peak'
configs_stripe_mode_peak_intensity_5_pts['weight_by_peak_num_points'] = 5
configs_stripe_mode_peak_intensity_5_pts['weight_by_peak_type'] = 'intensity'

configs_normal_mode = configs_stripe_mode_snr_akaike.copy()
configs_normal_mode['binning'] = (1, 1)
configs_normal_mode['convolution_kernel_x_size'] = 7
configs_normal_mode['convolution_kernel_y_size'] = 15
configs_normal_mode['M_filter_type'] = 8
configs_normal_mode['M_filter_size'] = (3, 3)
configs_normal_mode['range_median_filter_size'] = [5, 5]

# Process
input_data = data_gen.data[input_data_name]
print('SNR sorting - akaike')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_snr_akaike,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_snr_akaike = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('SNR sorting - percentage')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_snr_percentage,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_snr_percentage = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('SNR sorting - all 20 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_snr_20_points,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_snr_20_points = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('SNR sorting - max 3 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_snr_3_points,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_snr_3_points = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('SNR max - 3 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_peak_snr_3_pts,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_peak_snr_3_pts = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('SNR max - 5 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_peak_snr_5_pts,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_peak_snr_5_pts = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('Intensity max - 3 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_peak_intensity_3_pts,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_peak_intensity_3_pts = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

print('Intensity max - 5 points')
output_range_array_name = device.stripe_mode_process(input_data=input_data, configs=configs_stripe_mode_peak_intensity_5_pts,
                                                     start_vector=start_vector, HDR=(data_gen.data['hdr_thresholds'],
                                                                                     data_gen.data['hdr_retries']))
stripe_mode_peak_intensity_5_pts = np.copy(device.dsp.data[output_range_array_name].flt)
device.clear(force_clear=True)

# output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs_normal_mode)
# normal_mode_depth_map = np.copy(device.dsp.data[output_range_array_name].flt)
# device.clear(force_clear=True)

# Display
import matplotlib.pyplot as plt
max_range = 10.0

plt.subplot(2, 4, 1)
plt.imshow(np.flipud(stripe_mode_snr_akaike), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Auto (Akaike criterion)')
plt.subplot(2, 4, 2)
plt.imshow(np.flipud(stripe_mode_snr_percentage), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Auto (Simple percentage)')
plt.subplot(2, 4, 3)
plt.imshow(np.flipud(stripe_mode_snr_20_points), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Vanilla (20 pts)')
plt.subplot(2, 4, 4)
plt.imshow(np.flipud(stripe_mode_snr_3_points), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Vanilla (3 pts)')

plt.subplot(2, 4, 5)
plt.imshow(np.flipud(stripe_mode_peak_snr_5_pts), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Windowed SNR (5 pts)')
plt.subplot(2, 4, 6)
plt.imshow(np.flipud(stripe_mode_peak_snr_3_pts), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Windowed SNR (3 pts)')
plt.subplot(2, 4, 7)
plt.imshow(np.flipud(stripe_mode_peak_intensity_5_pts), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Windowed intensity (5 pts)')
plt.subplot(2, 4, 8)
plt.imshow(np.flipud(stripe_mode_peak_intensity_3_pts), cmap='jet', vmin=0, vmax=max_range, aspect='auto')
plt.title('Stripe mode - Windowed intensity (3 pts)')

plt.show()