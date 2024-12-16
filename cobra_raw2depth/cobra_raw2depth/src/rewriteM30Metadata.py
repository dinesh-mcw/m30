'''
file: rewriteM30Metadata.py

brief: A script to load, then rewrite some metadata fields in M30 ROIs so that
the raw files specify a different processing configuration.

Features:
Unsets the disable RawToDepth functionality. This bit is set in the metadata during data
acquisition on the embedded board since the board has insufficient resources to both
process the data and save to disk.

Copyright 2023 (C) Lumotive, Inc. All rights reserved.

'''

import M30Metadata as md
import argparse
from pathlib import Path
from glob import glob
import os
import numpy as np


if __name__ == "__main__" :
  parser = argparse.ArgumentParser(
      prog=Path(__file__).name,
      description='Reads and rewrites the metadata from all of the .bin files in the given directory.'
  )

  # "store_true" indicates a boolean argument and stores this argument as true only if it exists on the command line, otherwise it defaults to false.
  parser.add_argument('--print-random-fov-tag', action='store_true', help='Prints out metadata whenever the random FOV tag changes')
  parser.add_argument('--print-roi-counter', action='store_true', help='Prints the ROI counter to test for dropped ROIs.')
  parser.add_argument('--enable_rtd', action='store_true', help='Forces the disable_rtd bit to zero in the metadata header. Otherwise no change.')
  parser.add_argument('--hdr', action='store_true', help='Print HDR info')
  parser.add_argument('--dir_path')

  args = parser.parse_args()
  print(f'{md.currentFile(__file__)} - Reading files from {args.dir_path}')
  if args.enable_rtd :
    print(f'{md.currentFile(__file__)} - Removing the disable_rtd bit from input metadata headers.')
  
  p = glob(os.path.join(args.dir_path, '*.bin'))
  fnames = sorted(p)
  
  first_roi_found = False
  rois_prior_to_first = 0
  prev_roi_counter=-1
  changed_files_count = 0
  prev_random_fov_tag = [-1]*md.MAX_ACTIVE_FOVS
  rois_this_fov = [0]*md.MAX_ACTIVE_FOVS
  print(f'{md.currentFile(__file__)} - Processing {len(fnames)} files in {args.dir_path}')
  for fname_idx, fname in zip(range(len(fnames)), fnames) :
    # load the roi and extract the metadata, shifting off the low 4 bits.
    raw_roi = np.fromfile(fname, dtype=np.uint16)
    metadata = raw_roi[:md.MD_ROW_SHORTS] >> md.MD_SHIFT_BY

    for fov_idx in md.getActiveFovs(metadata):
      rois_this_fov[fov_idx] += 1
      if args.enable_rtd :
        metadata = md.enableRtd(metadata, fov_idx)

      if args.print_random_fov_tag :
        fov_tag = md.getRandomFovTag(metadata, fov_idx)

        if md.getFirstRoi(metadata, fov_idx):
          print(f'First ROI in fov {fname} has fov_tag {fov_tag} at filename index {fname_idx} with scan table tag {md.getScanTableTag(metadata)}')
        if prev_random_fov_tag[fov_idx] != fov_tag :
          print(f'New   tag in fov {fname} has fov_tag {fov_tag} at filename index {fname_idx} with scan table tag {md.getScanTableTag(metadata)}')
          prev_random_fov_tag[fov_idx] = fov_tag

      if md.getFirstRoi(metadata, fov_idx) :
        print(f'ROIs in fov {fov_idx} = {rois_this_fov[fov_idx]} expected={md.getFovNumRois(metadata, fov_idx)}')
        rois_this_fov[fov_idx] = 0
        first_roi_found = True
      else :
        if rois_prior_to_first > 0 and first_roi_found == True :
          print(f'ROIs skipped prior to the ROI marked as first-in-FOV: {rois_prior_to_first}')
          rois_prior_to_first = -1
        elif first_roi_found == False:
          rois_prior_to_first += 1


      if args.hdr :
        retake = md.wasPreviousRoiSaturated(metadata)
        first = (" first " if md.getFirstRoi(metadata, fov_idx) else "")
        last = (" last " if md.getFrameCompleted(metadata, fov_idx) else "")
        print(f'HDR: {first} {last} retake {retake} sat level {md.getSaturationThreshold(metadata)}')
        pass

      if args.print_roi_counter :
        if prev_roi_counter >= 0 and md.getRoiCounter(metadata) != prev_roi_counter + 1 :
          print(f'Dropped ROI at ROI counter = {md.getRoiCounter(metadata)}')
        prev_roi_counter = md.getRoiCounter(metadata)

    # reshift and store the roi, if the metadata has changed.
    metadata = metadata << md.MD_SHIFT_BY

    if np.not_equal(metadata, raw_roi[:md.MD_ROW_SHORTS]).any() :
      changed_files_count += 1
      print(f'writing raw_roi for {fname}')
      raw_roi[:md.MD_ROW_SHORTS] = metadata
      raw_roi.tofile(fname)

  print(f'{md.currentFile(__file__)} - Completed rewriting {changed_files_count} files.')


  pass