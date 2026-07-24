"""plan_background_demo.py — one cache slot for a whole background field.

:func:`~doppler.wfm.prepare` caches every source of a scene separately, which
is what makes a sweep cheap: each render is a re-weighted sum of buffers that
were synthesised once. The cost of that separability is one full-length buffer
per source, and it is the wrong trade for a scene dominated by emitters that
never move — a crowded uplink, a co-channel user population, an interference
field. Those sources exist to be *present*, not to be swept.

Marking them ``background=True`` tells :func:`prepare` to fold a contiguous
leading run of them into ONE pre-summed cache entry, each member pre-weighted
by its own ``10**(level/20)``. The composite is not a special case: it is just
another cache slot, so it takes a single entry in ``gains`` / ``phases`` /
``enable`` and counts as one in ``n_sources`` — which means the entire
background field scales, rotates or disappears with one number, while the few
sources you actually sweep keep their own slots.

The demo builds a crowded-uplink scene — a population of static users, plus a
wanted signal carrying the SNR and one variable interferer — and measures both
halves of the trade. Two views (saved to a PNG):

  * **Cache footprint** — bytes of signal cache vs population size, folded
    against per-source. Per-source grows linearly with the population; folded
    is flat, because the population is always one buffer.
  * **The field as one control** — Welch PSD of the baseline scene, of the
    background trimmed 10 dB, and of the background switched off. The whole
    field moves together while the wanted signal and the interferer stay put.

Run:  python -m doppler.examples.plan_background_demo  [out.png]
"""

from __future__ import annotations

import sys
import time

import numpy as np

from doppler.spectral import PSD
from doppler.wfm import Composer, Segment, prepare, qpsk

FS = 1e6  # sample rate (Hz)
NS = 8192  # on-time samples
POPULATION = (25, 50, 100, 200)  # background users, footprint sweep
SHOWN = 100  # population used for the spectrum panel
WANTED_SNR = 40.0  # dB, carried by the wanted user (Es/N0)
NFFT = 1024

# The real workload this feature exists for (quoted on the figure): a
# full-rate capture, where one buffer per source is gigabytes.
REAL_FS = 122.88e6
REAL_SECONDS = 10e-3
REAL_USERS = 400


# --8<-- [start:scene]
def uplink(n_users: int, *, background: bool = True) -> Composer:
    """A crowded uplink: `n_users` static users, a wanted signal, a jammer.

    The static population is written FIRST and flagged ``background=True``.
    That ordering is required, not stylistic: the composite sums from zero, so
    it reproduces a full compose bit-for-bit only when nothing precedes it.
    """
    field = [
        qpsk(
            seed=1000 + k,
            sps=8,
            pn_length=9,
            freq=-4.0e5 + 8.0e5 * k / max(n_users - 1, 1),
            level=-30.0,
            pulse="rrc",
            background=background,  # <-- the whole feature
        )
        for k in range(n_users)
    ]
    wanted = qpsk(
        seed=7, sps=8, pn_length=9, freq=0.0, snr=WANTED_SNR, pulse="rrc"
    )
    jammer = qpsk(
        seed=11, sps=8, pn_length=9, freq=3.3e5, level=-12.0, pulse="rrc"
    )
    return Composer(Segment.sum(*field, wanted, jammer, fs=FS, num_samples=NS))


# --8<-- [end:scene]


# --8<-- [start:savings]
def cache_bytes(plan) -> int:
    """Signal-cache footprint: one cf32 buffer per slot, ON-time long."""
    return plan.n_sources * NS * 8


def footprint(n_users: int) -> tuple[int, int]:
    """Cache bytes for `n_users` background sources, folded vs per-source."""
    folded = prepare(uplink(n_users, background=True))
    per_source = prepare(uplink(n_users, background=False))

    # Folding is invisible to the output: the composite is summed in spec
    # order, from zero, over a prefix — exactly the partial sum a full
    # compose holds at that point — so the contract still holds to the bit.
    assert np.array_equal(folded.render(), per_source.render()), (
        "folding changed the rendered samples"
    )
    # ...and the population really did collapse to a single slot.
    assert folded.n_sources == 3, "expected background + wanted + jammer"
    assert per_source.n_sources == n_users + 2

    return cache_bytes(folded), cache_bytes(per_source)


