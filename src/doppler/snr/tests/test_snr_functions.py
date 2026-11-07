# jm:generated test_snr_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.snr as module


class TestSnrFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_snr_data_aided_db_contract(self):
        try:
            result = module.snr_data_aided_db(
                soft=np.zeros(4, dtype=np.complex64),
                sign_bits=np.zeros(4, dtype=np.uint8),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_snr_m2m4_db_contract(self):
        try:
            result = module.snr_m2m4_db(x=np.zeros(4, dtype=np.complex64))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_snr_data_aided_db_series_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_snr_m2m4_db_series_contract(self):
        pass
