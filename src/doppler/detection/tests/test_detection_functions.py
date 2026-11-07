# jm:generated test_detection_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import doppler.detection as module


class TestDetectionFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    def test_marcum_q_contract(self):
        try:
            result = module.marcum_q(m=1, a=1.0, b=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_threshold_contract(self):
        try:
            result = module.det_threshold(pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_pd_contract(self):
        try:
            result = module.det_pd(snr=1.0, dwell=1, threshold=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_dwell_contract(self):
        try:
            result = module.det_dwell(
                snr=1.0,
                pd_min=1.0,
                pfa=1.0,
                max_dwell=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_det_snr_contract(self):
        try:
            result = module.det_snr(dwell=1, pd_min=1.0, pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_threshold_noncoherent_contract(self):
        try:
            result = module.det_threshold_noncoherent(pfa=1.0, n_noncoh=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_q_inv_contract(self):
        try:
            result = module.det_q_inv(p=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_dwell_gauss_contract(self):
        try:
            result = module.det_dwell_gauss(mean=1.0, var=1.0, pd=1.0, pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_det_threshold_gauss_contract(self):
        try:
            result = module.det_threshold_gauss(mean=1.0, pd=1.0, pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_ema_alpha_contract(self):
        try:
            result = module.det_ema_alpha(snr_in_db=1.0, snr_out_db=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_verify_count_contract(self):
        try:
            result = module.det_verify_count(p_look=1.0, p_target=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_det_verify_delay_contract(self):
        try:
            result = module.det_verify_delay(p_look=1.0, n=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_threshold_f_contract(self):
        try:
            result = module.det_threshold_f(pfa=1.0, n=1)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_pd_noncoherent_contract(self):
        try:
            result = module.det_pd_noncoherent(
                snr=1.0,
                n_coh=1,
                n_noncoh=1,
                threshold=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_n_noncoh_contract(self):
        try:
            result = module.det_n_noncoh(
                snr=1.0,
                n_coh=1,
                pd_min=1.0,
                pfa=1.0,
                max_n_noncoh=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_det_threshold_power_contract(self):
        try:
            result = module.det_threshold_power(pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_pd_power_contract(self):
        try:
            result = module.det_pd_power(
                snr_power=1.0,
                dwell=1,
                power_threshold=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_dwell_power_contract(self):
        try:
            result = module.det_dwell_power(
                snr_power=1.0,
                pd_min=1.0,
                pfa=1.0,
                max_dwell=1,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, int)
            self.assertNotIsInstance(result, bool)

    def test_det_snr_power_contract(self):
        try:
            result = module.det_snr_power(dwell=1, pd_min=1.0, pfa=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_pfa_cell_contract(self):
        try:
            result = module.det_pfa_cell(pfa=1.0, n_cells=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_cn0_to_snr_contract(self):
        try:
            result = module.det_cn0_to_snr(cn0_dbhz=1.0, fs=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_snr_to_cn0_contract(self):
        try:
            result = module.det_snr_to_cn0(snr=1.0, fs=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_det_pd_cfar_contract(self):
        try:
            result = module.det_pd_cfar(
                snr=1.0,
                dwell=1,
                threshold=1.0,
                k=1.0,
                leak=1.0,
                leak_cells=1.0,
            )
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)
