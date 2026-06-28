"""Integration test for the wfmgen -> disk -> DDC -> ACQ -> BurstDemod demo.

Exercises the full feedforward chain end to end through the real tools: the
``wfmgen`` CLI writes a multi-burst capture (Doppler + noise baked in) to disk,
then a single receive chain reads it back and decodes every burst (CRC + bits).
Skips when the wfmgen CLI isn't built. Runs non-paced (no real-time wait).
"""

import tempfile
from pathlib import Path

import numpy as np
import pytest

from doppler.examples import dsss_realtime_file_demod as demo

pytestmark = pytest.mark.skipif(
    demo.wfmgen_available() is None, reason="wfmgen CLI not built / on PATH"
)


def test_all_bursts_decode_from_disk():
    # seed 0 is the regression case: an under-sized Acquisition (a single
    # Doppler bin) failed every other burst; the slow-time Doppler search
    # (doppler_bins = reps) decodes them all.
    rng = np.random.default_rng(0)
    payloads = [
        rng.integers(0, 2, demo.PAYLOAD).astype(np.uint8) for _ in range(6)
    ]
    with tempfile.TemporaryDirectory() as tmp:
        cap = Path(tmp) / "cap.cf32"
        demo.generate(cap, payloads)
        results = demo.stream_decode(cap, realtime=False)

    assert len(results) == len(payloads)
    for r, payload in zip(results, payloads):
        assert r["detected"], f"burst {r['burst']} not detected"
        assert r["frame_valid"], f"burst {r['burst']} CRC failed"
        assert np.array_equal(r["bits"], payload)
        # DDC removed the nominal; BurstDemod recovers the true Doppler.
        assert abs(r["est_freq_hz"] - demo.DOPPLER_HZ) < 200.0
