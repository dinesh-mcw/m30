'''
file: m30_execute_python_verification.py

Python implementation of the Digital Signal Processing operations in RawToDepth.
Used for verification against the C++ code.

Copyright 2023 (C) Lumotive, Inc. All rights reserved.
'''

import numpy as np
import M30Metadata as md
from math import sqrt, modf, floor, ceil, exp
# import scipy.ndimage as nd

def gauss(M, std) :
    coeffs = np.zeros(M, dtype=np.float32)
    for x, idx in zip((np.array(list(range(M))) - (M-1)/2), range(M)) :
        h = M/2
        f = np.float32(exp(- (x*x)/(2*std*std)))
        coeffs[idx] = f
    return coeffs


def transpose(a, a_width) :
    b = a.copy()

    a_height = a.size//a_width
    for col in range(a_width) :
        for row in range(a_height) :
            a[row + a_height*col] = b[col + a_width*row]

#NOTE: >> INPUT_RAW_SHIFT happens during file reading in the python code. It happens during HDR in the c++ code.
# Perform HDR functionality. Return True if this ROI is to be skipped.
def hdr(roi, metadata, fov_idx, results) :
    
    if 'previous_roi_was_corrected' not in results :
        results['previous_roi_was_corrected'] = False
    
    # HDR is disabled, therefore leave the input roi unchanged.
    if md.isHdrDisabled(metadata) : 
        results['previous_roi_was_corrected'] = False
        return False
    
    # Invalid condition, though this can happen at startup if there are missing ROIs.
    # The very first ROI is marked as an HDR retake. We will skip it.
    if 'roi_prev' not in results and md.wasPreviousRoiSaturated(metadata) :
        results['previous_roi_was_corrected'] = False
        return True

    # Either this ROI is the first one ever, or the previous ROI was a retake.
    # Store this ROI into the previous buffer and skip processing on it.
    # Add an ROI of latency to wait to see if the next ROI is a retake.
    # (if 'roi_prev' is not in results, then this is the first ever ROI)
    if results['previous_roi_was_corrected'] or 'roi_prev' not in results or results['roi_prev'].shape != roi.shape:
        results['previous_roi_was_corrected'] = False
        if 'roi_prev' not in results or results['roi_prev'].shape != roi.shape :
            results['roi_prev'] = np.zeros(roi.shape)
            results['md_prev'] = np.zeros(metadata.size)
        results['roi_prev'][:] = roi.copy()
        results['md_prev'][:] = metadata.copy()
        return True
    
    # This ROI is not a retake. Pass along the previously acquired ROI
    # and do not skip it.
    if not md.wasPreviousRoiSaturated(metadata) :
        results['previous_roi_was_corrected'] = False
        tmp = results['roi_prev'].copy()
        results['roi_prev'][:] = roi.copy()
        roi[:] = tmp.copy()

        tmp = results['md_prev'].copy()
        results['md_prev'][:] = metadata.copy()
        metadata[:] = tmp.copy()

        return False

    # Combine this ROI with the previous ROI using HDR logic.
    results['previous_roi_was_corrected'] = True
    thresh = np.uint16(md.getSaturationThreshold(metadata)) >> md.INPUT_RAW_SHIFT

    this_a = roi[0::3]
    this_b = roi[1::3]
    this_c = roi[2::3]

    prev_a = results['roi_prev'][0::3]
    prev_b = results['roi_prev'][1::3]
    prev_c = results['roi_prev'][2::3]

    assert(this_a.shape==prev_a.shape)
    assert(this_b.shape==prev_b.shape)
    assert(this_c.shape==prev_c.shape)

    prev_max = np.max(np.stack([prev_a, prev_b, prev_c]), 0).astype(np.float32)

    a_out = np.zeros(prev_a.shape, dtype=np.float32)
    b_out = np.zeros(prev_b.shape, dtype=np.float32)
    c_out = np.zeros(prev_c.shape, dtype=np.float32)

    a_out[prev_max >= thresh] = this_a[prev_max >= thresh]
    a_out[prev_max < thresh]  = prev_a[prev_max < thresh]

    b_out[prev_max >= thresh] = this_b[prev_max >= thresh]
    b_out[prev_max < thresh]  = prev_b[prev_max < thresh]

    c_out[prev_max >= thresh] = this_c[prev_max >= thresh]
    c_out[prev_max < thresh]  = prev_c[prev_max < thresh]

    roi_out = np.zeros(roi.size, dtype=np.float32)
    roi_out[0::3] = a_out
    roi_out[1::3] = b_out
    roi_out[2::3] = c_out
    roi[:] = roi_out.copy()

    metadata[:] = results['md_prev'].copy()

    return False

