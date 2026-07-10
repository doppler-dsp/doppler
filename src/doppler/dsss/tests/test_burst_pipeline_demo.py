"""Integration test for the 5-burst wfmgen -> Acquisition/BurstDespreader/
BurstDemod pipeline demo. Skips when the wfmgen CLI isn't built.

BurstDemod's CRC pass rate is deliberately NOT asserted at full reliability
here — the module docstring documents why (a one-shot feedforward estimate
with no tracking loop, run against a payload long enough that residual
frequency error accumulates uncorrected phase drift). ``any(...)`` pins that
at least the best-case burst still decodes, without pinning the exact,
noise-realization-dependent pass count.
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


def test_acquisition_finds_every_burst_at_bin_zero():
    rx, acq_code, _data_code, _payload_bits, _frame_bits = _generate()
    starts = demo.burst_starts()
    hits = demo.demo_acquisition(rx, starts, acq_code)
    assert all(hit is not None for hit in hits)
    # No injected Doppler in this scene: every hit lands on bin 0.
    assert all(dop == 0 for dop, _cp, _acq in hits)


def test_despreader_tracks_every_burst_with_near_zero_errors():
    rx, acq_code, data_code, _payload_bits, frame_bits = _generate()
    starts = demo.burst_starts()
    hits = demo.demo_acquisition(rx, starts, acq_code)
    results = demo.demo_despreader(
        rx, starts, hits, acq_code, data_code, frame_bits
    )
    assert all(r is not None for r in results)
    # BurstDespreader tracks continuously through the whole frame; a handful
    # of bit errors at Es/N0=10dB is normal, dozens would not be.
    assert all(errs <= 5 for errs, _lock, _snr in results)


def test_burst_demod_runs_and_decodes_at_least_one_burst():
    rx, acq_code, data_code, payload_bits, _frame_bits = _generate()
    starts = demo.burst_starts()
    results = demo.demo_burst_demod(
        rx, starts, acq_code, data_code, payload_bits
    )
    assert len(results) == demo.N_BURSTS
    assert any(valid for valid, _errs in results)
