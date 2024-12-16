'''
file: m30_verification.py

Top-level python code that executes the mathematical operations for RawToDepth.
Used for verification against the C++ code.

Copyright 2023 (C) Lumotive, Inc. All rights reserved.
'''
import os
from glob import glob
import numpy as np
from m30_dsp import tap_rotation, snr_vote, set_active_rows, fill_missing_rows, bin, calculatePhase, smooth_raw, calculate_phase_smooth
from m30_dsp import compute_whole_frame_range, min_max_recursive, median_filter_plus, nearest_neighbor
from m30_dsp import get_range, get_signal, get_background, get_snr, bin1d, hdr, computeSnrSquaredWeights, median1d, gauss
import M30Metadata as md
from temperature_calibration import TemperatureCalibration
import json

def process_roi(roi, metadata, fov_idx, dsp_config, results) :
    if 'roi_idx' not in results :
        results['roi_idx'] = -1
    results['roi_idx'] += 1

    if 'process_roi_indices' in dsp_config :
        if results['roi_idx'] not in dsp_config['process_roi_indices'] :
            return
        else :
            print(f'Processing ROI index {results["roi_idx"]}')
    
    if 'range' in results:
        del results['range']

    if md.getStripeModeEnabled(metadata, fov_idx) :
        process_roi_stripe(roi, metadata, fov_idx, dsp_config,results)
        return
    process_roi_grid(roi, metadata, fov_idx, dsp_config, results)


