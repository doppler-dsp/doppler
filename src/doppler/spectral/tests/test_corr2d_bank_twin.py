"""Fast twin of the bank's detection characterization.

``characterization/corr2d_bank/characterize.py`` measures, at the request's
geometry and over minutes of Monte Carlo, what detecting on the coarse
carrier grid and refining the candidate costs against the fine grid. This
runs the same ``measure`` at a 16 x 127 frame, seeded, in about a second,
and asserts the same two claims at that scale: every detector holds the
false-alarm rate it was calibrated to on noise it has not seen (not
significantly above it, nor collapsed below it), and
detect-and-refine detects as often as the fine grid does, within the
intervals. The straddle the refine recovers, exactly, is pinned
deterministically in ``test_corr2d_core.c`` (``_test_bank_detect``).
"""

from __future__ import annotations

from doppler.spectral.tests.characterization.corr2d_bank.characterize import (
    Geometry,
    measure,
)

#: 16 periods of a 127-chip code, at a candidate rate and SNRs where the
#: oracle detects 0.8-0.9 of the time (measured with this seed).
SMALL = Geometry(16, 7, 0.1, 0.3, {"uniform": -19.5, "worst": -21.0})


def test_every_detector_holds_its_pfa_and_the_refine_matches_l4() -> None:
    r = measure(SMALL, trials=120, noise_frames=300)
    # Held: not significantly above the target on fresh noise (what a
    # detector owes its caller), and not collapsed far below it (a broken
    # calibration). Two-sided at the target would ignore that the threshold
    # is itself a quantile of 300 frames, with its own error.
    for name, (p, lo, hi) in r["pfa"].items():
        assert lo <= SMALL.pfa and p >= SMALL.pfa / 3, (
            f"{name}: Pfa {p:.3f} [{lo:.3f}, {hi:.3f}] vs {SMALL.pfa}"
        )
    for mode, pd in r["pd"].items():
        _, lo4, hi4 = pd["L4"]
        p, lo, hi = pd["refine"]
        assert lo <= hi4 and lo4 <= hi, (
            f"{mode}: refine {p:.3f} [{lo:.3f}, {hi:.3f}] apart from "
            f"L4 [{lo4:.3f}, {hi4:.3f}]"
        )
