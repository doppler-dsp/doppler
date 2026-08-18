"""Fast pytest twin of the `pull_in` characterization subject.

Imports the subject's own helpers and runs a handful of trials, so the per-push
suite keeps its stimulus, its acquisition criteria and its harness wiring
working between deliberate `make characterize` runs.

**Be precise about what this buys.** It proves the sweep still RUNS; it does
not re-derive the envelope. A regression that moves a pull-in boundary while
every import still succeeds passes here and waits for the next
`make characterize` (doppler#692). What it does check is the one property the
envelope is stated in terms of, at the two points where being wrong would
matter most: seeded AT the bound both loops acquire, and seeded far outside it
the carrier loop does not — because a criterion that cannot fail would make
every fraction in the full sweep meaningless.
"""

from __future__ import annotations

import pytest

from doppler.track.tests.characterization.pull_in.characterize import (
    BN,
    ORDERS,
    run_carrier,
    run_timing,
    shoulders,
)


@pytest.mark.parametrize("m", ORDERS)
def test_carrier_acquires_at_the_bound(m):
    """At 1.0x `bn_carrier / m` the carrier loop acquires, at every order.

    This is the rule the tests and validators are held to, so it is the one
    point that must hold per push rather than per campaign.
    """
    assert run_carrier(m, 8, 1.0, 700), (
        f"M={m}: seeded AT its acquisition bound (bn_carrier/{m}) and the "
        f"carrier loop did not converge — the seeding rule every test in the "
        f"tree depends on no longer holds"
    )


def test_carrier_fails_far_outside_the_bound():
    """And it does NOT acquire at 8x, which is what makes the fractions real.

    Without this the sweep's criterion could be satisfied by anything and
    every success fraction in the full run would read 1.00 — the reject half
    of the pair, and the one that stops the twin passing vacuously.
    """
    assert not run_carrier(4, 8, 8.0, 700), (
        "seeded at 8x the bound the carrier loop reported acquisition; the "
        "criterion cannot distinguish pull-in from failure, so no success "
        "fraction the full sweep prints means anything"
    )


def test_timing_acquires_at_its_bound():
    """The timing loop's bound is `bn_timing` with no `m` in it."""
    assert run_timing(4, 8, 1.0, 900), (
        f"seeded at 1.0x bn_timing ({BN}) the timing loop did not steer back "
        f"to the true rate"
    )


def test_shoulders_reports_absence_rather_than_guessing():
    """`shoulders` must return None where the range found no boundary.

    Quoting the last swept point as a boundary is how a sweep reports its own
    range as though it were the object's.
    """
    assert shoulders({0.5: 1.0, 1.0: 1.0}) == (1.0, None)
    assert shoulders({0.5: 0.0, 1.0: 0.0}) == (None, 0.5)
    assert shoulders({0.5: 1.0, 1.0: 0.5, 2.0: 0.0}) == (0.5, 2.0)
