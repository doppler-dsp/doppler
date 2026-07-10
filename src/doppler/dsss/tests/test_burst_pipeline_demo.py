"""Integration test for the 5-burst wfmgen -> Acquisition/BurstDespreader/
BurstDemod pipeline demo. Skips when the wfmgen CLI isn't built.

The whole capture (codes, payload, and every AWGN realization) is generated
from fixed seeds, so the blind Acquisition sweep's exact detections
(including any false alarms) reproduce identically run to run — these tests
pin qualitative behavior (every real burst decodes, no false alarm ever
passes CRC-16) rather than a specific false-alarm count, since that count
is sensitive to platform/SIMD floating-point differences the module
docstring's "Rough edges found" already flags as an open question
(doppler#394).
"""

import tempfile

import pytest

from doppler.examples import dsss_burst_pipeline_demo as demo

pytestmark = pytest.mark.skipif(
    demo.wfmgen_available() is None, reason="wfmgen CLI not built / on PATH"
)


def _generate():
    with tempfile.TemporaryDirectory() as tmp:
        return demo.generate_waveform(tmp)


def test_wfmgen_three_faces_agree():
    # generate_waveform() itself raises AssertionError if the CLI, the
    # from_file() composer, and the Segment/Composer object API diverge for
    # the same scene — reaching the length check means they agreed.
    rx, *_ = _generate()
    starts = demo.burst_starts()
    assert len(rx) == starts[-1] + demo.BURST_LEN + demo.GAPS[-1]


def test_acquisition_blind_sweep_finds_every_real_burst():
    rx, acq_code, _data_code, _payload_bits, _frame_bits = _generate()
    starts = demo.burst_starts()
    hits, _acq = demo.demo_acquisition(rx, acq_code)
    is_real = demo._label_hits(hits, starts)
    # Every true burst is discovered by the blind sweep, with zero prior
    # knowledge of where (or whether) it is in the stream.
    assert sum(is_real) == demo.N_BURSTS
    # No injected Doppler in this scene: every REAL hit lands on bin 0.
    assert all(h["dop"] == 0 for h, real in zip(hits, is_real) if real)


def test_despreader_tracks_real_bursts_with_correct_esn0():
    rx, acq_code, data_code, _payload_bits, frame_bits = _generate()
    starts = demo.burst_starts()
    hits, acq = demo.demo_acquisition(rx, acq_code)
    is_real = demo._label_hits(hits, starts)
    results = demo.demo_despreader(
        rx, hits, acq, acq_code, data_code, frame_bits
    )
    assert len(results) == len(hits)
    for r, real in zip(results, is_real):
        if not real:
            continue
        # BurstDespreader tracks continuously through the whole frame; a
        # handful of bit errors at Es/N0=10dB is normal, dozens would not
        # be.
        assert r["errs"] <= 5
        # The data-aided Es/N0 estimate should land near the Es/N0 the
        # scene was actually generated at — closes the loop between what
        # wfmgen was configured to produce and what the receiver measures.
        assert abs(r["esn0_db"] - demo.ESN0_DB) < 3.0


def test_burst_demod_decodes_every_real_burst_and_rejects_false_alarms():
    rx, acq_code, data_code, payload_bits, _frame_bits = _generate()
    starts = demo.burst_starts()
    hits, acq = demo.demo_acquisition(rx, acq_code)
    is_real = demo._label_hits(hits, starts)
    results = demo.demo_burst_demod(
        rx, hits, acq, acq_code, data_code, payload_bits
    )
    assert len(results) == len(hits)
    for (valid, errs), real in zip(results, is_real):
        if real:
            assert valid and errs == 0
        else:
            # A false alarm should never coincidentally pass CRC-16.
            assert not valid