def collapseStripes(raw_roi0, raw_roi1, binned_roi0, binned_roi1, metadata, dsp_config, fov_idx) :
    binned_roi0.fill(0)
    binned_roi1.fill(0)

    roi_width = md.getRoiNumColumns(metadata) * md.NUM_GPIXEL_PHASES
    roi_height = md.getRoiNumRows(metadata)
    vertically_collapsed_raw_roi0 = np.zeros(roi_width, dtype=np.float32)
    vertically_collapsed_raw_roi1 = np.zeros(roi_width, dtype=np.float32)

    assert(raw_roi1.size == raw_roi0.size)
    weights = np.zeros(raw_roi0.size, dtype=np.float32)
    weights_one_column = np.zeros(0, dtype=np.float32)

    # Choose the window used for summing along each column.
    # The window is programmable at test time via the dsp_config settings.
    if 'stripe_window' in dsp_config :
        if dsp_config['stripe_window']['window'] == 'rect' :
            weights_one_column = np.ones(roi_height, dtype=np.float32)
            number_of_summed_values = np.sum(weights_one_column).astype(np.float32)
        elif dsp_config['stripe_window']['window'] == 'snr-weighted' :
            weights = computeSnrSquaredWeights(raw_roi0, raw_roi1, roi_height, roi_width)
            number_of_summed_values = np.sum(gauss(roi_height, md.STRIPE_DEFAULT_GAUSSIAN_STD).astype(np.float32))
        elif dsp_config['stripe_window']['window'] == 'Gaussian' :
            weights_one_column = gauss(roi_height, dsp_config['stripe_window']['std']).astype(np.float32)
            number_of_summed_values = np.sum(weights_one_column).astype(np.float32)
        else :
            print(f'stripe mode window in dsp_config set to invalid value. Valid values are rect, snr-weighted, Gaussian')
            exit(-1)
    else :
        if md.getStripeModeSnrWeightedSum(metadata, fov_idx) :
            weights = computeSnrSquaredWeights(raw_roi0, raw_roi1, roi_height, roi_width)
            # Choosing to set number_of_summed_values for snr-weight to be the same as Gaussian mode, under the assumption that,
            # on average, the snr-weighted window tends to be approximately Gaussian-shaped. 
            number_of_summed_values = np.sum(gauss(roi_height, md.STRIPE_DEFAULT_GAUSSIAN_STD).astype(np.float32))
            pass
        elif md.getStripeModeRectSum(metadata, fov_idx) :
            weights_one_column = np.ones(roi_height, dtype=np.float32)
            number_of_summed_values = np.sum(weights_one_column).astype(np.float32)
        else : #default to gaussian.
            weights_one_column = np.float32(gauss(roi_height, md.STRIPE_DEFAULT_GAUSSIAN_STD))
            number_of_summed_values = np.sum(weights_one_column).astype(np.float32)
    
    if weights_one_column.size > 0 :
        for col_idx in range(roi_width) :
            weights[col_idx::roi_width] = weights_one_column
    

    raw_roi0 *= weights
    raw_roi1 *= weights

    for row in range(md.getRoiNumRows(metadata)) :
        vertically_collapsed_raw_roi0[:] += raw_roi0[roi_width*row : roi_width*(row+1)]
        vertically_collapsed_raw_roi1[:] += raw_roi1[roi_width*row : roi_width*(row+1)]

    binning = md.getBinning(metadata, fov_idx)
    binned_size = [1, roi_width // binning]
    unbinned_size = [1, roi_width]
    bin1d(vertically_collapsed_raw_roi0, binned_roi0, binning, unbinned_size, binned_size)
    bin1d(vertically_collapsed_raw_roi1, binned_roi1, binning, unbinned_size, binned_size)
    
    return number_of_summed_values

def separateStripeFrequencies(tap_rotated_roi, raw_roi, metadata) :
    raw_roi_size = tap_rotated_roi.size // 2
    raw_roi[0][:] = tap_rotated_roi[0:raw_roi_size]
    raw_roi[1][:] = tap_rotated_roi[raw_roi_size:2*raw_roi_size]

def process_roi_stripe(raw_roi, metadata, fov_idx, dsp_config, results) :

    if 'TemperatureCalibration' not in results:
        results['TemperatureCalibration'] = TemperatureCalibration()
    
    skip = hdr(raw_roi, metadata, fov_idx, results) 

    results['size'] = [1, md.getRoiNumColumns(metadata)/md.getBinning(metadata, fov_idx)]
    results['mapping_table_start'] = [int(2*md.getRoiStartRow(metadata) + (2*md.getRoiNumRows(metadata))/2 - 1),
                                     int(2*md.ROI_START_COLUMN + md.getBinning(metadata, fov_idx) - 1)]
    results['mapping_table_step'] = [int(2*md.getBinning(metadata, fov_idx)), int(2*md.getBinning(metadata, fov_idx))]
    results['fov_top_left'] = [int((md.getRoiStartRow(metadata) + md.getRoiNumRows(metadata)/2)//md.getBinning(metadata, fov_idx)), int(md.ROI_START_COLUMN//md.getBinning(metadata, fov_idx))]
    results['fov_step'] = [int(md.getBinning(metadata, fov_idx)), int(md.getBinning(metadata, fov_idx))]
    
    if skip :
        return

    results['TemperatureCalibration'].set_adc_values(metadata, fov_idx)

    results['tap_rotated_roi'] = np.zeros(md.NUM_GPIXEL_FREQUENCIES*md.getRoiNumRows(metadata) * md.getRoiNumColumns(metadata) * md.NUM_GPIXEL_PHASES, dtype=np.float32)
    tap_rotation(raw_roi, results['tap_rotated_roi'], metadata) 

    results['raw_roi'] = [ np.zeros(md.getRoiNumRows(metadata)*md.getRoiNumColumns(metadata)*md.NUM_GPIXEL_PHASES, dtype=np.float32),
                           np.zeros(md.getRoiNumRows(metadata)*md.getRoiNumColumns(metadata)*md.NUM_GPIXEL_PHASES, dtype=np.float32)]
    separateStripeFrequencies(results['tap_rotated_roi'], results['raw_roi'], metadata)
    
    binned_width = md.getRoiNumColumns(metadata) // md.getBinning(metadata, fov_idx)
    results['raw_roi_binned'] = [np.zeros(binned_width*md.NUM_GPIXEL_PHASES, dtype=np.float32),
                                 np.zeros(binned_width*md.NUM_GPIXEL_PHASES, dtype=np.float32)]
    
    number_of_summed_values = collapseStripes(results['raw_roi'][0], results['raw_roi'][1], results['raw_roi_binned'][0], results['raw_roi_binned'][1], metadata, dsp_config, fov_idx)

    results['phase_fov'] = [np.zeros(binned_width, dtype=np.float32),
                            np.zeros(binned_width, dtype=np.float32)]

    results['signal_fov'] = np.zeros(binned_width, dtype=np.float32)
    results['snr_fov'] = np.zeros(binned_width, dtype=np.float32)
    results['background_fov'] = np.zeros(binned_width, dtype=np.float32)

    number_of_summed_values = number_of_summed_values*md.getBinning(metadata, fov_idx)
    calculatePhase(results['raw_roi_binned'][0], results['phase_fov'][0], results['signal_fov'], results['snr_fov'], results['background_fov'], number_of_summed_values)
    calculatePhase(results['raw_roi_binned'][1], results['phase_fov'][1], results['signal_fov'], results['snr_fov'], results['background_fov'], number_of_summed_values)
    
    results['m'] = np.zeros(binned_width, dtype=np.float32)
    results['min_max_mask'] = np.zeros(binned_width, dtype=np.float32)
    fs = [md.getFs]
    pre_median_range = np.zeros(binned_width, dtype=np.float32)
    compute_whole_frame_range(results['phase_fov'], results['phase_fov'], pre_median_range, md.getFs(metadata), md.getFsInt(metadata), results['m'])

    pre_median_range -= results['TemperatureCalibration'].getRangeOffsetTemperature()
    pre_median_range[pre_median_range<0] = 0
    pre_median_range = pre_median_range % md.getMaxUnambiguousRange(metadata)

    results['range'] = np.zeros(binned_width, dtype=np.float32)
    if md.getStripeModeRangeMedianEnabled(metadata, fov_idx) : # pick this up from the metadata
        median1d(pre_median_range, results['range'], md.getBinning(metadata, fov_idx))
    else :
        results['range'][:] = pre_median_range.copy()
    results['roi_processing_completed'] = True


# Once per ROI
def process_whole_frame_stripe(fov_idx, results, metadata, dsp_config) :
    results['fov_segment'] = {}
    results['fov_segment']['range'] = np.zeros(md.getBinnedFovWidth(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['snr'] = np.zeros(md.getBinnedFovWidth(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['signal'] = np.zeros(md.getBinnedFovWidth(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['background'] = np.zeros(md.getBinnedFovWidth(metadata, fov_idx), dtype=np.uint16)

    get_range(results['range'], results['snr_fov'], results['min_max_mask'], results['fov_segment']['range'], metadata, fov_idx)
    get_signal(results['signal_fov'], results['fov_segment']['signal'])
    get_background(results['background_fov'], results['fov_segment']['background'])
    get_snr(results['snr_fov'], results['fov_segment']['snr'])

    frame_idx = results['frame_idx']
    tag = dsp_config["tag"]
    output_dir = dsp_config['output_dir']
    results['fov_segment']['range'].tofile(os.path.join(output_dir,      f'python_range_float_as_short_{tag}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['signal'].tofile(os.path.join(output_dir,     f'python_signal_float_as_short_{tag}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['background'].tofile(os.path.join(output_dir, f'python_bkg_float_as_short_{tag}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['snr'].tofile(os.path.join(output_dir,        f'python_snr_float_as_short_{tag}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    dumpGridCoords(dsp_config, fov_idx, frame_idx, results)


def process_roi_grid(roi, metadata, fov_idx, dsp_config, results) :

    # print(f'{md.currentFile(__file__)} - Processing ROI {results["roi_counter"]}')
    if 'TemperatureCalibration' not in results:
        results['TemperatureCalibration'] = TemperatureCalibration()

    # Note: grid mode hdr not implemented in python.
    #roi = hdr(roi, metadata, fov_idx, results)
    
    results['tap_rotated_roi'] = np.zeros(2*md.getRoiNumRows(metadata) * md.getRoiNumColumns(metadata) * md.NUM_GPIXEL_PHASES, dtype=np.float32)
    tap_rotation(roi, results['tap_rotated_roi'], metadata)

    if md.getFirstRoi(metadata, fov_idx) :
        results['prebinned_raw_fov'] = [np.zeros(md.getRawFovSize(metadata, fov_idx), dtype=np.float32),
                                        np.zeros(md.getRawFovSize(metadata, fov_idx), dtype=np.float32)]
        results['snr_vote_fov'] = np.zeros(md.getFovSize(metadata, fov_idx), dtype=np.float32)
        results['active_rows'] = np.zeros(md.getFovNumRows(metadata, fov_idx), dtype=bool)
        results['size'] = [int(md.getBinnedFovHeight(metadata, fov_idx)), int(md.getBinnedFovWidth(metadata, fov_idx))]
        results['mapping_table_start'] = [int(2*md.getFovStartRow(metadata, fov_idx) + md.getBinning(metadata, fov_idx) - 1),
                                           int(2*md.getFovStartColumn(metadata, fov_idx) + md.getBinning(metadata, fov_idx) - 1)]
        results['mapping_table_step'] = [int(2*md.getBinning(metadata, fov_idx)), int(2*md.getBinning(metadata, fov_idx))]
        results['fov_top_left'] = [int(md.getFovStartRow(metadata, fov_idx)//md.getBinning(metadata, fov_idx)), int(md.getFovStartColumn(metadata, fov_idx)//md.getBinning(metadata, fov_idx))]
        results['fov_step'] = [int(md.getBinning(metadata, fov_idx)), int(md.getBinning(metadata, fov_idx))]

    if 'prebinned_raw_fov' not in results :
        print(f'{md.currentFile(__file__)} - Ignoring ROI: first_roi not yet received.')
        return
    
    results['TemperatureCalibration'].set_adc_values(metadata, fov_idx)
    
    snr_vote(results['tap_rotated_roi'], results['prebinned_raw_fov'], results['snr_vote_fov'], fov_idx, metadata)
    set_active_rows(fov_idx, results['active_rows'], metadata)

    if False and dsp_config['output_intermediate_results'] :
        results[fov_idx]['tap_rotated_roi'].tofile(os.path.join(dsp_config["output_dir"], f'python_tap_rotated_roi{roi_idx}_fov{fov_idx}_float_as_float_pyr2dver.bin'))

    results['roi_processing_completed'] = True
    return

def process_whole_frame(fov_idx, results, metadata, dsp_config) :
    if not results['roi_processing_completed'] :
        if not md.getStripeModeEnabled(metadata, fov_idx) : # Don't want to print this warning in stripe mode -- every retake has this condition.
            print(f'Skipping whole-frame processing. Valid first ROI not yet received.')
        return
    
    if 'frame_idx' not in results :
        results['frame_idx'] = -1
    results['frame_idx'] += 1

    if 'process_frame_indices' in dsp_config:
        if results['frame_idx'] not in dsp_config['process_frame_indices'] :
            return
        else :
            print(f'Processing frame {results["frame_idx"]}')

    if md.getStripeModeEnabled(metadata, fov_idx) :
        process_whole_frame_stripe(fov_idx, results, metadata, dsp_config)
        return
    process_whole_frame_grid(fov_idx, results, metadata, dsp_config)

def process_whole_frame_grid(fov_idx, results, metadata, dsp_config) :

    frame_idx = results['frame_idx']

    print(f'{md.currentFile(__file__)} - Filling missing rows for FOV {fov_idx} and frame {frame_idx}')
    results['row_filled_raw_fov'] = [np.zeros(md.getRawFovSize(metadata, fov_idx), dtype=np.float32),
                                     np.zeros(md.getRawFovSize(metadata, fov_idx), dtype=np.float32)]
    fill_missing_rows(fov_idx, results['prebinned_raw_fov'][0], results['row_filled_raw_fov'][0], results['active_rows'], metadata)
    fill_missing_rows(fov_idx, results['prebinned_raw_fov'][1], results['row_filled_raw_fov'][1], results['active_rows'], metadata)

    results['binned_raw_fov'] = [np.zeros(md.getBinnedRawFovSize(metadata, fov_idx), dtype=np.float32),
                                 np.zeros(md.getBinnedRawFovSize(metadata, fov_idx), dtype=np.float32)]
    
    print(f'{md.currentFile(__file__)} - Binning for FOV {fov_idx} and frame {frame_idx}')
    binning = md.getBinning(metadata, fov_idx)
    unbinned_size = (md.getRawFovNumRows(metadata, fov_idx), md.getRawFovNumColumns(metadata, fov_idx))
    binned_size = (md.getBinnedRawFovNumRows(metadata, fov_idx), md.getBinnedRawFovNumColumns(metadata, fov_idx))
    bin(results['row_filled_raw_fov'][0], results['binned_raw_fov'][0], binning, unbinned_size, binned_size)
    bin(results['row_filled_raw_fov'][1], results['binned_raw_fov'][1], binning, unbinned_size, binned_size)

    results['phase_fov'] = [np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32), 
                            np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)]
    results['signal_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    results['snr_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    results['background_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    
    print(f'{md.currentFile(__file__)} - Calculating phase for FOV {fov_idx} and frame {frame_idx}')
    number_of_summed_values = md.getBinning(metadata, fov_idx)*md.getBinning(metadata, fov_idx)
    calculatePhase(results['binned_raw_fov'][0], results['phase_fov'][0], results['signal_fov'], results['snr_fov'], results['background_fov'], number_of_summed_values)
    calculatePhase(results['binned_raw_fov'][1], results['phase_fov'][1], results['signal_fov'], results['snr_fov'], results['background_fov'], number_of_summed_values)

    results['smoothed_raw_fov'] = [np.zeros(md.getBinnedRawFovSize(metadata, fov_idx), dtype=np.float32), 
                                   np.zeros(md.getBinnedRawFovSize(metadata, fov_idx), dtype=np.float32)]
    
    print(f'{md.currentFile(__file__)} - Smoothing raw data for FOV {fov_idx} and frame {frame_idx}')
    fov_size = [md.getBinnedFovHeight(metadata, fov_idx), md.getBinnedFovWidth(metadata, fov_idx)]
    smooth_raw(results['binned_raw_fov'][0], results['smoothed_raw_fov'][0], fov_size, md.getSmoothingFilterSize(metadata, fov_idx))
    smooth_raw(results['binned_raw_fov'][1], results['smoothed_raw_fov'][1], fov_size, md.getSmoothingFilterSize(metadata, fov_idx))

    print(f'{md.currentFile(__file__)} - Calculating smoothed phase for FOV {fov_idx} and frame {frame_idx}')
    results['corrected_phase_fov'] = [np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32),
                                  np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)]
    results['smoothed_phase_fov'] = [np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32),
                                 np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)]
    calculate_phase_smooth(results['smoothed_raw_fov'][0], results['smoothed_phase_fov'][0], 
                           results['phase_fov'][0], results['corrected_phase_fov'][0])
    calculate_phase_smooth(results['smoothed_raw_fov'][1], results['smoothed_phase_fov'][1], 
                           results['phase_fov'][1], results['corrected_phase_fov'][1])
    
    results['range_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    results['m_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
 
    print(f'{md.currentFile(__file__)} - Computing whole frame range for FOV {fov_idx} and frame {frame_idx}')
    gcf = md.getGcf(metadata)
    freqIdcs = [md.getF0ModulationIndex(metadata), md.getF1ModulationIndex(metadata)]
    compute_whole_frame_range(results['smoothed_phase_fov'], results['corrected_phase_fov'], results['range_fov'], md.getFs(metadata), md.getFsInt(metadata),
                              results['m_fov'])
    
    results['min_max_mask'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    if md.getPerformMinMaxFilter(metadata, fov_idx) :
        min_mask_filter_size = [3,3] #practical output from the two smoothing filters as specified in RawToDepthV2_float.cpp
        min_max_recursive(results['m_fov'], results['min_max_mask'], min_mask_filter_size, fov_size, 1)

    results['median_filtered_range_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    median_filter_plus(results['range_fov'], results['median_filtered_range_fov'], fov_size, md.getBinning(metadata, fov_idx), md.getPerformGhostMedian(metadata, fov_idx))

    results['nn_filtered_range_fov'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.float32)
    nearest_neighbor(results['median_filtered_range_fov'], results['nn_filtered_range_fov'], fov_size, md.getNearestNeighborFilterLevel(metadata, fov_idx))

    results['nn_filtered_range_fov'] -= results['TemperatureCalibration'].getRangeOffsetTemperature()
    results['nn_filtered_range_fov'][results['nn_filtered_range_fov']<0] = 0
    results['nn_filtered_range_fov'] = results['nn_filtered_range_fov'] % md.getMaxUnambiguousRange(metadata)

    # Convert the results to output format.
    results['fov_segment'] = {}
    results['fov_segment']['range'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['snr'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['signal'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.uint16)
    results['fov_segment']['background'] = np.zeros(md.getBinnedFovSize(metadata, fov_idx), dtype=np.uint16)

    print(f'{md.currentFile(__file__)} - Converting components to output format for FOV {fov_idx} and frame {frame_idx}')
    get_range(results['nn_filtered_range_fov'], results['snr_fov'], results['min_max_mask'], results['fov_segment']['range'], metadata, fov_idx)
    get_signal(results['signal_fov'], results['fov_segment']['signal'])
    get_background(results['background_fov'], results['fov_segment']['background'])
    get_snr(results['snr_fov'], results['fov_segment']['snr'])

    if dsp_config['output_intermediate_results'] :
        results['active_rows'].astype(np.float32).tofile(os.path.join(dsp_config["output_dir"], f'python_active_rows_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['snr_vote_fov'].tofile(os.path.join(dsp_config["output_dir"], f'python_snr_vote_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['row_filled_raw_fov'][0].tofile(os.path.join(dsp_config["output_dir"], f'python_row_filled_raw_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['row_filled_raw_fov'][1].tofile(os.path.join(dsp_config["output_dir"], f'python_row_filled_raw_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['binned_raw_fov'][0].tofile(os.path.join(dsp_config["output_dir"], f'python_binned_raw_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['binned_raw_fov'][1].tofile(os.path.join(dsp_config["output_dir"], f'python_binned_raw_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['phase_fov'][0].tofile(os.path.join(dsp_config["output_dir"], f'python_phase_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['phase_fov'][1].tofile(os.path.join(dsp_config["output_dir"], f'python_phase_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['signal_fov'].tofile(os.path.join(dsp_config["output_dir"], f'python_signal_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['snr_fov'].tofile(os.path.join(dsp_config["output_dir"], f'python_snr_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['background_fov'].tofile(os.path.join(dsp_config["output_dir"], f'python_background_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['smoothed_raw_fov'][0].tofile(os.path.join(dsp_config["output_dir"], f'python_smoothed_raw_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['smoothed_raw_fov'][1].tofile(os.path.join(dsp_config["output_dir"], f'python_smoothed_raw_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['smoothed_phase_fov'][0].tofile(os.path.join(dsp_config['output_dir'], f'python_smoothed_phase_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['smoothed_phase_fov'][1].tofile(os.path.join(dsp_config['output_dir'], f'python_smoothed_phase_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['corrected_phase_fov'][0].tofile(os.path.join(dsp_config['output_dir'], f'python_corrected_phase_f0_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['corrected_phase_fov'][1].tofile(os.path.join(dsp_config['output_dir'], f'python_corrected_phase_f1_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['range_fov'].tofile(os.path.join(dsp_config['output_dir'], f'python_range_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['m_fov'].tofile(os.path.join(dsp_config['output_dir'], f'python_m_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['min_max_mask'].tofile(os.path.join(dsp_config['output_dir'], f'python_min_max_mask_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['median_filtered_range_fov'].tofile(os.path.join(dsp_config['output_dir'], f'python_median_filtered_range_fov{fov_idx}_float_as_float_pyr2dver.bin'))
        results['nn_filtered_range_fov'].tofile(os.path.join(dsp_config['output_dir'], f'python_nn_filtered_range_fov{fov_idx}_float_as_float_pyr2dver.bin'))
    print(f'{md.currentFile(__file__)} - Writing {dsp_config["tag"]} results of size {results["fov_segment"]["range"].size} to {dsp_config["output_dir"]} for FOV {fov_idx}')
    results['fov_segment']['range'].tofile(os.path.join(dsp_config['output_dir'],           f'python_range_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['signal'].tofile(os.path.join(dsp_config['output_dir'],         f'python_signal_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['background'].tofile(os.path.join(dsp_config['output_dir'],        f'python_bkg_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    results['fov_segment']['snr'].tofile(os.path.join(dsp_config['output_dir'],               f'python_snr_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.bin'))
    dumpGridCoords(dsp_config, fov_idx, frame_idx, results)

def dumpGridCoords(dsp_config, fov_idx, frame_idx, results) :
    with open(os.path.join(dsp_config['output_dir'], f'python_coords_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.json'), "w") as jsonfile :
        jsonfile.write(
            json.dumps({
                "size" : results["size"],
                "topLeft" : results["mapping_table_start"],
                "step" : results["mapping_table_step"],
                "fovTopLeft" : results["fov_top_left"],
                "fovStep" : results["fov_step"]
            }, indent=4)
        )
    with open(os.path.join(dsp_config['output_dir'], f'python_dsp_config_float_as_short_{dsp_config["tag"]}_fov{fov_idx}_frame{frame_idx:04}.json'), "w") as jsonfile :
        jsonfile.write(
            json.dumps(dsp_config, indent=4)
        )
    pass

def process_rois(dsp_config) :

    print(f'Python RawToDepth numerical computation on directory {dsp_config["input_dir"]}')
    print(f'dsp_config for this processing set:')
    for key, value in dsp_config.items() :
        print(f'\t{key}: {value}')
    rois, metadatas = load_rois(dsp_config)
    results = [dict() for x in range(8)]

    print(f'Found {len(rois)} ROIs in the input dir')
    if len(rois) == 0 :
        print(f'No files in the input directory {dsp_config["input_dir"]}')
        exit(-1)

    md.printMetadata(metadatas[0])

    for roi_idx, (roi, metadata) in enumerate(zip(rois, metadatas)) :
        for fov_idx in md.getActiveFovs(metadata) :
            if 'roi_counter' not in results[fov_idx] :
                results[fov_idx]['roi_counter'] = 0
            results[fov_idx]['roi_counter'] += 1
            results[fov_idx]['roi_processing_completed'] = False
            process_roi(roi, metadata, fov_idx, dsp_config, results[fov_idx])

            if (md.getFrameCompleted(metadata, fov_idx)) :
                process_whole_frame(fov_idx, results[fov_idx], metadata, dsp_config)

    return results

def customize_metadata(metadata, dsp_config) :
    for fov_idx in md.getActiveFovs(metadata) :
        if 'set_snr_thresh' in dsp_config :
            md.setSnrThresh(metadata, fov_idx, dsp_config['set_snr_thresh'][fov_idx])
        if 'enable_stripe_median' in dsp_config :
            md.enableStripeModeRangeMedian(metadata, fov_idx)
    return metadata
    pass

def load_rois(dsp_config) :
    dir_path = dsp_config['input_dir']
    p = glob(os.path.join(dir_path, '*.bin'))
    fnames = sorted(p)
    
    print(f'{md.currentFile(__file__)} - Processing {len(fnames)} files in {dir_path}')

    rois = []
    metadatas = []
    for fname in fnames :
        raw_roi = np.fromfile(fname, dtype=np.uint16)
        metadata = raw_roi[0:md.MD_ROW_SHORTS] >> md.MD_SHIFT_BY
        metadata = customize_metadata(metadata, dsp_config)
        roi = ((raw_roi[md.MD_ROW_SHORTS:] & md.getRawPixelMask(metadata)) >> md.INPUT_RAW_SHIFT).astype(np.float32) # input scaling by right-shift 1. This is historical.

        expected_roi_length = md.getRoiNumRows(metadata) * md.getRoiNumColumns(metadata) * md.getNumFrequencies(metadata) * md.getNumPermutations(metadata) * md.getNumPhases(metadata)
        assert(roi.size == expected_roi_length)
        rois.append(roi)
        metadatas.append(metadata)
    return rois, metadatas