def tap_rotation(roi, tap_rotated_roi, metadata) :
    # roi order: rot0f0, rot0f1, rot1f0, rot1f1, rot2f0, rot2f1
    roi_image_stride = md.getRoiNumRows(metadata) * md.getRoiNumColumns(metadata) * md.NUM_GPIXEL_PHASES

    if not md.getDoTapAccumulation(metadata):
        assert(roi.size == roi_image_stride * 2)
        tap_rotated_roi[:] = roi.copy()
        return
    
    assert (roi.size == roi_image_stride * 6)

    tap_rotated_roi[0+0*roi_image_stride:1*roi_image_stride:3] = roi[0+0*roi_image_stride:1*roi_image_stride:3] + roi[1+2*roi_image_stride:3*roi_image_stride:3] + roi[2+4*roi_image_stride:5*roi_image_stride:3]
    tap_rotated_roi[1+0*roi_image_stride:1*roi_image_stride:3] = roi[1+0*roi_image_stride:1*roi_image_stride:3] + roi[2+2*roi_image_stride:3*roi_image_stride:3] + roi[0+4*roi_image_stride:5*roi_image_stride:3]
    tap_rotated_roi[2+0*roi_image_stride:1*roi_image_stride:3] = roi[2+0*roi_image_stride:1*roi_image_stride:3] + roi[0+2*roi_image_stride:3*roi_image_stride:3] + roi[1+4*roi_image_stride:5*roi_image_stride:3]

    tap_rotated_roi[0+1*roi_image_stride:2*roi_image_stride:3] = roi[0+1*roi_image_stride:2*roi_image_stride:3] + roi[1+3*roi_image_stride:4*roi_image_stride:3] + roi[2+5*roi_image_stride:6*roi_image_stride:3]
    tap_rotated_roi[1+1*roi_image_stride:2*roi_image_stride:3] = roi[1+1*roi_image_stride:2*roi_image_stride:3] + roi[2+3*roi_image_stride:4*roi_image_stride:3] + roi[0+5*roi_image_stride:6*roi_image_stride:3]
    tap_rotated_roi[2+1*roi_image_stride:2*roi_image_stride:3] = roi[2+1*roi_image_stride:2*roi_image_stride:3] + roi[0+3*roi_image_stride:4*roi_image_stride:3] + roi[1+5*roi_image_stride:6*roi_image_stride:3]

# w and h are width and height of the output array.
# w includes the phases (3*640)
def computeSnrSquaredWeights(rawRoi0, rawRoi1, h, w) :
    assert(rawRoi0.size == rawRoi1.size)
    snr_size = rawRoi0.size//md.NUM_GPIXEL_PHASES
    snr_width = w // md.NUM_GPIXEL_PHASES
    snrSquaredRoi = np.zeros(snr_size, dtype=np.float32)
    for idx in range(snr_size) :
        snrSquaredRoi[idx]  = np.sqrt(computeSnrSquared(rawRoi0, idx)).astype(np.float32)
        snrSquaredRoi[idx] += np.sqrt(computeSnrSquared(rawRoi1, idx)).astype(np.float32)

    # normalize the columns of the snrSquredRoi to use as weights for the sum
    for col_idx in range(snr_width) :
        max = np.max(snrSquaredRoi[col_idx::snr_width])
        snrSquaredRoi[col_idx::snr_width] /= np.float32(max)
    ret = np.zeros(3*snrSquaredRoi.size, dtype=np.float32)
    ret[0::3] = snrSquaredRoi
    ret[1::3] = snrSquaredRoi
    ret[2::3] = snrSquaredRoi
    return ret

