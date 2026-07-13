"""plan_demo.py — SNR-sweep stimulus with the wfm ``Plan`` component cache.

A system-evaluation workflow re-runs *one* scene at many operating points — a
detection or BER curve is a sweep over SNR, each point averaged over
independent noise draws. The signal DSP is identical at every point; only the
noise floor moves. :func:`prepare` renders each source once and caches it, so
every sweep point is a cheap re-weighted sum instead of a full re-synthesis —
bit-identical to a full compose, and far faster once the scene has real signal
work in it (here a five-user CDMA burst plus a pilot tone).

The demo builds one scene, prepares it, then sweeps SNR × Monte-Carlo, at each
point matched-filtering the wanted user's spreading code and recording the
correlation-peak SNR. It also times *stimulus generation* — the Plan against
re-composing every point — to show the speedup. Two views (saved to a PNG):

  * **Detection curve** — measured matched-filter peak SNR vs requested channel
    SNR, mean ± std over the Monte-Carlo draws. The cache reproduces the exact
    noise power the resolver placed, so the curve tracks the sweep faithfully.
  * **Monte-Carlo cloud** — the per-draw peak SNR at each point: the
    independent noise realizations a single Plan generates from a seed sweep.

Run:  python -m doppler.examples.plan_demo  [out.png]
"""

from __future__ import annotations

import sys
import time

import numpy as np

from doppler.wfm import Composer, Segment, prepare, qpsk, tone

FS = 1e6
N = 8192
SNRS = np.arange(-6.0, 15.0, 3.0)  # channel SNR sweep (dB)
DRAWS = 12  # Monte-Carlo noise realizations per SNR
USERS = 5  # co-channel CDMA users (distinct spreading seeds)


def _sources() -> list:
    """The wanted user (carries SNR = the anchor) + interferers + a pilot."""
    wanted = qpsk(snr=0.0, seed=7, sps=8, pn_length=9)
    interferers = [
        qpsk(seed=100 + k, sps=8, pn_length=9, level=-3.0)
        for k in range(USERS - 1)
    ]
    pilot = tone(freq=2.2e5, seed=3, level=-10.0)
    return [wanted, *interferers, pilot]


def _scene() -> Composer:
    return Composer(Segment.sum(*_sources(), fs=FS, num_samples=N))


def _peak_snr(x: np.ndarray, template: np.ndarray) -> float:
    """Matched-filter peak SNR (dB): |peak|^2 over the mean off-peak power."""
    c = np.abs(np.correlate(x, template, mode="valid"))
    pk = int(c.argmax())
    peak = c[pk] ** 2
    mask = np.ones(c.size, dtype=bool)
    mask[max(0, pk - 8) : min(c.size, pk + 9)] = False
    floor = np.mean(c[mask] ** 2) + 1e-30
    return 10.0 * np.log10(peak / floor)


def sweep() -> tuple[np.ndarray, np.ndarray, np.ndarray, float, float]:
    """Return (mean, std, cloud, t_plan, t_recompose) over the SNR sweep."""
    scene = _scene()
    # A clean copy of the wanted user alone is the matched-filter template.
    template = prepare(scene).render(enable=[i == 0 for i in range(USERS)])[
        : N // 2
    ]
    template = template / (np.linalg.norm(template) + 1e-30)

    # --8<-- [start:sweep]
    plan = prepare(scene)  # render + cache every source ONCE
    # The cache's contract, asserted: a Plan render is bit-for-bit
    # identical to a full compose of the same scene.
    assert np.array_equal(plan.render(), scene.compose()), (
        "Plan.render() is not bit-identical to Composer.compose()"
    )
    # Independent draws: the same operating point re-rendered with
    # different noise seeds must give distinct realizations.
    draws = {
        np.asarray(plan.at(float(SNRS[0]), s)).tobytes()
        for s in range(1000, 1000 + DRAWS)
    }
    assert len(draws) == DRAWS, "noise seeds did not give distinct draws"
    mean = np.empty(SNRS.size)
    std = np.empty(SNRS.size)
    cloud = np.empty((SNRS.size, DRAWS))
    for i, snr in enumerate(SNRS):
        # each draw is a cheap re-weighted sum, not a re-synthesis
        peaks = [
            _peak_snr(plan.at(snr, seed), template)
            for seed in range(1000 + i * DRAWS, 1000 + (i + 1) * DRAWS)
        ]
        cloud[i] = peaks
        mean[i], std[i] = np.mean(peaks), np.std(peaks)
    # --8<-- [end:sweep]

    # ── timing: stimulus GENERATION only (no measurement in either loop) ──
    seeds = list(range(DRAWS))
    t0 = time.perf_counter()
    for snr in SNRS:
        for seed in seeds:
            plan.at(float(snr), 1000 + seed)
    t_plan = time.perf_counter() - t0

    t0 = time.perf_counter()
    for _snr in SNRS:
        for _seed in seeds:
            Composer(Segment.sum(*_sources(), fs=FS, num_samples=N)).compose()
    t_recompose = time.perf_counter() - t0
    return mean, std, cloud, t_plan, t_recompose


def main(out: str = "plan_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    mean, std, cloud, t_plan, t_recompose = sweep()

    # ── self-validation: the detection curve behaves physically ─────────
    # The matched-filter peak SNR must rise monotonically with channel
    # SNR (the cache reproduces the resolver's noise power at every
    # point) and gain several dB across the 21 dB sweep before the
    # multi-user interference floor flattens it.
    print(
        f"peak SNR: {mean[0]:.2f} dB @ {SNRS[0]:.0f} dB channel SNR -> "
        f"{mean[-1]:.2f} dB @ {SNRS[-1]:.0f} dB"
    )
    assert np.all(np.diff(mean) > 0), "peak SNR must rise with channel SNR"
    assert mean[-1] - mean[0] > 3.0, "sweep gained implausibly little SNR"

    speedup = t_recompose / t_plan if t_plan else float("nan")
    print(
        f"{SNRS.size}x{DRAWS} campaign, {USERS} users: "
        f"Plan {t_plan * 1e3:.1f} ms vs re-compose {t_recompose * 1e3:.1f} ms"
        f"  ->  {speedup:.0f}x faster stimulus generation"
    )

    fig, (ax0, ax1) = plt.subplots(1, 2, figsize=(11, 4.2))
    ax0.errorbar(SNRS, mean, yerr=std, marker="o", capsize=3)
    ax0.set(
        title="Matched-filter peak SNR vs channel SNR",
        xlabel="channel SNR (dB)",
        ylabel="peak SNR (dB)",
    )
    ax0.grid(True, alpha=0.3)

    for i, snr in enumerate(SNRS):
        ax1.scatter(
            np.full(DRAWS, snr), cloud[i], s=12, alpha=0.5, color="tab:blue"
        )
    ax1.set(
        title=f"Monte-Carlo cloud ({DRAWS} draws/point)",
        xlabel="channel SNR (dB)",
        ylabel="peak SNR (dB)",
    )
    ax1.grid(True, alpha=0.3)
    fig.suptitle(
        f"One Plan, {SNRS.size}x{DRAWS} points — {speedup:.0f}x vs re-compose"
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print(f"wrote {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "plan_demo.png")
