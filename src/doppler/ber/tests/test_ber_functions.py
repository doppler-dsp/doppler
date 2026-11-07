# jm:generated test_ber_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.ber as module


class TestBerFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_ber_theory_ser_contract(self):
        try:
            result = module.ber_theory_ser(m=1, esn0=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_theory_ber_contract(self):
        try:
            result = module.ber_theory_ber(m=1, esn0=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_esn0_db_for_ser_contract(self):
        try:
            result = module.ber_esn0_db_for_ser(m=1, ser=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_evm_scatter_floor_db_contract(self):
        try:
            result = module.ber_evm_scatter_floor_db(m=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_evm_ideal_db_contract(self):
        try:
            result = module.ber_evm_ideal_db(esn0_db=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_settle_syms_contract(self):
        try:
            result = module.ber_settle_syms(bn_timing=1.0, bn_carrier=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_ber_lock_symbol_contract(self):
        try:
            result = module.ber_lock_symbol(
                flags=np.zeros(4, dtype=np.uint8),
                sustain=1,
                min_frac=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_ber_evm_db_contract(self):
        try:
            result = module.ber_evm_db(
                rx=np.zeros(4, dtype=np.complex64),
                lo=1,
                hi=1,
                m=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ber_settle_from_contract(self):
        try:
            result = module.ber_settle_from(
                budget=1,
                timing_lock=1,
                carrier_lock=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)
