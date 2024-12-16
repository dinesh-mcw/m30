"""
Experiment to see how saturation affects binning
"""

import numpy as np
from glob import glob
import os

def generate_fake_ROIs_with_random_int_values(num_rows, num_cols, num_rois, min_val, max_val):
    return np.random.randint(min_val, high=max_val, size=(3, 2, num_rows, num_cols, num_rois))

def load_ROI_data(num_rows, num_cols, num_rois, data_path, file_name_base):
    combined = np.zeros([3, 2, num_rows, num_cols, num_rois], dtype=np.uint16)
    fnames = glob(os.path.join(data_path, file_name_base + '*.bin'))
    for cnt in range(len(fnames)):
        raw_in = np.fromfile(fnames[cnt], dtype=np.uint16) / 16 # / 16 is to right shift by 4
        metadata = raw_in[:1920]
        combined[:, :, :, :, cnt] = np.reshape(raw_in[1920:], [3, 2, num_rows, num_cols], order='C')
    return combined

def bin_single_ROI(ROI, row_bin, col_bin):

    current_row_dim = ROI.shape[2]
    current_col_dim = ROI.shape[3]
    new_row_dim = int(current_row_dim / row_bin)
    new_col_dim = int(current_col_dim / col_bin)

    output_ROI_full = np.zeros([3, 2, new_row_dim, new_col_dim], dtype=np.uint64)
    output_ROI = np.zeros([3, 2, new_row_dim, new_col_dim], dtype=np.uint16)

    for comb in range(3):
        for freq in range(2):
            # https://scipython.com/blog/binning-a-2d-array-in-numpy/
            cp_ROI_full = np.squeeze(ROI[comb, freq, :, :]).reshape(new_row_dim, current_row_dim // new_row_dim, new_col_dim, current_col_dim // new_col_dim).astype(np.uint64)
            cp_ROI_full = cp_ROI_full.sum(-1).sum(1) # Sum with no constraint
            cp_ROI = np.clip(cp_ROI_full, a_min=0, a_max=(2**16)-1).astype(np.uint16)
            output_ROI[comb, freq, :, :] = cp_ROI
            output_ROI_full[comb, freq, :, :] = cp_ROI_full

    return output_ROI, output_ROI_full



if __name__ == "__main__":
    # Parameters
    path = '../../cobra_raw2depth_data/synth_circles_fpga_tap_accum_80_rois/'
    file_name_base = 'synth_80roi_0'
    max_num_cols = 640
    max_num_rows = 480
    num_rows_per_roi = 20
    num_rois = 1

    row_binning = np.asarray([1, 2, 4, 5, 10])
    col_binning = np.asarray([2, 4, 5, 8, 10, 16, 20, 32, 64, 80])

    # Load data (The loaded data is 16 bits wide with 12 bits loading)
    #combined_data_ROIs = load_ROI_data(num_rows_per_roi, max_num_cols, num_rois, path, file_name_base)
    combined_data_ROIs = generate_fake_ROIs_with_random_int_values(num_rows_per_roi, max_num_cols, num_rois, 0xFFF, 0xFFF+1).astype(np.uint16)
    print('Done loading data.')

    # Bin the data
    num_possibilities = len(row_binning) * len(col_binning) # only use 1 ROI to begin with (the others are ignored)
    binned_ROIs_fxp = []
    binned_ROIs_full = []
    binning = []
    for row_bin in row_binning:
        for col_bin in col_binning:
            (binned_fxp, binned_full) = bin_single_ROI(np.copy(np.squeeze(combined_data_ROIs[:, :, :, :, 0])), row_bin, col_bin)
            binned_ROIs_fxp.append(binned_fxp)
            binned_ROIs_full.append(binned_full)
            binning.append((row_bin, col_bin))
    print('Done binning data.')


    # Analyze the data to understand saturation
    for i in range(len(binned_ROIs_fxp)):
        if not np.any(np.subtract(binned_ROIs_fxp[i], binned_ROIs_full[i])):
            print("Combination ("+str(binning[i][0])+","+str(binning[i][1])+") works.")
