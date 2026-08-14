"""loop_filter_bandwidth_demo.py — the loop you asked for vs the one you get.

:class:`doppler.track.LoopFilter` takes a **loop noise bandwidth** ``bn`` and
an update period ``t``, and every tracking loop in the library sizes its
settling and its jitter off that promise. This demo measures whether the
promise holds, and hands back the one number a caller has to design to.

The measurement is exact rather than statistical. Drive the canonical loop —
a unit detector, this filter, an oscillator that integrates the control —
with a single impulse, and the estimate *is* the closed-loop impulse
response ``h``. Parseval then gives the noise bandwidth outright::

    sum h[n]^2  =  integral of |H(f)|^2  =  2 * Bn      ->  Bn = 0.5 * sum h^2

no noise realisation, no averaging, no fitting.

The answer, from
``src/doppler/track/tests/validation/loop_filter`` §2.2-§2.4:

    the delivered bandwidth is always slightly WIDE, never narrow, by

        Bn / (bn*t)  -  1  ~=  16*zeta^2 / (4*zeta^2 + 1)^2  *  (bn*t)

Two things make that useful rather than merely true. It depends only on the
**product** ``bn*t``, so changing how often a loop updates does not change
the loop — which is what lets one ``bn_carrier`` mean the same thing at every
tap of a receiver. And it is always in the safe direction: a caller sizing
noise off ``bn`` is being conservative, not optimistic.

Solved for a budget at the damping everything here uses::

    zeta = 0.707:   bn*t <= 0.0112  ->  within 1%
                    bn*t <= 0.0552  ->  within 5%

Every configuration shipped in this library sits inside the 1% figure.

Run it::

    python src/doppler/examples/loop_filter_bandwidth_demo.py
"""

from __future__ import annotations

import numpy as np

from doppler.track import LoopFilter

ZETA = 0.707


def excess_law(zeta: float) -> float:
    """Fractional bandwidth excess per unit of ``bn*t``, in closed form."""
    return 16.0 * zeta * zeta / (4.0 * zeta * zeta + 1.0) ** 2


def measured_bn(bn: float, zeta: float, t: float) -> float:
    """Closed-loop noise bandwidth in cycles per update, by Parseval.

    The loop is closed sample by sample — each error depends on the previous
    estimate — so this walks the impulse response one update at a time
    through the real object rather than filtering a prepared array.
    """
    lf = LoopFilter(bn=bn, zeta=zeta, t=t)
    n = max(int(400.0 / (bn * t)), 1000)
    phi = 0.0
    sum2 = 0.0
    one = np.ones(1, dtype=np.float64)
    for k in range(n):
        sum2 += phi * phi
        err = (1.0 if k == 0 else 0.0) - phi
        phi += float(lf.steps(one * err)[0])
    return 0.5 * sum2


def settling_updates(bn: float, zeta: float, tol: float = 0.05) -> int:
    """Updates until a unit step is inside ``tol`` and STAYS there.

    Last excursion rather than first arrival: a ``zeta = 0.707`` loop
    overshoots, and a first-arrival measure would report the overshoot's
    outbound crossing as the answer.
    """
    lf = LoopFilter(bn=bn, zeta=zeta, t=1.0)
    one = np.ones(1, dtype=np.float64)
    phi = 0.0
    last = 0
    for k in range(int(60.0 / bn)):
        phi += float(lf.steps(one * (1.0 - phi))[0])
        if abs(phi - 1.0) > tol:
            last = k
    return last


def main() -> None:
    print("Loop filter — the bandwidth you asked for vs the one you get")
    print(f"(zeta = {ZETA}, t = 1.0, so the group bn*t is just bn)\n")

    coeff = excess_law(ZETA)
    print(f"  closed-form excess coefficient: {coeff:.4f} per unit bn*t")
    print(f"  1% budget: bn*t <= {0.01 / coeff:.4f}")
    # Solving `coeff * bn*t = target` is a LINEAR extrapolation, and the
    # excess curves upward, so it is only trustworthy while the excess is
    # small. At 1% the two agree (0.0112); at 5% the linear solve says
    # 0.0562 against a measured crossing of 0.0552, and the measured one is
    # the number the header quotes. Printing both would be two answers to
    # one question, so this prints the regime where they are the same.
    print("  5% budget: bn*t <= 0.0552  (measured crossing; the linear")
    print("             solve reads 2% high by here, as the excess curves)\n")

    print(
        f"  {'bn':>8} {'measured Bn':>13} {'Bn/bn':>8} "
        f"{'predicted':>10} {'verdict':>10}"
    )
    inside, outside = [], []
    for bn in (0.001, 0.005, 0.01, 0.02, 0.05):
        got = measured_bn(bn, ZETA, 1.0)
        ratio = got / bn
        predicted = 1.0 + coeff * bn
        ok = "within 1%" if ratio <= 1.01 else "wide"
        print(
            f"  {bn:>8g} {got:>13.6g} {ratio:>8.4f} "
            f"{predicted:>10.4f} {ok:>10}"
        )
        (inside if bn <= 0.0112 else outside).append((bn, ratio, predicted))

    # The promise: inside the budget the bandwidth is the one requested.
    for bn, ratio, _ in inside:
        assert 1.0 <= ratio <= 1.01, (
            f"bn={bn} is inside the 1% budget but delivered {ratio:.4f}x"
        )

    # The law: it predicts what happens OUTSIDE the budget too, which is what
    # makes it a design rule rather than a tolerance someone picked.
    for bn, ratio, predicted in inside + outside:
        assert abs(ratio - predicted) < 0.1 * (predicted - 1.0) + 1e-6, (
            f"bn={bn}: measured {ratio:.4f} against predicted {predicted:.4f} "
            "— the excess no longer follows its closed form"
        )

    # Never narrow. A caller sizing jitter off bn must not be flattered.
    for bn, ratio, _ in inside + outside:
        assert ratio >= 1.0, f"bn={bn} delivered a NARROW loop ({ratio:.4f}x)"

    print("\n  Only the product bn*t matters — t drops out:")
    a = measured_bn(0.02, ZETA, 0.25) / (0.02 * 0.25)
    b = measured_bn(0.005, ZETA, 1.0) / (0.005 * 1.0)
    print(f"    (bn=0.02, t=0.25) -> {a:.6f}")
    print(f"    (bn=0.005, t=1.0) -> {b:.6f}")
    assert abs(a - b) < 1e-6 * b, "t failed to drop out of the excess"

    print("\n  Settling, in loop constants (n*bn) — flat across bandwidth:")
    consts = []
    for bn in (0.001, 0.005, 0.01, 0.05):
        n = settling_updates(bn, ZETA)
        consts.append(n * bn)
        print(f"    bn={bn:<7g} {n:>6} updates   = {n * bn:.2f} constants")
    assert max(consts) <= 6.0, (
        f"settling reached {max(consts):.2f} loop constants; the 5/bn rule "
        "the rest of the library sizes windows with no longer holds"
    )
    print(
        f"\n  Spread {min(consts):.2f}-{max(consts):.2f} constants over a 50x "
        "range of bn, so `5/bn` is comfortable rather than tight."
    )
    print("\nOK — bandwidth, its law, and the settling rule all hold.")


if __name__ == "__main__":
    main()
