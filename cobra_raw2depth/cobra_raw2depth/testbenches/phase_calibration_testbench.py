import argparse
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device
from development.src.M20_iTOF_viewer import M20_iTOF_viewer

"""Simple testbench to use M20-GPixel with imported data and save the SNR-voted frames for sharing with customers
"""

data_folder = Path(Path.home(), 'pixel-phase')
raw_data_folder = data_folder / 'phase_scene'
raw_data_prepend = 'pixel_phase_scene'

IGNORE = {}
OFFSET = 0
START = 3
END = 4
N_ROIS = 5000


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-v', '--verbose',
                        action='store_true',
                        help='Print stuff')
    parser.add_argument('-c', '--cal',
                        action='store_true',
                        help='Process calibrated data')
    parser.add_argument('-r', '--rot',
                        action='store_true',
                        help='Process calibrated data using precalculated rotation matrices')
    parser.add_argument('-u', '--uncal',
                        action='store_true',
                        help='Process uncalibrated data')
    parser.add_argument('--ply', action='store_true',
                        help='Save ply files')
    return parser.parse_args(argv)


# 2: Process
configs = {'perform_tap_add': False,
           'correct_strips': False,
           'phase_cal': True,
           'phase_cal_use_time_array': False,
           'phase_cal_time_array_path': data_folder / 'time_calibration_unbinned_fxp.npy',
           'phase_cal_rot0_path': data_folder / 'rotation_array_freq0.bin',
           'phase_cal_rot1_path': data_folder / 'rotation_array_freq1.bin',
           'phase_cal_int0_path': data_folder / 'integer_skip_freq0.bin',
           'phase_cal_int1_path': data_folder / 'integer_skip_freq1.bin',
           'binning': (1, 1),
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
           'M_filter_type': 3,
           'M_median_filter_size': 3,
           'M_median_filter_shape': None,
           'range_edge_filter_en': False,
           'range_median_filter_en': True,
           'range_median_filter_size': [5, 5],
           'range_median_filter_shape': '+',
           'NN_enable': True,
           'NN_filter_level': 0,
           'NN_min_neighbors': 6,
           'NN_patch_size': 3,
           'NN_range_tolerance': 0.7,
           'SNR_threshold_enable': False,
           'SNR_threshold': 0.1,
           'pixel_mask_path': data_folder / 'pixel_mask_A.bin',
           'invalid_pixel_mask_enable': True}



##############################################
# 1: Import input data
# General params
num_rows_per_roi = 20        # Number of rows per ROI at the input (before any binning)
num_columns_per_roi = 640   # Number of columns per ROI at the input (before any binning)
num_rois = 91               # Number of ROIs per frame
num_rows_full_frame = 460




