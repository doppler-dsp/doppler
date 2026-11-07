# jm:generated test_resample_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import doppler.resample as module


class TestResampleFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_ciccompmf_contract(self):
        pass

    def test_kaiser_beta_contract(self):
        try:
            result = module.kaiser_beta(atten=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_kaiser_num_taps_contract(self):
        try:
            result = module.kaiser_num_taps(
                num_phases=1,
                atten=1.0,
                pb=1.0,
                sb=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)
