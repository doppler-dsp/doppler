# jm:generated test_measure_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import doppler.measure as module


class TestMeasureFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_measure_min_samples_contract(self):
        try:
            result = module.measure_min_samples(
                fs=1.0,
                target_rbw=1.0,
                bits=1,
                dynamic_range_db=1.0,
                complex_input=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_measure_rec_nfft_contract(self):
        try:
            result = module.measure_rec_nfft(n=1, pad=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_measure_proc_gain_contract(self):
        try:
            result = module.measure_proc_gain(nfft=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_dp_coherent_freq_contract(self):
        try:
            result = module.dp_coherent_freq(fs=1.0, f_target=1.0, N=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)
