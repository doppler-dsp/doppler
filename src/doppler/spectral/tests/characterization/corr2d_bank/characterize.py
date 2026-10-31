"""Characterization of the hypothesis bank's detection loss: one stage on
the fine carrier grid (L = 4), one stage on the coarse one (L = 2), and the
bank's own detect-coarse-refine-the-candidate (``bank_detect``, 2 -> 4),
against an oracle that corrects the exact carrier.

This is the measurement behind ``docs/design/corr2d-hypotheses.md`` section
6 and ruling 9 on #2683. A repeated PN m-sequence (511 chips, the shipped
``PN``) on a carrier between grid points, its delay drawn uniformly, in the
shipped ``AWGN``. Every detector's thresholds come from its own noise-only
frames at one false-alarm rate per search (every hypothesis and cell of a
frame), and each detector's rate is then re-measured on fresh noise. Pd is
read at one SNR per carrier mode, chosen where the oracle detects about 0.9
of the time, with a Wilson 95% interval on every value.

Two carrier modes: ``uniform`` draws the fraction uniformly, and ``worst``
places it exactly between two coarse-grid points (a quarter bin), where the
coarse grid's straddle loss is largest and the refine has the most to
recover.

This is a **characterization**: at the request's geometry it is minutes of
Monte Carlo, run by ``make characterize``. The per-push fast twin is
``spectral/tests/test_corr2d_bank_twin.py``, which runs ``measure`` at a
small geometry and asserts the same claims.

Run:
  python -m doppler.spectral.tests.characterization.corr2d_bank.characterize
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from doppler.source import AWGN
from doppler.spectral import DelayDoppler
from doppler.wfm import PN, rotate, wfm_awgn_amplitude

NO_RATES = np.empty(0, np.complex64)
#: Whole-bin carrier hypotheses: the signal's own bin and one either side.
WHOLE_BINS = (-1, 0, 1)


@dataclass(frozen=True)
class Geometry:
    """One frame's shape and the operating point it is measured at."""

    ny: int  #: code periods per frame
    pn_length: int  #: the m-sequence's register; nx = 2**pn_length - 1
    pfa: float  #: false alarms per search, every detector
    pfa_candidate: float  #: bank_detect's candidate rate, before the refine
    snr_db: dict  #: per-sample SNR for each carrier mode

    @property
    def nx(self) -> int:
        return 2**self.pn_length - 1


#: The request's geometry (#2683): 64 periods of a 511-chip code.
FULL = Geometry(64, 9, 1e-3, 1e-2, {"uniform": -29.0, "worst": -30.7})


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float]:
    """The Wilson score interval for k successes in n trials."""
    if n == 0:
        return 0.0, 1.0
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return max(0.0, c - h), min(1.0, c + h)


class Bench:
    """The code, the correlator, and the three detectors' statistics."""

    def __init__(self, geo: Geometry, nthreads: int = 1) -> None:
        self.geo = geo
        chips = np.asarray(PN(seed=1, length=geo.pn_length).generate(geo.nx))
        self.code = (1.0 - 2.0 * chips).astype(np.complex64)
        self.dd = DelayDoppler(self.code, ny=geo.ny, nthreads=nthreads)
        self.n = geo.ny * geo.nx
        # carriers k*L .. k*L + L - 1 for every whole bin k, on each grid
        k0, k1 = WHOLE_BINS[0], WHOLE_BINS[-1]
        self.fine = (4 * k0, 4 * k1 + 3)
        self.coarse = (2 * k0, 2 * k1 + 1)

    def frame(self, phi: float, snr_db: float | None, seed: int):
        """The code at a uniform delay on carrier phi/nx, at ``snr_db`` per
        sample over unit-power noise; None is noise alone.  The noise is the
        same power in every frame, so one set of thresholds serves all."""
        rng = np.random.default_rng(seed)
        d = int(rng.integers(self.geo.nx))
        amp = float(wfm_awgn_amplitude(0.0, 1.0))  # unit total noise power
        x = AWGN(seed=seed + 1, amplitude=amp).generate(self.n)
        if snr_db is not None:
            s = np.tile(np.roll(self.code, d), self.geo.ny)
            s = rotate(s, phi / self.geo.nx, fs=1.0)
            x = x + 10 ** (snr_db / 20) * s
        return np.asarray(x, np.complex64)

    def oracle(self, x: np.ndarray, phi: float) -> float:
        """The strongest cell with the exact carrier corrected, within each
        period, as the bank corrects."""
        n = np.arange(self.geo.nx)
        y = x * np.tile(
            np.exp(-2j * np.pi * phi * n / self.geo.nx), self.geo.ny
        )
        m = self.dd.execute(y.astype(np.complex64))
        return float(np.max(m.real**2 + m.imag**2))

    def stats(self, x: np.ndarray, t1: float | None) -> dict:
        """One frame's statistic under each detector. ``refined`` is the
        strongest record bank_detect keeps at candidate threshold ``t1`` with
        confirm at 0 (so a caller's confirm threshold is a comparison against
        it), or 0 with none; with ``t1`` None it is not run."""
        f4, _ = self.dd.bank_power_max(x, 4, *self.fine, NO_RATES)
        f2, _ = self.dd.bank_power_max(x, 2, *self.coarse, NO_RATES)
        refined = 0.0
        if t1 is not None:
            p, *_ = self.dd.bank_detect(
                x, 2, 4, *self.coarse, t1, 0.0, NO_RATES
            )
            refined = float(p.max()) if len(p) else 0.0
        return {
            "L4": float(f4.max()),
            "L2": float(f2.max()),
            "refined": refined,
        }


