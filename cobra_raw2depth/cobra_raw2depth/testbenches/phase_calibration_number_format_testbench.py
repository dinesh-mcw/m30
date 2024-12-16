import argparse
import glob
import sys
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
from mpl_toolkits.mplot3d import Axes3D

from development.src.M20_iTOF_data_generator import M20_iTOF_data
from development.src.M20_GPixel import M20_GPixel_device

import scipy as sp
import scipy.linalg

"""Simple testbench to use M20-GPixel with imported data and save the SNR-voted frames for sharing with customers
"""

data_folder = Path(Path.home(), 'pixel-phase')
raw_data_folder = data_folder / 'phase_scene'
raw_data_prepend = 'pixel_phase_scene'
cal_path = raw_data_folder / 'calibrated_frames'
uncal_path = raw_data_folder / 'uncalibrated_frames'

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
    return parser.parse_args(argv)


def fit_plane(data):
    # Fit plane
    y = np.arange(data.shape[0])
    x = np.arange(data.shape[1])
    X, Y = np.meshgrid(x, y)#, indexing='xy')
    XX = X.flatten()
    YY = Y.flatten()

    # = np.c_[data[:,0], data[:,1], np.ones(data.shape[0])]
    A = np.c_[XX, YY, np.ones(data.flatten().shape[0])]
    C, _, _, _ = sp.linalg.lstsq(A, data.flatten())
    Z = C[0]*X + C[1]*Y + C[2]
    return X, Y, Z


def get_frames(pglob):
    fids = sorted(glob.glob(str(pglob)))[0:1]
    avgdata = None
    data = None
    for idx, f in enumerate(fids):
        d = np.load(f)
        if avgdata is None:
            avgdata = d
        else:
            avgdata += d
        if data is None:
            data = np.zeros((*d.shape, len(fids)))
        data[..., idx] = d
    return data, avgdata / len(fids)


def get_std_value(path, name):
    # Trim
    l = 125 #130
    r = 150 #160
    u = 110 #100
    d = 130 #140

    dat, avg = get_frames(path / name)

    tdat = dat[u:d, l:r, :]
    tavg = avg[u:d, l:r]
    x, y, z = fit_plane(tavg)
    zeroed = tdat - z[..., None]
    print('std',  np.std(zeroed))

    return np.std(zeroed)


# 2: Process
configs = {'perform_tap_add': True,
           'correct_strips': False,
           'phase_calibration': True,
           'phase_array_time_units': True,
           'phase_calibration_array_path': data_folder / 'time_calibration_unbinned.npy',
           'phase_cal_fxp_format': (True, 4, 2),
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
    ################
    # Import data recorded from real sensor (Uncomment the next 4 lines if you want to load data from sensor)
    # UNCOMMENT ALL THE FOLLOWING LINES TO USE

    use_old_metadata_format = False
    fov_num_to_use = 0

    lbits = []
    lstd = []

    lbits.append(0)
    lstd.append(get_std_value(uncal_path, 'range*uncalibrated*.npy'))

    for fxpf in [(True, 4, 2),
                 (True, 6, 4),
                 (True, 8, 6),
                 (True, 10, 8),
                 (True, 12, 10),
                 (True, 14, 12),
                 (True, 16, 14),
                 ]:

        lbits.append(fxpf[1])
        configs['phase_cal_fxp_format'] = fxpf

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
                    print(np.asarray(start_vector[idx-1], dtype=float) - np.asarray(start_vector[idx], dtype=float))
                print([x for x in start_vector])

            input_data = data_gen.data[input_data_name]

            do_list = [0]
            do_list = list(range(num_frames))


            # Process with phase calibration
            calcnt = []
            #for i in range(num_frames):
            for i in range(10):
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
                np.save(raw_data_folder / 'calibrated_frames' / f'range_{fxpf[0]}_{fxpf[1]}_{fxpf[2]}_{basefid}',
                        device.dsp.data[final_data].flt)
                #np.save(raw_data_folder / 'calibrated_frames' / f'phase_{basefid}',
                #        device.dsp.data['phase_frame'].flt)

                device.clear(force_clear=True)
            print('cal', calcnt)

        lstd.append(get_std_value(cal_path, f'range_{fxpf[0]}_{fxpf[1]}_{fxpf[2]}*calibrated*.npy'))

    fig, ax = plt.subplots(figsize=(6, 3))
    ax.plot(lbits, lstd, '-o')
    ax.set_xlabel('Number of bits')
    ax.set_ylabel('std')
    fig.tight_layout()

    fig, ax = plt.subplots(figsize=(6, 3))
    ax.plot(lbits[1:], [x / lstd[0] for x in lstd[1:]], '-o')
    ax.set_xlabel('Number of bits')
    ax.set_ylabel('Fractional benefit')
    fig.tight_layout()

    print([x / lstd[0] for x in lstd[1:]])


if __name__ == "__main__":
    args_ = parse_args(sys.argv[1:])
    main(args_)
    plt.show()
