# raw-to-depth-cpp
This directory contains the algorithm code for executing the mathematical operations for converting raw iTOF data into point clouds for display.

## General Description

The top-level entry point is found in RawToFovs. The three key methods are

* processRoi()
* fovsAvailable(), and
* getData()

You can get a good feel for how these work by studying the function "processOneDirectoryOfRois()" in ../raw-to-depth-cpp-tests/raw-to-depth-tests.cpp

The buffering, I/O control, and top-level interaction with the algorithms can be found in the two classes

* RawToDepth, and the specialization 
* RawToDepthV2_float.

The algorithms are mostly specified in 

* RawToDepthDsp

but some of the algorithms are broken into individual classes, e.g.

* Binning
* HDR
* NearestNeighbor
* Mapping Table
* TemperatureCalibration

## Some top-level headers:
<ol>
    <li>[RawToFovs.h](./RawToFovs.h)</li>
    The top-level entry point for passing data to, and retrieving results from the RawToDepth library.
    <li>[RawToDepth.h](./RawToDepth.h)</li>
    The parent class for the RawToDepth Module. Contains data and methods common for multiple RawToDepth specializations.   
    <li>[RawToDepthV2_float.h](RawToDepthV2_float.h)</li>
    Specialization of the RawToDepth class that implements the float-point RawToDepth algorithm set.
    <li>[FovSegment.h](./FovSegment.h)</li>
    The data structure that holds the output point cloud data data for this FOV.
    <li>[RtdMetadata.h](./RtdMetadata.h)</li>
    Interprets the metadata from an M20/M25/M30 scanhead. The metadata is passed to the RawToDepth code via an extra video line (640*3 shorts) at the beginning of each ROI.
    <li>[RawToDepthDsp.h](./RawToDepthDsp.h)</li>
    The algorithms for performing digital signal processing to generate point clouds from raw iTOF data.
    <li>[Binning.h](./Binning.h)</li>
    Performs binning on 2D raw data using 32-bit floating-point operations on the CPU.
    <li>[hdr.h](./hdr.h)</li>
    High Dynamic Range algorithms
    <li>[NearestNeighbor.h](./NearestNeighbor.h)</li>
    Implements the nearest-neighbor filter on range values using 32-bit floating-point operations on the CPU.
    <li>[MappingTable.h](./MappingTable.h)]</li>
    A utility class for loading and accessing the calibration angle-to-angle mapping table.
    <li>[TemperatureCalibration.h](./TemperatureCalibration.h)</li>
    Calculate fixed offset due to changes in temperature on the sensor. The temperature measurements and other parameters are provided in the metadata.
</ol>