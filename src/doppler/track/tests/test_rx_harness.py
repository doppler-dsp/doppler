"""The harness's refusal point, tested by making it fire.

A receiver measurement fails by returning a PLAUSIBLE NUMBER, not an error, so
goal 1 of `docs/design/rx-test.md` asks the harness never to report one from an
untrustworthy state. The design's §8 storyboard drew that as TWO refusals -- an
output-rate invariant at stage 4, and `align_ok` at stage 6 -- and measurement
collapses them into one:

**`BerMeter.align()` already refuses a wrong-rate stream.** A stream carrying
half, double or `m_out` times the symbols it should cannot correlate against
the truth, and reports -2.5 dB, -inf and -5.6 dB of detection margin against
+10.5 dB for a healthy run. The stage-4 invariant would have been a second
gate with a second tolerance, for a question `ber` answers already -- which
is what §2.2 means by "a caller measuring their own receiver already has the
trio and the alignment decision". The rate cases are therefore tested HERE,
against the alignment, rather than against a count invariant that does not
exist.

What this file pins is that the refusal actually fires, since a refusal nobody
has watched fire is not a refusal.
"""

from __future__ import annotations

import numpy as np
import pytest

from ._mpsk_rx_harness import (
    IF_FS4,
    SETTLE_SYMS,
    coherent_errors,
    demod,
    detect_alignment,
    make_signal,
    symbol_metrics,
)

SPS = 16
M_OUT = 4
NSYM = 4000


def _run(real=False, told_sps=None):
    x, idx = make_signal(SPS, NSYM, real=real, m=4, fc=IF_FS4, esn0_db=None)
    y, pr = demod(x, real=real, sps=told_sps or SPS, m_out=M_OUT, m=4)
    return x, idx, y, pr


# ── the alignment must detect on a healthy run ──────────────────────────────


def test_alignment_is_detected_on_a_healthy_stream():
    """The baseline. Without it every refusal below could be passing because
    detection never works at all -- the way a reject test passes vacuously.
    """
    _x, idx, y, _pr = _run()
    meter = detect_alignment(y, idx, 4, SETTLE_SYMS)
    assert meter is not None
    assert meter.align_ok
    assert abs(meter.lag) < 8, f"lag {meter.lag} is not group delay"
    assert meter.align_margin_db > 5.0, f"{meter.align_margin_db:.1f} dB"
    assert not meter.align_saturated


def test_a_healthy_run_emits_one_symbol_per_symbol_period():
    """The rate contract, stated as a measurement rather than enforced twice.

    Measured 2 to 10 symbols short of `len(x)/sps` across every configuration
    these tests use -- cascade startup latency plus the timing loop absorbing
    the clock offset it was given. Stating it here keeps the number honest
    without putting a second tolerance in the measurement path.
    """
    x, _idx, y, _pr = _run()
    assert 0 <= len(x) // SPS - len(y) <= 16, f"{len(y)} symbols"


# ── the refusal ─────────────────────────────────────────────────────────────


@pytest.mark.parametrize("factor", [0.5, 2.0])
def test_a_stream_at_the_wrong_rate_is_refused(factor):
    """Being told the wrong `sps` must produce no number, from either scorer.

    This is the stage-4 failure, refused at stage 6. The receiver did emit one
    symbol per symbol period -- of the rate it was given -- so the run is
    entirely self-consistent and nothing about the symbols reveals that they
    are not the burst `idx` describes. Only the correlation against truth does.
    """
    x, idx = make_signal(SPS, NSYM, real=False, m=4, fc=IF_FS4, esn0_db=None)
    y, _pr = demod(x, real=False, sps=SPS * factor, m_out=M_OUT, m=4)
    assert len(y) == pytest.approx(NSYM / factor, rel=0.02)  # self-consistent
    assert detect_alignment(y, idx, 4, SETTLE_SYMS) is None
    assert coherent_errors(y, idx, 4, SETTLE_SYMS) is None
    # Which impossibility bites first depends on the direction: at half the
    # symbols there is no window left to measure in, at twice them there is a
    # window and nothing in it correlates. Both are refusals, and the point is
    # that neither returns a number.
    with pytest.raises(AssertionError, match=r"no alignment|too short"):
        symbol_metrics(y, idx, m=4, settle=SETTLE_SYMS)


def test_terminal_outputs_mistaken_for_symbols_are_refused():
    """`m_out` times too many symbols must not measure as a poor receiver.

    The terminal cascade rate is `m_out` outputs per symbol, which the timing
    loop consumes internally; taking those for symbol decisions gives `m_out`
    times too many, each a real matched-filter output and none of them a
    decision. Stood up here by repeating the stream, which is the shape of that
    mistake without needing a receiver that makes it.
    """
    _x, idx, y, _pr = _run()
    fat = np.repeat(y, M_OUT)
    assert detect_alignment(fat, idx, 4, SETTLE_SYMS) is None
    with pytest.raises(AssertionError, match="no alignment"):
        symbol_metrics(fat, idx, m=4, settle=SETTLE_SYMS)


def test_truth_from_another_draw_is_refused():
    """A perfectly good burst and a perfectly good truth that do not match.

    The minimum-over-lag search this replaced always returns its best lag here:
    over 401 candidates at QPSK it lands near 3/4, and 0.75 is a number a
    person can talk themselves into as "mostly failing". Detection reports that
    there is nothing to find.
    """
    _x, idx, y, _pr = _run()
    other = np.random.default_rng(99).integers(0, 4, idx.size).astype(np.uint8)
    assert detect_alignment(y, other, 4, SETTLE_SYMS) is None
    assert coherent_errors(y, other, 4, SETTLE_SYMS) is None
    with pytest.raises(AssertionError, match="no alignment"):
        symbol_metrics(y, other, m=4, settle=SETTLE_SYMS)


def test_a_window_too_short_to_measure_is_refused():
    """Fewer than 200 settled symbols is not a measurement.

    The old form returned `(nan, nan, None)`, and a NaN reads as a number in
    every f-string it lands in while comparing False against any threshold --
    so `assert evm < -18` fails pointing at the receiver rather than at the
    empty window.
    """
    _x, idx, y, _pr = _run()
    with pytest.raises(AssertionError, match="too short to measure"):
        symbol_metrics(y, idx, m=4, settle=len(y) - 100)


def test_the_marker_symbols_are_held_out_of_the_score():
    """The symbols that fixed the alignment must not also be scored.

    They are guaranteed to agree -- that is how they were chosen -- so scoring
    them flatters the rate by their share of the window. `BerMeter.score()`
    excludes them itself and reports the count in `skipped`; the harness's job
    is to let it, rather than to compute a window past the marker by hand.
    """
    _x, idx, y, _pr = _run()
    meter = detect_alignment(y, idx, 4, SETTLE_SYMS)
    assert meter is not None
    scored = meter.score(
        np.ascontiguousarray(y, np.complex64), lo=SETTLE_SYMS, hi=len(y)
    )
    assert meter.skipped >= 256, f"only {meter.skipped} held out"
    assert scored == len(y) - SETTLE_SYMS - meter.skipped