def main(args):
    data_gen = M20_iTOF_data(num_rows_per_roi, num_columns_per_roi, num_rois, num_rows_full_frame)
    device = M20_GPixel_device(num_rows_per_roi, num_columns_per_roi,
                               num_rois, num_rows_full_frame)
    data_viewer = M20_iTOF_viewer()
    ################
    # Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
    # UNCOMMENT ALL THE FOLLOWING LINES TO USE

    use_old_metadata_format = False
    fov_num_to_use = 0

    for idx, base in enumerate(range(START, END)):

        num_frames = int((N_ROIS-IGNORE.get(base, 90)) / num_rois)
        # The device processes 1 frame at a time

        cnt = (idx+OFFSET) * num_frames
        print('\n\nbase', base, 'count', cnt, 'idx', idx, '\n\n')

        file_name_base = raw_data_prepend + f'_0_{base:02}_'
        print('file name base', file_name_base, raw_data_folder)
        input_data_name, input_data_shape, perform_tap_add = (
            data_gen.load_sensor_data(raw_data_folder, file_name_base, num_frames, np.uint16,
                                      use_old_metadata_format, fov_num_to_use,
                                      ignore_num=IGNORE.get(base, 90))
        )
        configs['perform_tap_add'] = perform_tap_add

        ###########################################################################
        start_vector = data_gen.data["ROI_start_vector"]
        print(len(start_vector), [len(x) for x in start_vector])
        # Checking that we skipped the right number of frames in the beginning. These should all be the same.
        print('first entry', [x[0] for x in start_vector])
        print('last entry', [x[-1] for x in start_vector])
        if args.verbose:
            for idx, x in enumerate(start_vector[1::]):
                print(np.asarray(start_vector[idx-1], dtype=float)
                      - np.asarray(start_vector[idx], dtype=float))
            print([x for x in start_vector])

        input_data = data_gen.data[input_data_name]

        do_list = [0]
        #do_list = list(range(num_frames))

        if args.cal:
            # Process with phase calibration
            configs['phase_cal_use_time_array'] = True
            configs['phase_cal_time_array_path'] = data_folder / 'time_calibration_unbinned_fxp.npy'
            configs['phase_cal_rot0_path'] = None
            configs['phase_cal_rot1_path'] = None
            configs['phase_cal_int0_path'] = None
            configs['phase_cal_int1_path'] = None

            calcnt = []
            for i in range(num_frames):
                calcnt.append(cnt+i)
                if i not in do_list:
                    continue
                final_data = device(input_data=input_data[i],
                                    roi_start_vector=start_vector[i],
                                    configs=configs)
                save_file = (raw_data_folder
                             / 'calibrated_frames'
                             / f'calibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy')
                print(save_file)
                basefid = f'calibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy'
                np.save(raw_data_folder / 'calibrated_frames' / f'range_{basefid}',
                        device.dsp.data[final_data].flt)
                np.save(raw_data_folder / 'calibrated_frames' / f'phase_{basefid}',
                        device.dsp.data['phase_frame'].flt)

                if args.ply:
                    # Save a few PLY
                    data_viewer.assign_dict(device.dsp.data)
                    data_viewer.plot_3d_range(
                        data_folder / 'mapping_table_A.csv',
                        configs['binning'],
                        device.frame_start_row,
                        device.frame_stop_row,
                        range_array_name=final_data,
                        show_pointcloud=False,
                        save_pointcloud=True,
                        filename=(raw_data_folder
                                  / 'calibrated_frames'
                                  / f'range_calibrated_pointcloud_{base+OFFSET:02}_{i:03}.ply'),
                        max_rgb_range=10.0,
                        range_multiplicator=1,
                    )
                device.clear(force_clear=True)
            print('cal', calcnt)
        cnt = (idx+OFFSET) * num_frames
        if args.rot:
            # Process with phase calibration using precalculated rotation matrices
            configs['phase_cal_use_time_array'] = False
            configs['phase_cal_time_array_path'] = None
            configs['phase_cal_rot0_path'] = data_folder / 'rotation_array_freq0.bin'
            configs['phase_cal_rot1_path'] = data_folder / 'rotation_array_freq1.bin'
            configs['phase_cal_int0_path'] = data_folder / 'integer_skip_freq0.bin'
            configs['phase_cal_int1_path'] = data_folder / 'integer_skip_freq1.bin'

            calcnt = []
            for i in range(num_frames):
                calcnt.append(cnt+i)
                if i not in do_list:
                    continue
                final_data = device(input_data=input_data[i],
                                    roi_start_vector=start_vector[i],
                                    configs=configs)
                save_file = (raw_data_folder
                             / 'rot_calibrated_frames'
                             / f'rot_calibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy')
                print(save_file)
                basefid = f'rot_calibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy'
                np.save(raw_data_folder / 'rot_calibrated_frames' / f'range_{basefid}',
                        device.dsp.data[final_data].flt)
                np.save(raw_data_folder / 'rot_calibrated_frames' / f'phase_{basefid}',
                        device.dsp.data['phase_frame'].flt)

                if args.ply:
                    # Save a few PLY
                    data_viewer.assign_dict(device.dsp.data)
                    data_viewer.plot_3d_range(
                        data_folder / 'mapping_table_A.csv',
                        configs['binning'],
                        device.frame_start_row,
                        device.frame_stop_row,
                        range_array_name=final_data,
                        show_pointcloud=False,
                        save_pointcloud=True,
                        filename=(raw_data_folder
                                  / 'calibrated_frames'
                                  / f'range_calibrated_pointcloud_{base+OFFSET:02}_{i:03}.ply'),
                        max_rgb_range=10.0,
                        range_multiplicator=1,
                    )
                device.clear(force_clear=True)
            print('cal', calcnt)
        cnt = (idx+OFFSET) * num_frames
        if args.uncal:
            # Process without phase calibration
            configs['phase_calibration'] = False
            uncalcnt = []
            for i in range(num_frames):
                uncalcnt.append(cnt+i)
                if i not in do_list:
                    continue
                final_data = device(input_data=input_data[i],
                                    roi_start_vector=start_vector[i],
                                    configs=configs)
                save_file = (raw_data_folder
                             / 'uncalibrated_frames'
                             / f'uncalibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy')
                print(save_file)
                np.save(save_file, device.dsp.data[final_data].flt)
                basefid = f'uncalibrated_assembled_frame_{base+OFFSET:02}_{i:03}.npy'
                np.save(raw_data_folder / 'uncalibrated_frames' / f'range_{basefid}',
                        device.dsp.data[final_data].flt)
                np.save(raw_data_folder / 'uncalibrated_frames' / f'phase_{basefid}',
                        device.dsp.data['phase_frame'].flt)
                if args.ply:
                    # Save a few PLY
                    data_viewer.assign_dict(device.dsp.data)
                    data_viewer.plot_3d_range(
                        data_folder / 'mapping_table_A.csv',
                        configs['binning'],
                        device.frame_start_row,
                        device.frame_stop_row,
                        range_array_name=final_data,
                        show_pointcloud=False,
                        save_pointcloud=True,
                        filename=(raw_data_folder
                                  / 'uncalibrated_frames'
                                  / f'range_uncalibrated_pointcloud_{base+OFFSET:02}_{i:03}.ply'),
                        max_rgb_range=10.0,
                        range_multiplicator=1,
                    )
                device.clear(force_clear=True)
            print('uncalibrated', uncalcnt)


        up = np.load(raw_data_folder
                     / 'uncalibrated_frames'
                     / f'phase_uncalibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')
        ur = np.load(raw_data_folder
                     / 'uncalibrated_frames'
                     / f'range_uncalibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')

        cr = np.load(raw_data_folder
                     / 'calibrated_frames'
                     / f'range_calibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')
        cp = np.load(raw_data_folder
                     / 'calibrated_frames'
                     / f'phase_calibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')

        rr = np.load(raw_data_folder
                     / 'rot_calibrated_frames'
                     / f'range_rot_calibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')
        rp = np.load(raw_data_folder
                     / 'rot_calibrated_frames'
                     / f'phase_rot_calibrated_assembled_frame_{base+OFFSET:02}_{0:03}.npy')


        pcal = np.load(data_folder / 'time_calibration_unbinned.npy')

        fig, ax = plt.subplots(ncols=2, nrows=2)
        ax = ax.ravel()
        im = ax[0].imshow(up[0, ...])
        divider = make_axes_locatable(ax[0])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[1].imshow(cp[0, ...])
        divider = make_axes_locatable(ax[1])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[2].imshow(up[0, ...]-cp[0, ...])
        divider = make_axes_locatable(ax[2])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[3].imshow(up[0, ...]-cp[0, ...] - pcal * (1e9 / 8 / 1e9) / 3)
        divider = make_axes_locatable(ax[3])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')

        fig, ax = plt.subplots(ncols=2, nrows=2)
        ax = ax.ravel()
        im = ax[0].imshow(up[1, ...])
        divider = make_axes_locatable(ax[0])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[1].imshow(cp[1, ...])
        divider = make_axes_locatable(ax[1])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[2].imshow(up[1, ...]-cp[1, ...])
        divider = make_axes_locatable(ax[2])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[3].imshow(up[1, ...]-cp[1, ...] - pcal * (1e9 / 8 / 1e9) / 3)
        divider = make_axes_locatable(ax[3])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        fig.colorbar(im, cax=cax, orientation='vertical')


        fig, ax = plt.subplots(ncols=2, nrows=2)
        ax = ax.ravel()
        im = ax[0].imshow(ur)
        divider = make_axes_locatable(ax[0])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        ax[0].set_title('cal vs uncal comparison')
        im = ax[1].imshow(cr)
        divider = make_axes_locatable(ax[1])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[2].imshow(ur-cr)
        divider = make_axes_locatable(ax[2])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        fig.colorbar(im, cax=cax, orientation='vertical')

        fig, ax = plt.subplots(ncols=2, nrows=2)
        ax = ax.ravel()
        im = ax[0].imshow(cr)
        divider = make_axes_locatable(ax[0])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        ax[0].set_title('time vs rot cal range comparison')
        im = ax[1].imshow(rr)
        divider = make_axes_locatable(ax[1])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[2].imshow(cr-rr)
        divider = make_axes_locatable(ax[2])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        fig.colorbar(im, cax=cax, orientation='vertical')

        fig, ax = plt.subplots(ncols=2, nrows=2)
        ax = ax.ravel()
        im = ax[0].imshow(cp[0,...])
        divider = make_axes_locatable(ax[0])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        ax[0].set_title('time vs rot cal phase comparison')
        im = ax[1].imshow(rp[0,...])
        divider = make_axes_locatable(ax[1])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        c = fig.colorbar(im, cax=cax, orientation='vertical')
        im = ax[2].imshow(cp[0,...]-rp[0,...])
        divider = make_axes_locatable(ax[2])
        cax = divider.append_axes('right', size='5%', pad=0.1)
        fig.colorbar(im, cax=cax, orientation='vertical')


if __name__ == "__main__":
    args_ = parse_args(sys.argv[1:])
    main(args_)
    plt.show()
