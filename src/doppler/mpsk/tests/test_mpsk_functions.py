# jm:generated test_mpsk_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.mpsk as module


class TestMpskFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mpsk_map_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mpsk_demap_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mpsk_diff_map_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mpsk_diff_demap_contract(self):
        pass

    def test_mpsk_soft_demap_contract(self):
        try:
            result = module.mpsk_soft_demap(
                x=np.zeros(4, dtype=np.complex64),
                llr=np.zeros(4, dtype=np.float32),
                m=1,
                n0=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    def test_mpsk_bits_per_symbol_contract(self):
        try:
            result = module.mpsk_bits_per_symbol(m=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)
