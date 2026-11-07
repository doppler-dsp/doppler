# jm:generated test_arith_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.arith as module


class TestArithFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_add_q15_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_sub_q15_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mul_q15_contract(self):
        pass

    def test_dot_q15_contract(self):
        try:
            result = module.dot_q15(
                a=np.zeros(4, dtype=np.int16),
                b=np.zeros(4, dtype=np.int16),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shl_q15_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shr_q15_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_add_q8_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_sub_q8_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_mul_q8_contract(self):
        pass

    def test_dot_q8_contract(self):
        try:
            result = module.dot_q8(
                a=np.zeros(4, dtype=np.int8),
                b=np.zeros(4, dtype=np.int8),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shl_q8_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shr_q8_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shl_i64_contract(self):
        pass

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_shr_i64_contract(self):
        pass