# --8<-- [end:savings]


def _psd(x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Welch PSD in dB over 50 %-overlapped Hann segments.

    ``PSD.psd_db()`` returns an already-CENTERED spectrum (bin 0 is -fs/2,
    bin nfft/2 is DC), so it must not be fftshifted again — only the matching
    frequency axis is built with fftshift.
    """
    est = PSD(n=NFFT, fs=FS, window="hann")
    for i in range(0, x.size - NFFT + 1, NFFT // 2):
        est.accumulate(np.ascontiguousarray(x[i : i + NFFT]))
    db = np.asarray(est.psd_db())
    return np.fft.fftshift(np.fft.fftfreq(db.size, 1.0 / FS)), db


def _band_power(
    freq: np.ndarray, db: np.ndarray, lo: float, hi: float
) -> float:
    """Mean LINEAR power over a frequency window.

    Linear, not dB, because the checks below subtract one band power from
    another to isolate the background field's own contribution from the noise
    floor and the other sources sharing the window. Those are mutually
    independent, so their powers add.
    """
    sel = (freq >= lo) & (freq <= hi)
    return float(np.mean(10.0 ** (db[sel] / 10.0)))


def _db(x: float) -> float:
    return 10.0 * np.log10(max(x, 1e-30))


# --8<-- [start:override]
def overrides(plan) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Three renders of one Plan that differ by a single number.

    Slot 0 is the whole background field: one entry in gains/phases/enable,
    whatever the population. Slots 1 and 2 are the wanted signal and the
    interferer, which keep their own controls.
    """
    base = plan.render()  # field at its own levels
    trimmed = plan.render(gains=[-10.0, 0.0, -12.0])  # field down 10 dB
    off = plan.render(enable=[False, True, True])  # field removed
    return base, trimmed, off


# --8<-- [end:override]


def main(out: str = "plan_background_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # ── footprint: folded is flat, per-source grows with the population ──
    folded_b, per_source_b = zip(*(footprint(n) for n in POPULATION))
    folded_b = np.array(folded_b, dtype=float)
    per_source_b = np.array(per_source_b, dtype=float)
    ratio = per_source_b[-1] / folded_b[-1]
    print(
        f"{POPULATION[-1]} background users: "
        f"{per_source_b[-1] / 1e6:.1f} MB per-source vs "
        f"{folded_b[-1] / 1e6:.2f} MB folded  ->  {ratio:.0f}x smaller"
    )
    assert np.all(folded_b == folded_b[0]), "folded cache must not grow"
    assert ratio > 50.0, "expected a large saving at the biggest population"

    # ── the scene the spectrum panel uses, and its bit-exactness ────────
    scene = uplink(SHOWN)
    plan = prepare(scene)
    assert np.array_equal(plan.render(), scene.compose()), (
        "Plan.render() is not bit-identical to Composer.compose()"
    )

    # ── the field as ONE control: three renders, one changed number ─────
    base, trimmed, off = overrides(plan)

    # ── render cost: a sweep touches 3 buffers, not SHOWN + 2 ───────────
    flat = prepare(uplink(SHOWN, background=False))
    flat_gains = [0.0] * flat.n_sources
    t0 = time.perf_counter()
    for _ in range(20):
        plan.render(gains=[-10.0, 0.0, -12.0])
    t_folded = time.perf_counter() - t0
    t0 = time.perf_counter()
    for _ in range(20):
        flat.render(gains=flat_gains)
    t_flat = time.perf_counter() - t0
    speedup = t_flat / t_folded if t_folded else float("nan")
    print(
        f"render x20 @ {SHOWN} users: folded {t_folded * 1e3:.1f} ms vs "
        f"per-source {t_flat * 1e3:.1f} ms  ->  {speedup:.0f}x faster"
    )

    freq, db_base = _psd(base)
    _, db_trim = _psd(trimmed)
    _, db_off = _psd(off)

    # ── self-validation: the override moved the FIELD, not the signals ──
    # A slice of band the background users dominate (the wanted signal sits at
    # DC, the jammer at +330 kHz). The field-off render is what everything
    # else in that window contributes, so subtracting it leaves the field's
    # own power — the quantity the gains override is supposed to scale.
    band = (-3.6e5, -2.0e5)
    p_base = _band_power(freq, db_base, *band)
    p_trim = _band_power(freq, db_trim, *band)
    p_off = _band_power(freq, db_off, *band)
    field_base, field_trim = p_base - p_off, p_trim - p_off
    trim_db = _db(field_base) - _db(field_trim)
    print(
        f"background band: field {_db(field_base):.1f} dB, "
        f"{_db(field_base) - _db(p_off):.1f} dB over everything else; "
        f"a -10 dB trim scaled it by {trim_db:.2f} dB"
    )
    assert field_base > 0 and field_trim > 0, "the field must be measurable"
    assert _db(field_base) - _db(p_off) > 10.0, "field too weak to resolve"
    assert 9.5 < trim_db < 10.5, "gains[0] must scale the field by 10 dB"

    # The wanted signal shares the window at DC and must NOT have moved.
    wdb_base = _db(_band_power(freq, db_base, -2.0e4, 2.0e4))
    wdb_trim = _db(_band_power(freq, db_trim, -2.0e4, 2.0e4))
    print(f"wanted band: {wdb_base:.2f} dB -> {wdb_trim:.2f} dB")
    assert abs(wdb_base - wdb_trim) < 0.5, "the wanted signal must not move"

    fig, (ax0, ax1) = plt.subplots(1, 2, figsize=(11, 4.2))

    ax0.plot(POPULATION, per_source_b / 1e6, marker="o", label="per source")
    ax0.plot(POPULATION, folded_b / 1e6, marker="s", label="background=True")
    ax0.set(
        title="Signal-cache footprint vs population",
        xlabel="static background users",
        ylabel="cache (MB)",
        yscale="log",
    )
    ax0.legend(loc="center right")
    ax0.grid(True, alpha=0.3, which="both")

    # The workload this exists for: a full-rate capture is ~150x longer than
    # the demo's ON-time, which is what turns "a few MB" into "a few GB".
    real_n = REAL_FS * REAL_SECONDS
    real_per_source = (REAL_USERS + 2) * real_n * 8
    real_folded = 3 * real_n * 8
    ax0.text(
        0.03,
        0.13,
        f"at {REAL_FS / 1e6:.2f} MHz x {REAL_SECONDS * 1e3:.0f} ms,\n"
        f"{REAL_USERS} users: "
        f"{real_per_source / 1e9:.1f} GB -> {real_folded / 1e6:.0f} MB",
        transform=ax0.transAxes,
        fontsize=8,
        va="bottom",
        bbox={"boxstyle": "round", "fc": "0.93", "ec": "0.7"},
    )

    ax1.plot(freq / 1e3, db_base, lw=0.9, label="baseline")
    ax1.plot(freq / 1e3, db_trim, lw=0.9, label="field −10 dB")
    ax1.plot(freq / 1e3, db_off, lw=0.9, label="field off")
    ax1.set(
        title=f"One gain moves the whole field ({SHOWN} users)",
        xlabel="frequency (kHz)",
        ylabel="PSD (dB)",
    )
    ax1.legend(loc="lower left", fontsize=8, framealpha=0.9)
    ax1.grid(True, alpha=0.3)

    fig.suptitle(
        f"background=True — {POPULATION[-1]} users in 1 cache slot "
        f"({ratio:.0f}x smaller, {speedup:.0f}x faster to re-render)"
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print(f"wrote {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "plan_background_demo.png")
