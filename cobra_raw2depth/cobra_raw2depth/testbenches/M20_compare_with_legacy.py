import numpy as np
import os
import matplotlib.pyplot as plt

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

from legacy.GPixel_iTOF_Frame_Mod import GPixel_iTOF_Frame_Mod

"""
    Simple testbench to use M20-GPixel with generated/imported data and visualize the depth map at the output
"""

def run_legacy():
    frame = GPixel_iTOF_Frame_Mod(480, 640)
    fpath = os.path.join('..', '..', '..', 'cobra_raw2depth_data', '2021-07-08-loading_dock')
    frame.load_ROI_data(fpath, 'raw_data')
    frame.calculate_phases(compute_SNR=False)
    frame.smooth_combined_data(7, 14)
    frame.calculate_smoothed_phases()
    frame.correct_phase()
    frame.calculate_range()
    return frame

##############################################
# 1: Import input data
# General params
num_rows_per_roi = 21       # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 88               # Number of ROIs per frame
num_rows_full_frame = 480
num_frames = 1              # The device processes 1 frame at a time

data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois)
device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

###############################################################################################
# Fake data generator (this data is noise) - UNCOMMENT NEXT 2 LINES TO USE
# data_gen.generate_rois_with_random_int_values(0, 0xFFF, np.uint16)
# input_data = data_gen.data["tap_data"]
###############################################################################################

###############################################################################################
# Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
# UNCOMMENT ALL THE FOLLOWING LINES TO USE
path = os.path.join('..', '..', '..', 'cobra_raw2depth_data', '2021-07-08-loading_dock')
file_name_base = 'raw_data'
data_gen.load_roi_data(path, file_name_base, num_frames, np.uint16)

if data_gen.data["tap_data"] is not None:
    input_data = data_gen.data["tap_data"]
    perform_tap_add = True
elif data_gen.data["combined_data"] is not None:
    input_data = data_gen.data["combined_data"]
    perform_tap_add = True
###############################################################################################
start_vector = data_gen.data["ROI_start_vector"]

# 2: Process
device(input_data=input_data, roi_start_vector=start_vector, perform_tap_accumulation=perform_tap_add)

# Legacy class
legacy_data = run_legacy()

if np.any(np.asarray(legacy_data.start_vector) - device.roi_start_vector):
    print("Start vector mismatch")

input_data_relative_error = 100*np.abs((device.dsp.data["input_data"].flt - legacy_data.tap_data_rois) / legacy_data.tap_data_rois)
input_data_relative_error = np.clip(input_data_relative_error, a_min=0, a_max=100)
combined_data_re = 100*np.abs((device.dsp.data["combined_data"].flt - legacy_data.combined_rois) / legacy_data.combined_rois)
combined_data_re = np.clip(combined_data_re, a_min=0, a_max=100)
snr_data_rois_re = 100*np.abs((device.dsp.data["SNR_2"].flt - legacy_data.SNR_rois) / legacy_data.SNR_rois)
snr_data_rois_re = np.clip(snr_data_rois_re, a_min=0, a_max=100)
snr_frame_re = 100*np.abs((device.dsp.data["SNR_2_frame"].flt - legacy_data.SNR) / legacy_data.SNR)
snr_frame_re = np.clip(snr_frame_re, a_min=0, a_max=100)
combined_data_frame_re = 100*np.abs((device.dsp.data["combined_data_frame"].flt - legacy_data.combined) / legacy_data.combined)
combined_data_frame_re = np.clip(combined_data_frame_re, a_min=0, a_max=100)
range_relative_error = 100*np.abs((device.dsp.data["ranges"].flt - legacy_data.ranges) / legacy_data.ranges)
range_relative_error = np.clip(range_relative_error, a_min=0, a_max=100)
plt.subplot(231)
plt.hist(input_data_relative_error.flatten())
plt.title("Tap data ROIs")
plt.subplot(232)
plt.hist(combined_data_re.flatten())
plt.title("Combined data ROIs")
plt.subplot(233)
plt.hist(snr_data_rois_re.flatten())
plt.title("SNR data rois")
plt.subplot(234)
plt.hist(snr_frame_re.flatten())
plt.title("SNR frame")
plt.subplot(235)
plt.hist(combined_data_frame_re.flatten())
plt.title("Combined data frame")
plt.subplot(236)
plt.hist(range_relative_error.flatten())
plt.plot("Range data (output)")
plt.show()

