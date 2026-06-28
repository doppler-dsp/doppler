"""Integration test for the continuous wfmgen -> tail -> demod pipeline.

The wfmgen CLI streams scene.json (--continuous) to a growing file; the reader
tails it and decodes each burst (CRC + bits) as its samples land, concurrently
with the writer. Each repeat is a fresh noise realization (wfmgen advances the
seed) but the code/payload are fixed, so every burst decodes to the same frame.
Run non-paced (realtime=False) so the writer races and the reader drains it
fast. Skips when the wfmgen CLI isn't built.
"""

import numpy as np
import pytest

from doppler.examples import dsss_realtime_file_demod as demo

pytestmark = pytest.mark.skipif(
    demo.wfmgen_available() is None, reason="wfmgen CLI not built / on PATH"
)


def test_tailing_pipeline_decodes_every_burst():
    results = demo.run_streaming(4, realtime=False)

    assert len(results) == 4
    for r in results:
        assert r["detected"], f"burst {r['burst']} not detected"
        assert r["frame_valid"], f"burst {r['burst']} CRC failed"
        assert np.array_equal(r["bits"], demo._PAYLOAD_BITS)
        assert abs(r["est_freq_hz"] - demo.DOPPLER_HZ) < 200.0
        assert "code_phase" in r
    # (When the wfmgen seed-advance feature is present each repeat is a fresh
    # noise realization; the frame is identical so every burst still decodes.)
