"""Integration test for the continuous wfmgen -> follow -> demod pipeline.

The wfmgen CLI streams scene.json (--continuous) to a growing file; the reader
follows it and decodes each burst (CRC + bits) as its samples land, alongside
the writer. The scene uses wfmgen's ranged-field interface, so each repeat
draws a fresh Doppler (``freq: [lo, hi]``) and a fresh arrival jitter (trailing
``off_samples: [lo, hi]`` → varying code phase) on top of a fresh noise
realization (``seed_advance="noise"``); the code/payload are fixed, so every
burst still decodes to the same frame. Run non-paced (realtime=False) so the
writer races and the reader drains it fast. Skips when the CLI isn't built.
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
        # Doppler is drawn per burst; the recovered estimate lands in the band.
        assert (
            demo.DOPPLER_LO - 200.0
            <= r["est_freq_hz"]
            <= demo.DOPPLER_HI + 200.0
        ), f"burst {r['burst']} Doppler {r['est_freq_hz']:.0f} out of band"
        assert 0 <= r["code_phase"] <= demo.JITTER_MAX + demo.SPC


def test_ranged_fields_vary_burst_to_burst():
    """The whole point of the ranged scene: Doppler and code phase shift from
    burst to burst (a fixed scene would repeat them identically)."""
    results = demo.run_streaming(5, realtime=False)
    dopplers = [r["est_freq_hz"] for r in results]
    phases = [r["code_phase"] for r in results]
    # At least three distinct Doppler draws and three distinct code phases out
    # of five bursts (uniform draws practically never collide this much).
    assert len({round(f) for f in dopplers}) >= 3
    assert len(set(phases)) >= 3