def computeSnrSquared(rawRoi, idx) :
    aIdx = idx*md.NUM_GPIXEL_PHASES
    a = rawRoi[aIdx]
    b = rawRoi[aIdx + 1]
    c = rawRoi[aIdx + 2]

    if (a <= b and a <= c) :
        tmp = c
        c = a
        a = b
        b = tmp

    elif (b <= c and b < a) :
        tmp = a
        a = c
        c = b
        b = tmp

    num = a + b - np.float32(2.0)*c
    snr_squared = np.float32(num*num) / np.float32(np.float32(2.0) * c)
    return np.float32(snr_squared)

def snr_vote(roi, rawFov, snrSquaredFov, fovIdx, metadata) :
    fovOffset = (md.getRoiStartRow(metadata)-md.getFovStartRow(metadata, fovIdx))*md.getRoiNumColumns(metadata)
    fov0 = rawFov[0]
    fov1 = rawFov[1]
    roi0 = roi[0:roi.size//2]
    roi1 = roi[roi.size//2:]
    numSnrValues = roi0.size//md.NUM_GPIXEL_PHASES

    for idx in range(numSnrValues) :
        snr0 = computeSnrSquared(roi0, idx)
        snr1 = computeSnrSquared(roi1, idx)
        snr = snr0+snr1

        if (snr > snrSquaredFov[idx + fovOffset]) :
            aIdx = idx*md.NUM_GPIXEL_PHASES
            offset = md.NUM_GPIXEL_PHASES * fovOffset

            fov0[aIdx + offset + 0] = roi0[aIdx + 0]
            fov0[aIdx + offset + 1] = roi0[aIdx + 1]
            fov0[aIdx + offset + 2] = roi0[aIdx + 2]

            fov1[aIdx + offset + 0] = roi1[aIdx + 0]
            fov1[aIdx + offset + 1] = roi1[aIdx + 1]
            fov1[aIdx + offset + 2] = roi1[aIdx + 2]

            snrSquaredFov[idx + fovOffset] = snr

    

def set_active_rows(fov_idx, active_rows, metadata) :
    for idx in range(md.getRoiNumRows(metadata)):
        row = md.getRoiStartRow(metadata) - md.getFovStartRow(metadata, fov_idx) + idx
        active_rows[row] = True

def fill_missing_rows(fov_idx, raw_fov, filled_raw_fov, active_rows, metadata) :
    filled_raw_fov[:] = raw_fov[:] #copy contents.
    binning = md.getBinning(metadata, fov_idx)
    fov_width = md.getFovNumColumns(metadata, fov_idx) * md.NUM_GPIXEL_PHASES
    # fov_height is reduced to an even multiple of binning.
    fov_height = (md.getFovNumRows(metadata, fov_idx) // binning) * binning
    
    for row in 1 + np.array(range(fov_height-2)) :
        upidx = (row-1) * fov_width 
        idx = row*fov_width
        downidx = (row+1) * fov_width 

        a = active_rows[row] == 1
        u = active_rows[row-1] == 1
        d = active_rows[row+1] == 1

        upval = raw_fov[upidx:upidx+fov_width]
        downval = raw_fov[downidx:downidx+fov_width]
        if a : continue

        if (not a and u and d) :
            filled_raw_fov[idx:idx+fov_width] = 0.5*(upval + downval)
        elif (not a and u) :
            filled_raw_fov[idx:idx+fov_width] = upval
        elif (not a and d) :
            filled_raw_fov[idx:idx+fov_width] = downval


def pr(b, size, tag="") :
    msg = tag + '\n'
    for row in range(size[0]) :
        for col in range(size[1]) :
            msg += f' {b[col + row*size[1]]:5.3}'
        msg += '\n'
    print(msg)


# unbinned_size[0] == binned_size[0] == 1
def bin1d(unbinned_fov, fov_binned_horiz, binning, unbinned_size, binned_size) :
    assert unbinned_size[0] == 1
    assert binned_size[0] == 1
    assert binned_size[1] == unbinned_size[1] // binning
    for bin in range(binning):
        fov_binned_horiz[0::3] += unbinned_fov[3*bin+0::3*binning]
        fov_binned_horiz[1::3] += unbinned_fov[3*bin+1::3*binning]
        fov_binned_horiz[2::3] += unbinned_fov[3*bin+2::3*binning]
    fov_binned_horiz[:] = fov_binned_horiz[:]

def bin(unbinned_fov_in, binned_fov, binning, unbinned_size, binned_size) :
    if binning == 1 :
        binned_fov[:] = unbinned_fov_in.copy()
        return

    # normalize the size of the input fov such that the number of rows is divisible by
    # binning.
    unbinned_width  = unbinned_size[1]
    unbinned_height = unbinned_size[0] - (unbinned_size[0]%binning)
    unbinned_fov = unbinned_fov_in[:unbinned_width*unbinned_height]

    binned_height = binned_size[0]
    binned_width  = binned_size[1]

    fov_binned_horiz = np.zeros(unbinned_height*binned_width, dtype=np.float32)

    for bin in range(binning):
        fov_binned_horiz[0::3] += unbinned_fov[3*bin+0::3*binning]
        fov_binned_horiz[1::3] += unbinned_fov[3*bin+1::3*binning]
        fov_binned_horiz[2::3] += unbinned_fov[3*bin+2::3*binning]
    
    # pr(fov_binned_horiz, (unbinned_height, binned_width))

    transpose(fov_binned_horiz, binned_width)

    # pr(fov_binned_horiz, (binned_width, unbinned_height))

    for bin in range(binning) :
        binned_fov[:] += fov_binned_horiz[bin::binning]

    # pr(binned_fov, (binned_width, binned_height))

    transpose(binned_fov, binned_height) 

    # pr(binned_fov, (binned_height, binned_width))

    binned_fov[:] = binned_fov[:]


#This routine is called twice once for each modulation frequency
#
# signal, snr, c_out (aka "background"), are initialized with zeros on the first 
# invocation of this function. This function is then called again with the same variables.
# On the second call, signal, snr, and c_out are modified by adding the current computation 
# to the previous result.
#
# phase for this frequency is assumed to have been initialized to zero.
def calculatePhase(binned_raw_fov, phase, signal, snr, c_out, number_of_summed_values) :

    # Note that the output size differs from the input size due to the non-integer relationship
    # between the specified output FOV size and binning.
    output_length = phase.size
    assert(signal.size == output_length)
    assert(signal.size == output_length)
    assert(signal.size == output_length)

    ph0 = binned_raw_fov[0:3*output_length:3]
    ph1 = binned_raw_fov[1:3*output_length:3]
    ph2 = binned_raw_fov[2:3*output_length:3]

    raw = np.stack([ph0,ph1,ph2], 0)

    c_idx = np.argmin(raw, 0)
    a_idx = np.mod(c_idx+1, 3)
    b_idx = np.mod(c_idx+2, 3)

    frac = np.zeros(c_idx.size)
    frac[c_idx == 0] = np.float32(1.0/3.0)
    frac[c_idx == 1] = np.float32(2.0/3.0)
    
    a = raw[a_idx, np.arange(a_idx.size)].astype(np.float32)
    b = raw[b_idx, np.arange(b_idx.size)].astype(np.float32)
    c = raw[c_idx, np.arange(c_idx.size)].astype(np.float32)


    signal_this_freq = a + b - np.float32(2)*c
    signal[:] += (signal_this_freq / np.float32(number_of_summed_values))

    # compute the phase, but leave the phase zero whenever signal is zero to avoid div-by-zero
    part1 = (b-c)[signal_this_freq>0]
    part2 = (np.float32(1.0)/np.float32(3.0)) * (part1/signal_this_freq[signal_this_freq>0]) + frac[signal_this_freq>0]
    phase[signal_this_freq>0] = part2

    clip = np.float32(1)/np.float32(65535.0)
    c[c<clip] = clip
    c[signal_this_freq<=0] = 0
    c_out[:] += (c[:] / np.float32(number_of_summed_values))
    snr[signal_this_freq>0] += signal_this_freq[signal_this_freq>0] / np.float32(np.sqrt(np.float32(2.0) * c[signal_this_freq>0]))


kernel_7 = np.array([4.433048175e-03, 5.400558262e-02, 2.420362294e-01, 3.990502797e-01, 2.420362294e-01, 5.400558262e-02, 4.433048175e-03], dtype=np.float32)
kernel_5 = np.array([6.646033000e-03, 1.942255544e-01, 5.982568252e-01, 1.942255544e-01, 6.646033000e-03], dtype=np.float32)
kernel_3 = np.array([1.9684139e-01, 6.0631722e-01, 1.9684139e-01], dtype=np.float32)
def transpose_raw(fov, fov_size) :
    fov_b = fov.copy()
    fov[:] = np.zeros(fov.size, dtype=np.float32)
    fov_width = fov_size[1]
    fov_height = fov_size[0]

    for row in range(fov_size[0]) :
        for col in range(fov_size[1]) :
            in_idx  = 3*col + 3*fov_width*row
            out_idx = 3*row + 3*fov_height*col
            fov[0 + out_idx] = fov_b[0 + in_idx]
            fov[1 + out_idx] = fov_b[1 + in_idx]
            fov[2 + out_idx] = fov_b[2 + in_idx]

def smooth_raw(raw_fov, smoothed_raw_fov, fov_size, filter_size) :
    if filter_size[0] > fov_size[0] or filter_size[1] > fov_size[1] :
        smoothed_raw_fov[:] = raw_fov.copy()
        return
    if filter_size == [7,5] :
        smooth_raw_impl(raw_fov, smoothed_raw_fov, fov_size, kernel_7, kernel_5)
        return
    smooth_raw_impl(raw_fov, smoothed_raw_fov, fov_size, kernel_5, kernel_3)
    return
    

# fov_size is in triplets.
def smooth_raw_impl(raw_fov, smoothed_raw_fov, fov_size, k_vert, k_horiz) :
    input_transposed = raw_fov.copy()
    fov_width = fov_size[1]
    fov_height = fov_size[0]
    half_vert = k_vert.size//2
    half_horiz = k_horiz.size//2

    transpose_raw(input_transposed, fov_size)
    column_smoothed = input_transposed.copy()

    for row in range(fov_width) :
        for col in np.array(range(fov_height - 2*half_vert)) :
            idx = 3*half_vert + 3*col + 3*fov_height*row
            sum_a = np.sum(input_transposed[idx-3*half_vert : idx+3*half_vert+1:3] * k_vert)
            column_smoothed[idx] = sum_a

            idx += 1
            sum_b = np.sum(input_transposed[idx-3*half_vert : idx+3*half_vert+1:3] * k_vert)
            column_smoothed[idx] = sum_b

            idx += 1
            sum_c = np.sum(input_transposed[idx-3*half_vert : idx+3*half_vert+1:3] * k_vert)
            column_smoothed[idx] = sum_c
    
    transpose_raw(column_smoothed, [fov_size[1], fov_size[0]])
    smoothed_raw_fov[:] = column_smoothed.copy()

    for row in range(fov_height) :
        for col in np.array(range(fov_width - 2*half_horiz)) :
            idx = 3*half_horiz + 3*col + 3*fov_width*row
            sum_a = np.sum(column_smoothed[idx-3*half_horiz : idx+3*half_horiz+1:3] * k_horiz)
            smoothed_raw_fov[idx] = sum_a

            idx += 1
            sum_b = np.sum(column_smoothed[idx-3*half_horiz : idx+3*half_horiz+1:3] * k_horiz)
            smoothed_raw_fov[idx] = sum_b

            idx += 1
            sum_c = np.sum(column_smoothed[idx-3*half_horiz : idx+3*half_horiz+1:3] * k_horiz)
            smoothed_raw_fov[idx] = sum_c

def calculate_phase_smooth(raw_smoothed, phase_smoothed, phase_frame, corrected_phase) :
    ph0 = raw_smoothed[0::3]
    ph1 = raw_smoothed[1::3]
    ph2 = raw_smoothed[2::3]

    raw = np.stack([ph0,ph1,ph2], 0)

    c_idx = np.argmin(raw, 0)
    a_idx = np.mod(c_idx+1, 3)
    b_idx = np.mod(c_idx+2, 3)

    frac = np.zeros(c_idx.size)
    frac[c_idx == 0] = np.float32(1.0/3.0)
    frac[c_idx == 1] = np.float32(2.0/3.0)

    a = raw[a_idx, np.arange(a_idx.size)].astype(np.float32)
    b = raw[b_idx, np.arange(b_idx.size)].astype(np.float32)
    c = raw[c_idx, np.arange(c_idx.size)].astype(np.float32)
    
    sig = a + b - np.float32(2)*c
    good_signal = sig[sig>0]

    part1 = (b-c)[sig>0]
    phase_smoothed[sig>0] = np.float32(1.0/3.0) * (part1/good_signal) + frac[sig>0]

    phase_smoothed[sig<=0] = np.float32(0)

    phase = np.zeros(phase_frame.size, dtype=np.float32)
    phase[sig>0] = phase_frame[sig>0]

    sub_one = (phase - phase_smoothed > np.float32(0.5)).astype(np.float32)
    add_one = (phase - phase_smoothed < np.float32(-0.5)).astype(np.float32)
    
    corrected_phase[:] = phase - sub_one + add_one

def cppround_scalar(mRaw_tmp):
    msign = (mRaw_tmp >= 0).astype(np.float32) * np.float32(2.0) - np.float32(1.0)
    mabs = np.abs(mRaw_tmp)
    mfrac = mabs - np.floor(mabs)
    # for all values that lie on 0.5, round them to the 
    m = np.round(mRaw_tmp)
    if mfrac == 0.5 :
        m = msign * np.ceil(mabs)
    return m


def cppround(mRaw_tmp):
    msign = np.array(mRaw_tmp >= 0, dtype=np.float32) * np.float32(2.0) - np.float32(1.0)
    mabs = np.abs(mRaw_tmp)
    mfrac = mabs - np.floor(mabs)
    # for all values that lie on 0.5, round them to the 
    m = mRaw_tmp.round()
    m[mfrac == 0.5] = msign[mfrac==0.5] * np.ceil(mabs[mfrac == 0.5])
    return m

def compute_whole_frame_range(phaseSmoothed, phaseFrame, range, fs, fsInt, mFrame):
    f0 = np.float32(fs[0])
    f1 = np.float32(fs[1])

    fInt0 = np.float32(fsInt[0])
    fInt1 = np.float32(fsInt[1])

    a = np.float32(0.5) * md._c / (np.float32(2.0) * f1)
    c = np.float32(0.5) * md._c / (np.float32(2.0) * f0)

    maskNegatives = (phaseSmoothed[1] < phaseSmoothed[0]).astype(np.float32)
    mRaw1 = fInt0 * phaseSmoothed[1]
    mRaw2 = fInt1 * phaseSmoothed[0]
    mRaw3 = fInt0 * maskNegatives
    mRaw_tmp = mRaw1 - mRaw2 + mRaw3

    #signed round as per c++

    m = cppround(mRaw_tmp)
    
    mFrame[:] = m + m + maskNegatives

    b = m + phaseFrame[1] + maskNegatives
    d = m + phaseFrame[0]

    range[:] = (a*b + c*d).astype(np.float32)
    range[range<0] = np.float32(0)

def min_max(m_fov, min_max_mask, window_size, thresh) :

    fov_w = m_fov.shape[1]
    fov_h = m_fov.shape[0]
    block_w = window_size[1]
    block_h = window_size[0]
    half_w = block_w//2
    half_h = block_h//2
    for row in half_h + np.arange(fov_h - block_h + 1) :
        for col in half_w + np.arange(fov_w - block_w + 1) :
            block =       m_fov[row - half_h : row + half_h + 1, col - half_w : col + half_w + 1]
            mask = min_max_mask[row - half_h : row + half_h + 1, col - half_w : col + half_w + 1]
            if np.all(mask == 1) :
                continue
            unmasked_block = block[mask==0]
            value_range = np.abs(np.max(unmasked_block) - np.min(unmasked_block))
            if (value_range > thresh) :
                min_max_mask[row , col] = 1
    

def min_max_recursive(m_fov, min_max_mask, window_size, fov_size, thresh):
    # so much easier to index with a 2D array.
    m_fov_2d        = np.reshape(m_fov.copy(),        (fov_size[0], fov_size[1]))
    min_max_mask_2d = np.zeros((fov_size[0], fov_size[1]), dtype=np.float32)
    min_max(m_fov_2d, min_max_mask_2d, window_size, thresh)

    min_max_mask_2d_reversed = np.zeros((fov_size[0], fov_size[1]), dtype=np.float32)
    m_fov_2d_reversed        = np.reshape(m_fov.copy()[::-1], (fov_size[0], fov_size[1]))
    min_max(m_fov_2d_reversed, min_max_mask_2d_reversed, window_size, thresh)

    min_max_mask[:] = min_max_mask_2d_reversed.flatten()[::-1] * min_max_mask_2d.flatten()


def median_filter_core(fov_2d, output, footprint) :
    indices = np.argwhere(footprint)
    fov_shape = fov_2d.shape
    half_h = (footprint.shape[0] | 1)//2
    half_w = (footprint.shape[1] | 1)//2
    output[:] = fov_2d.copy()    
        
    for yidx in range(half_h, fov_shape[0]-half_h) :
        for xidx in range(half_w, fov_shape[1]-half_w):
            pixels = fov_2d[yidx-half_h+indices[:,0], xidx-half_w+indices[:,1]]
            output[yidx, xidx] = np.median(pixels)

def median_filter_plus(fov, filtered_fov, fov_size, binning, do_perform_median_filter) :
    median_size = (7,5)
    if binning == 4 :
        median_size = (5,3)

    if not do_perform_median_filter :
        filtered_fov[:] = fov.copy()
        return

    fov_2d = np.reshape(fov, fov_size)
    filtered_fov_2d = np.reshape(filtered_fov, fov_size)
    plus_footprint = np.zeros((median_size[0], median_size[1]), dtype=bool)
    half_w = (median_size[1] | 1) // 2
    half_h = (median_size[0] | 1) // 2
    plus_footprint[:, half_w] = True
    plus_footprint[half_h, :] = True

    # nd.median_filter(fov_2d, output=filtered_fov_2d, footprint=plus_footprint)
    median_filter_core(fov_2d, output=filtered_fov_2d, footprint=plus_footprint)
    # "ignore the edge" is not an option for the canned median filter.
    filtered_fov_2d[0:half_h,:] = fov_2d[0:half_h,:]
    filtered_fov_2d[-half_h:,:] = fov_2d[-half_h:,:]
    filtered_fov_2d[:,0:half_w] = fov_2d[:,0:half_w]
    filtered_fov_2d[:,-half_w:] = fov_2d[:,-half_w:]

    filtered_fov[:] = filtered_fov_2d.flatten()

_lutNeighborCountTolerance = [0,      3,      5,      5,      7,     11]
_lutWindowSize             = [0,      3,      5,      6,      7,      9]
tol = np.float32(1.0)/np.float32(16.0)
_flutRangeToleranceFrac    = [0,  tol, tol, tol, tol, tol]

def nearest_neighbor(range_fov, nn_filtered_range_fov, fov_size, nn_level):
    neighbor_count = _lutNeighborCountTolerance[nn_level]
    window_size = _lutWindowSize[nn_level]
    range_tol_frac = _flutRangeToleranceFrac[nn_level]
    half_win = window_size // 2

    range_fov_2d = np.reshape(range_fov, fov_size)
    nn_filtered_range_fov_2d = range_fov_2d.copy()

    fov_w = fov_size[1]
    fov_h = fov_size[0]


    if nn_level == 0 :
        nn_filtered_range_fov[:] = range_fov[:]
        return

    idx = 0
    for row in half_win + np.arange(fov_h - 2*half_win) :
        for col in half_win + np.arange(fov_w - 2*half_win) :
            win = range_fov_2d[row-half_win:row-half_win+window_size, col-half_win:col-half_win+window_size]
            val = range_fov_2d[row, col]
            range_tol = np.float32(1.0)/np.float32(1024.0) + val * range_tol_frac
            num_neighbors = np.sum(np.abs(win-val) <= range_tol)
            if num_neighbors < neighbor_count : 
                nn_filtered_range_fov_2d[row,col] = 0
        idx += 1
    nn_filtered_range_fov[:] = nn_filtered_range_fov_2d.flatten()

def get_range(range_float, snr, min_max_mask, range_short, metadata, fov_idx) :
    maxUnambiguousRange = md.getMaxUnambiguousRange(metadata)
    range_limit = maxUnambiguousRange
    if md.getEnabledMaxRangeLimit(metadata, fov_idx):
        range_limit = md.RANGE_LIMIT_FRACTION * maxUnambiguousRange

    # unbinned, used to lookup into the pixel_mask table.
    fov_size = [md.getFovNumRows(metadata, fov_idx), md.getFovNumColumns(metadata, fov_idx)]
    if md.getStripeModeEnabled(metadata, fov_idx) :
        pixel_mask_start = [md.getRoiStartRow(metadata), md.ROI_START_COLUMN]
        pixel_mask_stride = md.getRoiNumColumns(metadata)
    else :
        pixel_mask_start = [md.getFovStartRow(metadata, fov_idx), md.getFovStartColumn(metadata, fov_idx)]
        pixel_mask_stride = fov_size[1]
    pixel_mask_step = [md.getBinning(metadata, fov_idx), md.getBinning(metadata, fov_idx)]

    size = [md.getBinnedFovHeight(metadata, fov_idx), md.getBinnedFovWidth(metadata, fov_idx)]

    pixel_mask = np.fromfile("../unittest-artifacts/mapping_table/pixel_mask_A.bin", dtype=np.uint16)
    for idx in range(range_float.size) :
        min_max_mask_val = min_max_mask[idx]
        snr_thresh = md.getSnrThresh(metadata, fov_idx)

        range_x = idx% size[1]
        range_y = idx//size[1]
        pixelMask_x = pixel_mask_start[1] + range_x*pixel_mask_step[1]
        pixelMask_y = pixel_mask_start[0] + range_y*pixel_mask_step[0]
        pixel_mask_idx = pixelMask_x + pixel_mask_stride*pixelMask_y

        fRange = range_float[idx]

        if not md.getDisableRangeMasking(metadata, fov_idx) :
            pixel_mask_val = pixel_mask[pixel_mask_idx]
            snr_val = snr[idx]
            if (min_max_mask_val or 
                snr_val < 2*snr_thresh or 
                fRange > range_limit or
                pixel_mask_val==0
                ) :
                fRange = 0

        iRange = cppround_scalar(fRange * np.float32(1024.0)).astype(np.uint16)
        range_short[idx] = np.uint16(iRange)



def get_signal(signal_float, signal_short) :
    signal_average = cppround(signal_float.copy() * np.float32(0.5))
    signal_average[signal_average > np.float32(65535.0)] = np.float32(65535.0)
    signal_short[:] = signal_average.astype(np.uint16)

def get_background(background_float, background_short) :
    background_average = cppround(background_float)
    background_average[background_float > np.float32(65535.0)] = np.float32(65535.0)
    background_short[:] = background_average.astype(np.uint16)

def get_snr(snr_float, snr_short) :
    snr_short[:] = cppround(snr_float / np.float32(2))

def median1d(range_in, ranges, binning) :
    ranges[:] = range_in.copy()
    medianSizeLut = [5,5,5,3,3]
    median_filter_length = medianSizeLut[binning]

    for idx in range(range_in.size - median_filter_length+1) :
        points = range_in[idx:idx + median_filter_length]
        median = np.median(points)
        ranges[idx + median_filter_length//2] = median

from numpy.random import randint
if __name__ == '__main__' :


    a = np.array([20,20,10,10,10,100,10,10,100,100], dtype=np.float32)
    b = np.zeros(a.size, dtype=np.float32)
    median1d(a,b,2)
    print(f'a {a}')
    print(f'b {b}')

    #Verify c++ rounding (manual)
    a = np.array([-4.5, -3.7, -3.5, -3.2, -1.0, 0.0, 1.0, 1.2, 1.5, 1.7, 2.5])
    b = np.array([-5.0, -4.0, -4.0, -3.0, -1.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0])
    c = cppround(a)
    print(f'       input    {a}')
    print(f'       cppround {c}')
    print(f'       expected {b}')

    d = []
    for idx in range(a.size) :
        d.append(cppround_scalar(a[idx]))

    print(f'scalar expected {d}')

    a = np.array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,
                    11,12,13,14,15,16,17,18,19,20,21,22,
                    21,22,23,24,25,26,27,28,29,30,31,32,
                    31,32,33,34,35,36,37,38,39,40,41,42,]).astype(np.float32)
    pr(a, (4,12), "a")
    a_transposed = a.copy()
    transpose_raw(a_transposed, (4,4))
    pr(a_transposed, (4, 12), "a transposed")

    a_snr = computeSnrSquaredWeights(a, 4, 4)
    pr(a_snr, (4,12), "a_snr normalized true")
    a_norm = np.reshape(a_snr, (4,12))
    pr(np.sum(a_norm, 0), (1,12), "a_norm_sum")


