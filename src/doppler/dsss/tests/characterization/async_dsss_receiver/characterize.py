"""Characterization: `AsyncDsssReceiver` at extreme Es/N0, trying to break it.

A collapsing noise estimate is the classic high-SNR failure: a CFAR
threshold or a ``|P|``-normaliser whose denominator tends to zero blows up to
inf/NaN. This receiver guards every ratio's denominator, so fed a signal far
above its design point it should stay finite, lock, and decode. This sweep is
how that claim is checked: a receiver designed at a normal Es/N0 = 20 dB is
fed the SPEC geometry's 500 Hz/s Doppler ramp from 20 dB up to effectively
noiseless (200 dB), over several seeds, at the full capture length.

It is a characterization rather than a unit test because it is a stress
sweep, not a contract of one operating point, and because it streams ~5.5 M
samples per trial -- cheap optimised (~0.7 s), minutes under coverage
instrumentation, where it used to be one of the slowest things in the
coverage job's Python leg. `make characterize` runs it deliberately; the
fast twin in ``test_async_dsss_receiver.py`` runs the one extreme point
(noiseless, a third of the length) on every push.

Measured, and reported rather than asserted: constellation quality
saturates at an implementation floor (~-24 dB EVM / ~22 dB effective SNR,
from the point-sample 2x replica's matched-filter loss, the async-symbol
straddle and NCO quantisation); more SNR past ~40 dB neither improves nor
destabilises it.

Run:  make characterize   (or: uv run python <this file>)
"""

from __future__ import annotations

import numpy as np

from doppler.dsss.tests.test_async_dsss_receiver import (
    N_SYM,
    SYM_RATE,
    _best_ber,
    _make_ramp_signal,
    _new_receiver,
    _stream,
)

DESIGN_ESN0_DB = 20.0  # the receiver's own, normal design point
ESN0_SWEEP_DB = (20.0, 40.0, 60.0, 80.0, 120.0, 160.0, 200.0)
SEEDS = (21, 22, 23, 24, 25)


def _cn0(esn0_db: float) -> float:
    return esn0_db + 10.0 * np.log10(SYM_RATE)


def run_trial(esn0_db: float, seed: int = 21, n_sym: int = N_SYM) -> dict:
    """One capture at *esn0_db* into a receiver designed at 20 dB.

    Returns what the stability claim is about: whether it tracks, how many
    symbols came out, whether every one is finite, and the decode BER.
    """
    x, data = _make_ramp_signal(_cn0(esn0_db), seed=seed, n_sym=n_sym)
    rx = _new_receiver(_cn0(DESIGN_ESN0_DB))
    syms = _stream(rx, x)
    return {
        "esn0_db": esn0_db,
        "seed": seed,
        "tracking": int(rx.tracking),
        "n_syms": len(syms),
        "n_sym": n_sym,
        "finite": bool(np.all(np.isfinite(syms.view(np.float64)))),
        "ber": _best_ber(syms, data),
    }


def stable(r: dict) -> bool:
    """The claim, for one trial: tracking, finite, most symbols, clean."""
    return (
        r["tracking"] == 1
        and r["finite"]
        and r["n_syms"] > r["n_sym"] // 2
        and r["ber"] < 0.01
    )


def main() -> int:
    print(
        f"AsyncDsssReceiver designed at Es/N0 = {DESIGN_ESN0_DB:.0f} dB, fed "
        f"{N_SYM} symbols of a 500 Hz/s ramp; {len(SEEDS)} seeds per level\n"
    )
    print("  Es/N0 dB   stable   worst BER   min symbols")
    failed = 0
    for esn0 in ESN0_SWEEP_DB:
        rs = [run_trial(esn0, s) for s in SEEDS]
        ok = sum(stable(r) for r in rs)
        failed += len(rs) - ok
        print(
            f"  {esn0:8.0f}   {ok:2d}/{len(rs):<2d}    "
            f"{max(r['ber'] for r in rs):9.4f}   "
            f"{min(r['n_syms'] for r in rs):11d}"
        )
    print(f"\n{'STABLE' if failed == 0 else f'{failed} UNSTABLE trial(s)'}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
