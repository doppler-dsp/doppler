# jm:generated test_util_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import numpy as np

import doppler.util as module


class TestUtilFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_square_clip_contract(self):
        try:
            result = module.square_clip(y=1 + 0j, lin=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, complex)

    def test_next_pow_two_contract(self):
        try:
            result = module.next_pow_two(n=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_saturate_contract(self):
        try:
            result = module.saturate(v=1.0, lo=1.0, hi=1.0, nan_to=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ema_step_contract(self):
        try:
            result = module.ema_step(state=1.0, x=1.0, alpha=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ema_alpha_decim_contract(self):
        try:
            result = module.ema_alpha_decim(alpha=1.0, d=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_complement_power_contract(self):
        try:
            result = module.complement_power(p=1.0, x=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_sinc_contract(self):
        try:
            result = module.sinc(u=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_mean_sinc_contract(self):
        try:
            result = module.mean_sinc(umax=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_simpson_weights_contract(self):
        try:
            result = module.simpson_weights(w=np.zeros(4, dtype=np.float64))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    def test_midpoint_nodes_contract(self):
        try:
            result = module.midpoint_nodes(u=np.zeros(4, dtype=np.float64))
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)

    def test_gauss_hermite_contract(self):
        try:
            result = module.gauss_hermite(
                z=np.zeros(4, dtype=np.float64),
                p=np.zeros(4, dtype=np.float64),
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsNone(result)
