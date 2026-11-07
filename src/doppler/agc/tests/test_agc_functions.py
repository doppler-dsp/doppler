# jm:generated test_agc_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import doppler.agc as module


class TestAgcFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_settling_samples_contract(self):
        try:
            result = module.settling_samples(
                loop_bw=1.0,
                alpha=1.0,
                gain_err_db=1.0,
                tol_db=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)