def thresholds(bench: Bench, frames: int, seed: int) -> dict:
    """Each detector's thresholds from its own noise-only frames: L4 and L2
    at the search's Pfa; bank_detect's candidate at its candidate rate, and
    its confirm where the overall rate is the search's Pfa."""
    geo = bench.geo
    noise = [bench.frame(0.0, None, seed + 2 * i) for i in range(frames)]
    s = [bench.stats(x, None) for x in noise]
    c2 = np.array([v["L2"] for v in s])
    t1 = float(np.quantile(c2, 1 - geo.pfa_candidate))
    refined = np.array([bench.stats(x, t1)["refined"] for x in noise])
    passed = refined[c2 > t1]
    q = 1 - geo.pfa / geo.pfa_candidate
    return {
        "L4": float(np.quantile([v["L4"] for v in s], 1 - geo.pfa)),
        "L2": float(np.quantile(c2, 1 - geo.pfa)),
        "t1": t1,
        "t2": float(np.quantile(passed, q)) if len(passed) else math.inf,
    }


def detects(bench: Bench, x: np.ndarray, th: dict) -> dict:
    """Each detector's decision, through the shipped API: L4 and L2 against
    their thresholds, and bank_detect with its own candidate AND confirm
    thresholds, deciding by whether it keeps a record."""
    f4, _ = bench.dd.bank_power_max(x, 4, *bench.fine, NO_RATES)
    f2, _ = bench.dd.bank_power_max(x, 2, *bench.coarse, NO_RATES)
    p, *_ = bench.dd.bank_detect(
        x, 2, 4, *bench.coarse, th["t1"], th["t2"], NO_RATES
    )
    return {
        "L4": float(f4.max()) > th["L4"],
        "L2": float(f2.max()) > th["L2"],
        "refine": len(p) > 0,
    }


def measure(
    geo: Geometry, trials: int, noise_frames: int, seed: int = 2683
) -> dict:
    """Pd per detector and carrier mode, and Pfa on fresh noise, with
    Wilson intervals; ``oracle`` is held to the L4 threshold."""
    bench = Bench(geo)
    th = thresholds(bench, noise_frames, seed)
    out: dict = {"thresholds": th, "pd": {}, "pfa": {}}
    for mode, snr in geo.snr_db.items():
        hits = {"oracle": 0, "L4": 0, "L2": 0, "refine": 0}
        rng = np.random.default_rng(seed + (7 if mode == "worst" else 3))
        for t in range(trials):
            phi = 0.25 if mode == "worst" else float(rng.uniform(-0.5, 0.5))
            x = bench.frame(phi, snr, seed + 100_003 * (t + 1))
            hits["oracle"] += bench.oracle(x, phi) > th["L4"]
            for k, v in detects(bench, x, th).items():
                hits[k] += v
        out["pd"][mode] = {
            k: (v / trials, *wilson(v, trials)) for k, v in hits.items()
        }
    fa = {"L4": 0, "L2": 0, "refine": 0}
    for i in range(noise_frames):
        x = bench.frame(0.0, None, seed + 7_000_001 + 2 * i)
        for k, v in detects(bench, x, th).items():
            fa[k] += v
    out["pfa"] = {
        k: (v / noise_frames, *wilson(v, noise_frames)) for k, v in fa.items()
    }
    return out


def report(geo: Geometry, r: dict) -> str:
    lines = [
        f"bank detection, ny {geo.ny}, nx {geo.nx}, whole bins "
        f"{WHOLE_BINS}, Pfa {geo.pfa:g} per search",
        "",
        "| mode | SNR/sample | oracle | L = 4 | L = 2 | detect 2 -> 4 |",
        "|---|---|---|---|---|---|",
    ]
    for mode, pd in r["pd"].items():
        cells = [
            f"{pd[k][0]:.3f} [{pd[k][1]:.3f}, {pd[k][2]:.3f}]"
            for k in ("oracle", "L4", "L2", "refine")
        ]
        lines.append(
            f"| {mode} | {geo.snr_db[mode]:.1f} dB | "
            + " | ".join(cells)
            + " |"
        )
    lines += ["", "| detector | Pfa on fresh noise |", "|---|---|"]
    for k, v in r["pfa"].items():
        lines.append(f"| {k} | {v[0]:.2e} [{v[1]:.1e}, {v[2]:.1e}] |")
    return "\n".join(lines)


if __name__ == "__main__":
    print(report(FULL, measure(FULL, trials=1000, noise_frames=20_000)))
