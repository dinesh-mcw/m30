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
path = os.path.join('..', '..', '..', 'rm_generated_datasets', 'rm_dataset_1_to_25_meters_physical_SNR_1mstep_low_solar')
use_old_metadata_format = False
fov_num_to_use = 0
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_rm_data(path, step=1)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'correct_strips': False,
           'binning': (2, 2),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'HDR_mode_en': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 5,
           'convolution_kernel_y_size': 7,
           'M_filter': True,
           'M_filter_loc': 0,
           'M_filter_type': 8,
           'M_filter_size': (3, 3),
           'M_filter_shape': None,
           'range_edge_filter_en': False,
           'range_median_filter_en': True,
           'range_median_filter_size': [5, 5],
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

import matplotlib.pyplot as plt

for dataset_idx in range(len(data_gen.data[input_data_name])):

    if data_gen.data['target_distance'][dataset_idx] > 15:
        break

    print('Processing target distance of ' + str(data_gen.data['target_distance'][dataset_idx]))
    input_data = data_gen.data[input_data_name][dataset_idx]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)

    data_viewer.assign_dict(device.dsp.data)

    # plt.figure()
    # plt.hist(device.dsp.data['M'].flt[np.logical_or(device.dsp.data['phase_correction_mask'].flt[0, :, :], device.dsp.data['phase_correction_mask'].flt[1, :, :])], bins=np.arange(16)-0.5, rwidth=0.1)

    plt.figure(figsize=(15, 5))
    plt.subplot(1, 2, 1)
    plt.hist(device.dsp.data['M+N'].flt.flatten(), bins=np.arange(16)-0.5, rwidth=0.1)
    plt.xlim([-1, 16])
    plt.xlabel('M+N')
    plt.title('M+N histogram (target distance = ' + str(data_gen.data['target_distance'][dataset_idx]) + ' m)')
    # plt.subplot(1, 3, 2)
    # plt.imshow(np.mean(device.dsp.data["SNR_frame"].flt, axis=0), cmap='Greys_r')
    # plt.title('SNR map (Mean SNR = ' + str(np.nanmean(device.dsp.data["SNR_frame"].flt.flatten())) + ')')
    # plt.colorbar()
    # plt.clim([0, 130])
    plt.subplot(1, 2, 2)
    plt.imshow(device.dsp.data[output_range_array_name].flt, cmap='viridis')
    plt.title('Depth map')
    plt.colorbar()
    plt.clim([0, device.dsp.max_range])
    dump_path = os.path.join('..', 'output')
    filename = 'SNR_map_dist_' + '{:02d}'.format(dataset_idx) + '.png'
    # plt.show()
    plt.savefig(os.path.join(dump_path, filename))

    device.clear(force_clear=True)

