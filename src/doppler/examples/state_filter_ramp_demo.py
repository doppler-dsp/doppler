"""state_filter_ramp_demo.py — a ramp the second-order loop cannot hold.

A carrier loop that updates once per code period and needs a narrow noise
bandwidth meets a Doppler that drifts: the carrier frequency is a ramp. The
second-order filter (:class:`doppler.track.LoopFilter`) nulls a frequency
*step* but not a ramp; it settles to a steady phase error of ``r / ki`` for a
ramp of ``r`` cycles per update per update, and ``ki`` falls as the square of
the bandwidth. Narrow the loop to ride out the noise and the lag grows
with it.

:class:`doppler.track.StateFilter` adds a rate estimate in cascade with the
frequency estimate, so a frequency ramp is something it *tracks*: the steady
phase error is zero, at the same noise bandwidth, from the same two numbers
(``bn`` and ``t``).

This demo closes both loops on a ramp, checks each against the closed form
the object owns, and reads the loop's exact bandwidth and stability for the
detector delay and loop gain a real receiver presents.

Run it::

    python src/doppler/examples/state_filter_ramp_demo.py
"""

from __future__ import annotations

import numpy as np

from doppler.track import LoopFilter, StateFilter

# --8<-- [start:ramp]
BN = 0.02  # loop noise bandwidth, cycles per update
RAMP = (
    2e-7  # carrier frequency drifts by this many cycles per update, per update
)


def close_the_loop(loop, n=40_000):
    """Drive ``loop`` with a frequency ramp; return the last phase error.

    The detector is ideal: ``e = input_phase - oscillator_phase``. The
    oscillator accumulates the loop's control output.
    """
    freq = phase_in = phase_osc = err = 0.0
    for _ in range(n):
        freq += RAMP
        phase_in += freq
        err = phase_in - phase_osc
        phase_osc += loop.step(err)
    return err


second = LoopFilter(bn=BN, zeta=0.707, t=1.0)
third = StateFilter(bn=BN, t=1.0)
err2 = close_the_loop(second)
err3 = close_the_loop(third)
# --8<-- [end:ramp]

# --8<-- [start:exact]
# The loop's exact delivered noise bandwidth and its stability, for a
# detector that reads the error one update late and a slope of 0.7 of
# nominal: the numbers a caller sizes from, not a measurement.
bw_ideal = third.bandwidth()
bw_late = third.bandwidth(delay=1.0)
bw_low = third.bandwidth(gain=0.7)
# --8<-- [end:exact]


def main() -> None:
    print(f"ramp {RAMP:g} cycles/update^2 into a loop of bn = {BN}")
    print(f"  second order: steady phase error {err2:+.3e} cycles")
    print(f"                predicted r/ki       {RAMP / second.ki:+.3e}")
    print(f"  third order:  steady phase error {err3:+.3e} cycles")
    print(
        f"exact bandwidth: ideal {bw_ideal:.5f}, one update late "
        f"{bw_late:.5f}, slope 0.7 {bw_low:.5f}"
    )

    # The second-order lag is the closed form, to the digits it settles to.
    assert abs(err2 / (RAMP / second.ki) - 1.0) < 1e-4
    # The third-order loop nulls the ramp: its error is orders of magnitude
    # under the second-order loop's at the same bandwidth.
    assert abs(err3) < 1e-6 * abs(err2)
    # The delivered bandwidth is the one asked for at zero delay; a late
    # detector widens the loop and a low slope narrows it.
    assert abs(bw_ideal / BN - 1.0) < 1e-6
    assert bw_late > bw_ideal > bw_low
    # The automatic case is stable over the rectangle it documents.
    edge = StateFilter(bn=0.12)
    assert all(
        edge.stable(delay=d, gain=g) == 1
        for d in np.linspace(0.0, 2.0, 9)
        for g in (0.5, 1.0, 2.0)
    )
    print("OK: the ramp is nulled, and the loop is stable over its rectangle")


if __name__ == "__main__":
    main()
