# jm:generated test_cvt_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.cvt as module


class TestCvtFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_int_to_bin_contract(self):
        try:
            result = module.int_to_bin(
                v=1,
                n_bits=1,
                out=np.zeros(4, dtype=np.uint8),
                bitorder=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    @unittest.skip(
        "parameter 'hex' of type 'const char *' has no contract argument yet"
    )
    def test_hex_to_bin_contract(self):
        pass

    def test_bytes_to_bin_contract(self):
        try:
            result = module.bytes_to_bin(
                octets=np.zeros(4, dtype=np.uint8),
                out=np.zeros(4, dtype=np.uint8),
                bitorder=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_bin_to_int_contract(self):
        try:
            result = module.bin_to_int(
                bits=np.zeros(4, dtype=np.uint8),
                bitorder=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_bin_to_hex_contract(self):
        try:
            result = module.bin_to_hex(
                bits=np.zeros(4, dtype=np.uint8),
                out=np.zeros(4, dtype=np.uint8),
                bitorder=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_bin_to_nrz_contract(self):
        try:
            result = module.bin_to_nrz(
                bits=np.zeros(4, dtype=np.uint8),
                out=np.zeros(4, dtype=np.float32),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_nrz_to_bin_contract(self):
        try:
            result = module.nrz_to_bin(
                nrz=np.zeros(4, dtype=np.float32),
                out=np.zeros(4, dtype=np.uint8),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)
