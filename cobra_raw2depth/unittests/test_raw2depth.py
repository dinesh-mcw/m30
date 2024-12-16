import unittest

import numpy as np
from parameterized import parameterized

import cobra_raw2depth.raw2depth as r2d

N_SUBFRAME = 6
N_TAP = 3


class TestRaw2Depth(unittest.TestCase):

    @parameterized.expand([
        (np.array([2 ** 5, 2 ** 7, 2 ** 9]).astype(np.uint16),
         np.array([2 ** 1, 2 ** 3, 2 ** 5], dtype=np.uint16)),
        (np.array([[2 ** 5, 2 ** 7],
                   [2 ** 9, 2 ** 6]]).astype(np.float32),
         np.array([2 ** 1, 2 ** 3, 2 ** 5, 2 ** 2], dtype=np.uint16))
    ])
    def test_condition_data(self, start, result):
        """Ensure we get a flat array with uint16s and bit shift by 4"""
        out = r2d.preprocess_data(start)
        self.assertTrue(np.array_equal(out, result))

    # Test primitive functions
    @parameterized.expand([(
            np.array([[[0, 1, 0],
                       [1, 1, 1],
                       [1, 0, 1]],
                      [[1, 0, 1],
                       [1, 1, 1],
                       [1, 1, 1]],
                      [[1, 0, 0],
                       [0, 1, 0],
                       [0, 0, 1]]],
                     ),
            np.array([[0, 3, 1],
                      [2, 2, 3],
                      [3, 1, 2]])),
    ])
    def test_tap_addition(self, start, result):
        """Here we are basically grabbing the outermost index (the rotations),
        taking that array, and rotating the columns (innermost index) backwards,
         then adding everything up along the rotation dimension."""
        out = r2d.tap_addition(start)
        self.assertTrue(np.array_equal(out, result))

    @parameterized.expand([(
            np.array([[0, 1, 2],
                      [4, 8, 0],
                      [5, 1, 6]]
                     ),
            np.array([[0, 1, 2],
                      [0, 4, 8],
                      [1, 6, 5]])),
    ])
    def test_tap_rotation(self, start, result):
        """Pretty straightforward. All we are doing here is rotating all
        the minimum elements in each row to the front."""
        out = r2d.tap_rotation(start)[0]
        self.assertTrue(np.array_equal(out, result))

    @parameterized.expand([(
            np.array([[[0, 1, 0],
                       [1, 1, 1],
                       [1, 0, 1]],
                      [[1, 0, 1],
                       [1, 1, 1],
                       [1, 1, 1]],
                      [[1, 0, 0],
                       [0, 1, 0],
                       [0, 0, 1]]],
                     ),
            np.array([1,
                      3,
                      2,
                      2,
                      3,
                      3,
                      1,
                      1,
                      1],
                     ),
    )])
    def test_intensity_image(self, start, result):
        """Intensity image returns a flat array of all the bins added
        together (i.e. along the innermost axis; columns above)."""
        out = r2d.intensity_image(start)
        self.assertTrue(np.array_equal(out, result))

    @unittest.skip('not implemented in r2d')
    def test_compute_phase_snr(self, start, result):
        raise NotImplementedError()

    @unittest.skip('not implemented in r2d')
    def test_compute_depth(self, start, result):
        raise NotImplementedError()

    # @parameterized.expand([
    #     # input, output, type, error
    #     (np.array([2 ** 14 - 2]), np.array([4095]), 'clip', None),
    #     (np.array([2 ** 14 - 2]), np.array([4095]), 'floor', None),
    #     (np.array([2 ** 14 - 2]), np.array([4096]), 'round', None),
    #     (np.array([2 ** 14 + 2]), np.array([4095]), 'clip', None),
    #     (np.array([2 ** 14 + 2]), np.array([4096]), 'floor', None),
    #     (np.array([2 ** 14 + 2]), np.array([4096]), 'round', None),
    #     (np.array([1000.5]), np.array([1000.5]), 'clip', None),
    #     (np.array([1000.5]), None, 'floor', TypeError),
    #     (np.array([1000.5]), 250, 'round', None),
    # ])
    # def test_data_reduce(self, idata, odata, rtype, error):
    #     r2d = Raw2Depth(0, 0, 10, 10, clip_round_floor=rtype)
    #     if error is not None:
    #         with self.assertRaises(error):
    #             r2d.data_reduce(idata)
    #     else:
    #         out = r2d.data_reduce(idata)
    #         self.assertEqual(out, odata)


if __name__ == "__main__":
    unittest.main()
