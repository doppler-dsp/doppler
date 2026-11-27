# jm:generated test_ccsds_functions.py
# jm regenerates this file on apply while the line above is here.
# To make it yours, delete that line: jm then never writes it again.
import unittest

import doppler.ccsds as module


class TestCcsdsFunctions(unittest.TestCase):
    """Contract smoke test for the module-level functions (gh-2127).

    Each wrapper is called with arguments derived from its declared
    parameters. The test asserts the call succeeds and the result has
    the declared return type. It asserts no values: the bodies are the
    author's, and their documented cases are the header's ``@code``
    examples.
    """

    @unittest.skip("out_type changes the call's shape: no contract call yet")
    def test_asm_bits_contract(self):
        pass

    def test_ccsds415_chip_rate_contract(self):
        try:
            result = module.ccsds415_chip_rate(fc=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)

    def test_ccsds415_scene_contract(self):
        try:
            result = module.ccsds415_scene(fc=1.0, spc=1.0, start_hz=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, module.Ccsds415Scene)

    def test_ccsds415_cn0_contract(self):
        try:
            result = module.ccsds415_cn0(esn0_db=1.0)
        except (ValueError, RuntimeError, OverflowError) as e:
            self.skipTest(f"the kernel refused the synthetic arguments: {e}")
        else:
            self.assertIsInstance(result, float)
