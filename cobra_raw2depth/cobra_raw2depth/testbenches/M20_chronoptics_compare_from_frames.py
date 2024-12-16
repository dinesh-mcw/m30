import argparse
import sys
import os
from glob import glob
import numpy as np
import time
from pathlib import Path

from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

"""Testbench to compare with Chronoptics pipeline
"""


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-s', '--save',
                        action='store_true',
                        help='Save npy files')
    parser.add_argument('-v', '--view',
                        action='store_true',
                        help='View 3d point cloud')
    return parser.parse_args(argv)


##############################################
# General params
num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 91                # Number of ROIs per frame
num_rows_full_frame = 460
num_frames = 12              # The device processes 1 frame at a time

device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                           num_rois, num_rows_full_frame)
data_viewer = M20_iTOF_viewer()

# Data path: paths where the npys containing the frames are
data_path = Path(Path.home(), 'chronoptics', 'scene_h', 'binned_frame_npy')
file_name_base = 'scene_h_assembled_frame_binned_'
pixel_mask_path = Path(Path.home(), 'chronoptics', 'pixel_mask_SN88.bin')
mapping_table_path = Path(Path.home(), 'chronoptics', 'mapping_table_SN88.csv')

###############################################################################################
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
           'pixel_mask_path': pixel_mask_path,
           'invalid_pixel_mask_enable': False}
           #'invalid_pixel_mask_enable': True}


def main(args):
    # 1: Load data
    fnames = sorted(glob(os.path.join(data_path, file_name_base + '*.npy')))
    range_frames = []
    signal_frames = []
    SNR_frames = []
    background_frames = []

    for frame in range(len(fnames)):
        print(str(100*frame/len(fnames))+' %')
        combined_data_frame = np.load(fnames[frame])

        # Create frames: combined_data_frame, phase_frame, signal_frame, background_frame, SNR_frame
        num_rows = combined_data_frame.shape[2]
        num_cols = combined_data_frame.shape[3]
        col_idx = range(num_cols)
        row_idx = range(num_rows)
        col_ptr = np.outer(np.ones_like(row_idx), col_idx)
        row_ptr = np.outer(row_idx, np.ones_like(col_idx))
        # I could loop the two frequency indexes, but easy enough to create a 2D matrix of frequency index
        freq_ptr = [np.zeros([num_rows, num_cols], dtype=int), np.ones([num_rows, num_cols], dtype=int)]
        # argmin gives the index of which of the three taps is the minimum
        # So I don't have to remake all the pointers, I added this bit to calculate the phases from the smoothed
        # data, although it probably ought to be a separate function.
        C_ptr = np.argmin(combined_data_frame, axis=0)
        A_ptr = np.mod(C_ptr + 1, 3)
        B_ptr = np.mod(C_ptr + 2, 3)
        As = combined_data_frame[A_ptr, freq_ptr, [row_ptr, row_ptr], [col_ptr, col_ptr]]
        Bs = combined_data_frame[B_ptr, freq_ptr, [row_ptr, row_ptr], [col_ptr, col_ptr]]
        Cs = combined_data_frame[C_ptr, freq_ptr, [row_ptr, row_ptr], [col_ptr, col_ptr]]
        signal_frame = As + Bs - 2 * Cs
        Cs[Cs <= 0] = 1
        signal_frame[signal_frame <= 0] = 1
        phase_frame = (Bs - Cs)/(3 * signal_frame) + A_ptr / 3
        background_frame = np.sqrt(2 * Cs)
        snr_frame = (np.sqrt(configs['binning'][0]
                             * configs['binning'][1]
                             * configs['temporal_boxcar_length']
                             / 8)
                     * signal_frame
                     / background_frame)

        device.frame_start_row = 0
        device.frame_stop_row = 460

        output_range_array_name = device.process_from_frame(combined_data_frame,
                                                            phase_frame,
                                                            signal_frame,
                                                            background_frame,
                                                            snr_frame,
                                                            configs=configs)

        # 3: Data analysis
        range_frames.append(device.dsp.data[output_range_array_name].flt)
        signal_frames.append(device.dsp.data["signal_frame"].flt)
        SNR_frames.append(device.dsp.data["SNR_frame"].flt)
        background_frames.append(device.dsp.data["background_frame"].flt)
        data_viewer.assign_dict(device.dsp.data)
        # data_viewer.plot_snr_simple(save_figure=True, flip_ud=False)
        data_viewer.plot_range_simple(range_array_name=output_range_array_name, save_figure=True,
                                      flip_ud=False, show_figure=False, title='RTD frame ' + str(frame),
                                      filename=f'range_{file_name_base[0:8]}_{fnames[frame][-10:-4]}.png')

        if args.view:
            if frame < 1:
                data_viewer.plot_3d_range(mapping_table_path,
                                          configs['binning'],
                                          device.frame_start_row,
                                          device.frame_stop_row,
                                          max_rgb_range=10.0,
                                          range_multiplicator=1000.0, # set range to mm for comparison with chronoptics pt
                                          range_array_name=output_range_array_name,
                                          show_pointcloud=False,
                                          save_pointcloud=True,
                                          filename=file_name_base+str(frame)+'.ply')
            data_viewer.dump_frames_to_binary_files()
            data_viewer.dump_range_arrays()

        device.clear(force_clear=True)

    range_frames = np.asarray(range_frames)
    signal_frames = np.asarray(signal_frames)
    SNR_frames = np.asarray(SNR_frames)
    background_frames = np.asarray(background_frames)
    filename = file_name_base + time.strftime("%Y%m%d-%H%M%S") + '.npy'
    dump_path = os.path.join('..', 'output')
    if args.save:
        Path(dump_path).mkdir(parents=True, exist_ok=True)
        np.save(os.path.join(dump_path, 'RANGE_' + filename), range_frames)
        np.save(os.path.join(dump_path, 'SIGNAL_' + filename), signal_frames)
        np.save(os.path.join(dump_path, 'SNR_' + filename), SNR_frames)
        np.save(os.path.join(dump_path, 'BACKGROUND_' + filename), background_frames)


if __name__ == "__main__":
    args_ = parse_args(sys.argv[1:])
    main(args_)
