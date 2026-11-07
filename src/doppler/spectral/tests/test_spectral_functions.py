# jm:generated test_spectral_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.spectral as module


class TestSpectralFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_kaiser_enbw_contract(self):
        try:
            result = module.kaiser_enbw(w=np.zeros(4, dtype=np.float32))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_kaiser_window_contract(self):
        try:
            result = module.kaiser_window(
                w=np.zeros(4, dtype=np.float32),
                beta=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    def test_kaiser_beta_for_sidelobe_contract(self):
        try:
            result = module.kaiser_beta_for_sidelobe(atten_db=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_hann_window_contract(self):
        try:
            result = module.hann_window(w=np.zeros(4, dtype=np.float32))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    def test_blackman_harris_window_contract(self):
        try:
            result = module.blackman_harris_window(
                w=np.zeros(4, dtype=np.float32),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_magnitude_db_cf32_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_magnitude_db_cf64_contract(self):
        pass

    @unittest.skip(
        "result_fields changes the call's shape: no contract call yet"
    )
    def test_find_peaks_f32_contract(self):
        pass

    def test_obw_from_power_contract(self):
        try:
            result = module.obw_from_power(
                pwr=np.zeros(4, dtype=np.float64),
                fs=1.0,
                frac=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_noise_floor_db_contract(self):
        try:
            result = module.noise_floor_db(db=np.zeros(4, dtype=np.float32))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_power_to_db_f32_contract(self):
        pass
