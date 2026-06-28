"""Integration test for the tailing wfmgen -> demod pipeline.

The wfmgen CLI streams a multi-burst capture (Doppler + noise baked in) to a
growing file; the reader tails it and decodes each burst (CRC + bits) as its
samples land — concurrently with the writer. Run non-paced (realtime=False)
so the writer races ahead and the tail reader drains it fast. Skips when the
wfmgen CLI isn't built.
"""

import numpy as np
import pytest

from doppler.examples import dsss_realtime_file_demod as demo

pytestmark = pytest.mark.skipif(
    demo.wfmgen_available() is None, reason="wfmgen CLI not built / on PATH"
)


def test_tailing_pipeline_decodes_every_burst():
    # seed 0 is the regression case: an under-sized Acquisition (a single
    # Doppler bin) failed every other burst; the slow-time Doppler search
    # (doppler_bins = reps) decodes them all.
    rng = np.random.default_rng(0)
    payloads = [
        rng.integers(0, 2, demo.PAYLOAD).astype(np.uint8) for _ in range(6)
    ]
    results = demo.run_streaming(payloads, realtime=False)

    assert len(results) == len(payloads)
    for r, payload in zip(results, payloads):
        assert r["detected"], f"burst {r['burst']} not detected"
        assert r["frame_valid"], f"burst {r['burst']} CRC failed"
        assert np.array_equal(r["bits"], payload)
        # DDC removed the nominal; BurstDemod recovers the true Doppler.
        assert abs(r["est_freq_hz"] - demo.DOPPLER_HZ) < 200.0
