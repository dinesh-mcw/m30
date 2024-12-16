import numpy as np
import matplotlib.pyplot as plt
import os

num_rows = 480
num_cols = 640
match_tolerance = 2e-5

# print("Load python binary output")
# # Algorithm 0
# range_frame_python = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_ranges.bin'),
#                                             dtype=np.float32).astype(np.float32), [num_rows, num_cols])
# # Algorithm 1
# range_frame_python_alg1 = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_ranges_alg1.bin'),
#                                             dtype=np.float32).astype(np.float32), [num_rows, num_cols])
#
# # Common
# snr0_frame_python = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_SnrSquared0.bin'),
#                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])
# snr1_frame_python = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_SnrSquared1.bin'),
#                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])
# background0_frame_python = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_background0.bin'),
#                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])
# background1_frame_python = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'jetson_array_opticmout_checkerboard_higherpower_background1.bin'),
#                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])

# Load independent binary
range_frame_reference = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'cobra_raw2depth_data'), 'synth_1roi_actual_ranges.bin'),
                                               dtype=np.float32).astype(np.float32), [num_rows, num_cols])

print("Load cpp binary output")
# Algorithm 0
range_frame_cpp = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_ranges_synthrois_1.bin'),
                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])
# Algorithm 1
range_frame_cpp_alg1 = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_ranges_synthrois_1_alg1.bin'),
                                            dtype=np.float32).astype(np.float32), [num_rows, num_cols])

# Common
snr0_frame_cpp = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_snr0.bin'),
                                           dtype=np.float32).astype(np.float32), [num_rows, num_cols])
snr1_frame_cpp = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_snr1.bin'),
                                           dtype=np.float32).astype(np.float32), [num_rows, num_cols])
background0_frame_cpp = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_background0.bin'),
                                           dtype=np.float32).astype(np.float32), [num_rows, num_cols])
background1_frame_cpp = np.reshape(np.fromfile(os.path.join(os.path.join('..', '..', 'tmp'), 'cpp_background1.bin'),
                                           dtype=np.float32).astype(np.float32), [num_rows, num_cols])

# Error matrices
# range_err = np.abs((range_frame_reference - range_frame_cpp)/range_frame_python)
# range_err_alg1 = np.abs((range_frame_reference - range_frame_cpp_alg1)/range_frame_python_alg1)

# snr0_err = np.abs((snr0_frame_python - snr0_frame_cpp))
# snr1_err = np.abs((snr1_frame_python - snr1_frame_cpp))
# back0_err = np.abs((background0_frame_python - background0_frame_cpp))
# back1_err = np.abs((background1_frame_python - background1_frame_cpp))

# not_matching_ranges_error = range_err_alg1[range_err_alg1 > match_tolerance]
# not_matching_ranges_where = np.argwhere(range_err_alg1 > match_tolerance)
# not_matching_ranges_python = range_frame_python_alg1[range_err_alg1 > match_tolerance]
# not_matching_ranges_cpp = range_frame_cpp_alg1[range_err_alg1 > match_tolerance]

# Max errors
# print("Max range error is " + str(np.amax(range_err)))
# print("Max range error is " + str(np.amax(range_err_alg1)))
# print("Max snr0 error is " + str(np.amax(snr0_err)))
# print("Max snr1 error is " + str(np.amax(snr1_err)))
# print("Max background0 error is " + str(np.amax(back0_err)))
# print("Max background1 error is " + str(np.amax(back1_err)))

fig0, (ax0, ax1, ax2) = plt.subplots(1, 3, sharex=True, sharey=True)
pcm = ax0.imshow(range_frame_reference, vmin=0, vmax=np.max([np.amax(range_frame_reference), np.amax(range_frame_cpp), np.amax(range_frame_cpp_alg1)]), cmap='jet', aspect='auto')
ax0.set_title('Reference frame')
fig0.colorbar(pcm, ax=ax0)
pcm = ax1.imshow(range_frame_cpp, vmin=0, vmax=np.max([np.amax(range_frame_reference), np.amax(range_frame_cpp), np.amax(range_frame_cpp_alg1)]), cmap='jet', aspect='auto')
ax1.set_title('Cpp frame (Alg 0)')
fig0.colorbar(pcm, ax=ax1)
pcm = ax2.imshow(range_frame_cpp_alg1, vmin=0, vmax=np.max([np.amax(range_frame_reference), np.amax(range_frame_cpp), np.amax(range_frame_cpp_alg1)]), cmap='jet', aspect='auto')
ax2.set_title('Cpp frame (Alg 1)')
fig0.colorbar(pcm, ax=ax2)

# fig1, (ax10, ax11) = plt.subplots(1, 2, sharex=True, sharey=True)
# ax10.imshow(range_frame_python_alg1, vmin=0, vmax=np.amax(range_frame_python_alg1), cmap='jet', aspect='auto')
# ax10.set_title('Python range frame - ' + str(len(not_matching_ranges_error)) + " errors")
# range_error_display = np.zeros_like(range_frame_python_alg1)
# range_error_display[np.nonzero(range_err_alg1 > match_tolerance)] = 1
# ax11.imshow(range_error_display, vmin=0, vmax=1, cmap='binary', aspect='auto')
# ax11.set_title('Location of errors with cpp code')

plt.show()