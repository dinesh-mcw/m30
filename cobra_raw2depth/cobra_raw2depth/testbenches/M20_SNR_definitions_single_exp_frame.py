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
path = os.path.join('..', '..', '..', 'strips_investiguation', 'angular_resolution_tests', '1p0')
use_old_metadata_format = False
fov_num_to_use = 0
file_name_base = 'ang_res_1p0_0_01_'
input_data_name, input_data_shape, perform_tap_add =\
    data_gen.load_sensor_data(path, file_name_base, num_frames, np.uint16, use_old_metadata_format, fov_num_to_use)

###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
configs = {'perform_tap_add': perform_tap_add,
           'binning': (2, 2),
           'SNR_voting_combined': True,
           'SNR_voting_thres_enable': False,
           'temporal_boxcar_length': 1,  # Set this to 1 to disable it
           'enable_convolution': True,
           'enable_phase_correction': True,
           'use_1d_convolutions': False,
           'convolution_kernel_x_size': 7,
           'convolution_kernel_y_size': 15,
           #'convolution_kernel_x': [0.003325727090904847, 0.023817922039352332, 0.09719199228354419, 0.22597815248035666, 0.2993724122116841, 0.22597815248035666, 0.09719199228354419, 0.023817922039352332, 0.003325727090904847],
           #'convolution_kernel_y': [0.0036553302113411516, 0.044531038224040448, 0.19957426729948921, 0.32904233958106088, 0.19957426729948921, 0.044531038224040448, 0.0036553302113411516],
           'M_filter': False,
           'M_filter_type': 3,
           'M_median_filter_size': 7,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'NN_enable': False,
           'NN_filter_level': 2,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 6}

input_data = data_gen.data[input_data_name]
output_range_array_name = device(input_data=input_data, roi_start_vector=start_vector, configs=configs)

# 3: Data analysis
data_viewer.assign_dict(device.dsp.data)
data_viewer.plot_snr_simple(save_figure=False, flip_ud=True)
# data_viewer.plot_range_simple(range_array_name=output_range_array_name, flip_ud=True, save_figure=False)
# data_viewer.plot_snr_vs_range()
# data_viewer.dump_frames_to_binary_files()
# data_viewer.dump_range_arrays()

