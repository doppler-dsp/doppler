"""Fast twin for the ``dsss_burst_receiver`` characterization subject.

`make characterize` runs the full sweep — a whole acquisition frame of burst
offsets for two spreading codes, plus a ten-point C/N0 curve at 24 trials a
point — and that costs minutes, so it is run deliberately rather than on
every push. This is the per-push cover for it: it imports the same helpers,
the same geometry and the same scene builder, so the script cannot silently
stop importing or stop working while nobody is looking.

Be clear about the scope, which is the trade the characterization category
exists to make (see ``doppler.dsss.tests.characterization``): this proves the
helpers still run and that the two anchor points of the envelope are still on
the right side of the knee. It does **not** re-derive the envelope — a
regression that moves the knee a few dB without breaking an import is caught
by ``make characterize``, not here.
"""

import numpy as np

from doppler.dsss.tests.characterization.dsss_burst_receiver.characterize import (  # noqa: E501  # noqa: E501
    ACQ_FRAME,
    ACQ_SF,
    BASE_AT,
    BURST_LEN,
    CODE_PERIOD,
    OFFSET_SIGMA,
    REPS,
    _codes,
    _run_one,
    _structured_codes,
    cn0_dbhz,
    peak_to_sidelobe,
    sweep_offset,
)

# Far above the knee: the burst must decode, exactly, every time.
CLEAN_SIGMA = 0.02
# A handful of offsets, not a whole frame — enough to exercise the sweep.
FAST_OFFSETS = 6


def test_geometry_is_the_declared_one() -> None:
    """The subject's geometry still derives the way the sweep assumes."""
    assert CODE_PERIOD == ACQ_SF * 4  # spc = 4
    assert ACQ_FRAME == REPS * CODE_PERIOD
    assert BURST_LEN > ACQ_FRAME
    # The sweep's base offset must sit on a frame boundary, or "offset
    # within one frame" means something different every run.
    assert BASE_AT % ACQ_FRAME == 0


def test_the_codes_are_what_the_finding_rests_on() -> None:
    """The good/poor code contrast is the sweep's headline finding (F6).

    If the two codes ever stopped differing in autocorrelation the panel
    would still plot, and would show nothing.

    Note what "good" means HERE: the subject's reference code is a RANDOM
    one (peak/worst-sidelobe 2.07), not the m-sequence the certification
    uses (31). That is deliberate for this panel — the contrast being drawn
    is against a code that merely looks deterministic, and a random code is
    the honest middle. Do not import a number from the certification into
    this file; they are different codes.
    """
    good = peak_to_sidelobe(_codes()[0])
    poor = peak_to_sidelobe(_structured_codes()[0])
    assert good > 1.8, f"the reference code got worse ({good:.3f})"
    assert poor < 1.5, f"the poor code is no longer poor ({poor:.3f})"
    assert good > 1.5 * poor, (
        f"the contrast the panel rests on has collapsed: {good:.3f} "
        f"against {poor:.3f}"
    )


def test_cn0_conversion_is_monotone_in_sigma() -> None:
    """More noise is less C/N0, and the sweep's operating point is near the
    knee rather than 40 dB above it — the correction that made the offset
    sweep mean anything (doppler#1006)."""
    a, b = cn0_dbhz(0.02), cn0_dbhz(OFFSET_SIGMA)
    assert a > b
    assert 50.0 < b < 70.0, f"OFFSET_SIGMA is off the knee: {b:.1f} dB-Hz"


def test_a_clean_burst_decodes_at_its_exact_sample() -> None:
    """The trial driver still works end to end, well above the knee."""
    for k in range(3):
        exact, valid, margin = _run_one(
            BASE_AT + k * CODE_PERIOD, sigma=CLEAN_SIGMA, seed=100 + k
        )
        assert exact, "preamble_start is not the burst's own sample"
        assert valid, "CRC failed on a clean burst"
        # Resolved, not merely decoded: the runner-up period must lose.
        assert 0.0 < margin < 0.9


def test_offset_sweep_runs_and_the_good_code_wins() -> None:
    """A short slice of the real sweep, at the real operating point.

    The assertion is the ORDERING, not a rate: the sweep's own numbers move
    with the noise realization, and pinning one here would duplicate
    `make characterize` badly rather than cover it.
    """
    offs, ok = sweep_offset(_codes())
    assert offs.size > FAST_OFFSETS
    assert ok.dtype == np.dtype(bool)
    good_rate = float(ok.mean())

    _, ok_poor = sweep_offset(_structured_codes())
    poor_rate = float(ok_poor.mean())

    assert good_rate > poor_rate, (
        f"the m-sequence ({good_rate:.0%}) no longer beats the structured "
        f"code ({poor_rate:.0%}) — the subject's headline finding"
    )
