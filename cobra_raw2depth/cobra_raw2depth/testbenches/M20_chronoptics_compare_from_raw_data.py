import numpy as np
import os
import time
from pathlib import Path

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
num_frames = 32              # The device processes 1 frame at a time

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
path = os.path.join('..', '..', '..', 'chronoptics_data', 'scene_g', 'raw_roi_bin', 'eol69')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'scene_g_0_12_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': False,
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
           'M_filter': True,
           'M_filter_loc': 0,
           'M_filter_type': 8,
           'M_filter_size': (3, 3),
           'M_filter_shape': None,
           'range_edge_filter_en': False,
           'range_median_filter_en': True,
           'range_median_filter_size': [5, 5],
           'range_median_filter_shape': '+',
           'NN_enable': True,
           'NN_filter_level': 5,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0.1,
           'pixel_mask_path': os.path.join('..', '..', '..', 'pixel_mask_SN88.bin'),
           'invalid_pixel_mask_enable': True}

range_frames = []
signal_frames = []
SNR_frames = []
background_frames = []

num_frames = data_gen.num_loaded_frames
for frame in range(num_frames):
    print(str(100*frame/num_frames)+' %')
    input_data = data_gen.data[input_data_name][frame]
    output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector[frame], configs=configs)

    # 3: Data analysis
    range_frames.append(device.dsp.data[output_range_array_name].flt)
    signal_frames.append(device.dsp.data["signal_frame"].flt)
    SNR_frames.append(device.dsp.data["SNR_frame"].flt)
    background_frames.append(device.dsp.data["background_frame"].flt)
    data_viewer.assign_dict(device.dsp.data)
    # data_viewer.plot_snr_simple(save_figure=True, flip_ud=True)
    data_viewer.plot_range_simple(range_array_name=output_range_array_name, save_figure=True,
                                  flip_ud=False, show_figure=False, title='RTD frame ' + str(frame))
    device.clear(force_clear=True)

range_frames = np.asarray(range_frames)
signal_frames = np.asarray(signal_frames)
SNR_frames = np.asarray(SNR_frames)
background_frames = np.asarray(background_frames)
filename = file_name_base + time.strftime("%Y%m%d-%H%M%S") + '.npy'
dump_path = os.path.join('..', 'output')
Path(dump_path).mkdir(parents=True, exist_ok=True)
np.save(os.path.join(dump_path, 'RANGE_' + filename), range_frames)
np.save(os.path.join(dump_path, 'SIGNAL_' + filename), signal_frames)
np.save(os.path.join(dump_path, 'SNR_' + filename), SNR_frames)
np.save(os.path.join(dump_path, 'BACKGROUND_' + filename), background_frames)

