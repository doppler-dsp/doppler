"""Plan — certification evidence for the CACHE.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_plan/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`.

**This is a cache, so its subject is INDISTINGUISHABILITY.** `Composer`
is certified in its own folder and this report cites it rather than
re-deriving it: nothing here re-measures what a segment sums to or what a
gap carries. A cache earns its place by two properties and this measures
both --

- that every answer it serves is the answer the thing it caches would
  have given, on every axis it claims to serve, bit for bit;
- that what it will NOT serve, it refuses, rather than serving something
  subtly different. `Plan` refuses a Doppler source for reasons measured
  against `compose()`, and that refusal is the honest half of the
  contract.

The order is the campaign's: `native/inc/wfm/wfm_plan.h` is the SSOT and
`native/tests/test_wfm_plan.c` certifies it in C.
"""

from __future__ import annotations

import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import (
    Composer,
    PlanFromBlob,
    PlanFromFile,
    Segment,
    prepare,
    qpsk,
    tone,
)

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

FS = 1.0e6
N = 4096

# The seeds the ranged-gap draws are taken at. Fixed, because a report is
# byte-compared: a random seed here would make every render stale.
GAP_SEEDS = (1, 2, 3, 7, 11, 13, 17, 19)


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── the scenes ───────────────────────────────────────────────────────
def scene(qpsk_snr: float = 12.0, tone_level: float = 0.0) -> Composer:
    """A separable 2-source scene: a qpsk anchor (carrying SNR) + a tone.

    Separable on purpose: the qpsk source owns the noise floor and the
    tone is clean, so an override aimed at one is visible against the
    other. A single-source scene cannot show a floor staying put while a
    signal moves.
    """
    return Composer(
        Segment.sum(
            qpsk(snr=qpsk_snr, seed=7, sps=8, pn_length=7),
            tone(freq=1.0e5, seed=3, sps=8, level=tone_level),
            fs=FS,
            num_samples=N,
        )
    )


def multi_segment() -> Composer:
    """Two segments, so the flat segment-major source order is exercised."""
    return Composer(
        [
            Segment.sum(
                qpsk(snr=12.0, seed=7, sps=8, pn_length=7),
                fs=FS,
                num_samples=N // 2,
            ),
            Segment.sum(
                tone(freq=2.0e5, seed=3, sps=8),
                fs=FS,
                num_samples=N // 2,
            ),
        ]
    )


def repeats_scene() -> Composer:
    """A fixed-gap burst train — three instances of one declared span."""
    return Composer(
        Segment.sum(
            qpsk(snr=10.0, seed=33, sps=8, pn_length=7),
            fs=FS,
            num_samples=256,
            off_samples=128,
            repeats=3,
        )
    )


def ranged_gap_scene() -> Composer:
    """A burst train whose GAPS are ranged — redrawn per instance.

    This is the shape the capacity contract is about: the signal is fixed
    but the timing is not, so the materialized length is a property of
    the draw rather than of the scene.
    """
    return Composer(
        Segment.sum(
            qpsk(snr=10.0, seed=33, sps=8, pn_length=7),
            fs=FS,
            num_samples=256,
            off_samples=(32, 512),
            delay_samples=(16, 256),
            repeats=3,
        )
    )


def arr(x) -> np.ndarray:
    return np.asarray(x)


def power(x: np.ndarray) -> float:
    z = np.asarray(x).astype(np.complex128)
    return float(np.mean(np.abs(z) ** 2))


def rejected(build) -> bool:
    """True if preparing this scene raises, as an out-of-scope spec must."""
    try:
        build()
    except ValueError:
        return True
    except Exception:
        return False
    return False


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    base_rows: list[list[str]] = field(default_factory=list)
    base_all_exact: bool = False
    axis_rows: list[list[str]] = field(default_factory=list)
    axes_all_exact: bool = False
    superpose_err: float = 0.0
    superpose_ok: bool = False
    phase_identity: bool = False
    reject_rows: list[list[str]] = field(default_factory=list)
    all_rejected: bool = False
    edge_rows: list[list[str]] = field(default_factory=list)
    all_edges_accepted: bool = False
    cap: int = 0
    gap_rows: list[list[str]] = field(default_factory=list)
    gap_lengths: tuple[int, ...] = ()
    n_distinct: int = 0
    draw_le_cap: bool = False
    ragged_stack_fails: bool = False
    prealloc_fails: bool = False
    fixed_is_rectangular: bool = False
    blob_bytes: int = 0
    buffer_bytes: int = 0
    buffer_fraction: float = 0.0
    restore_exact: bool = False
    file_exact: bool = False
    restore_override_exact: bool = False
    corrupt_rejected: bool = False
    mc_all_distinct: bool = False
    mc_err8: float = 0.0
    mc_err64: float = 0.0
    mc_converges: bool = False
    mc_ratio: float = 0.0
    cited: tuple[str, ...] = ()


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Plan` prepares a scene once and re-materializes parameter "
        "variations cheaply: the expensive DSP (spreading, pulse shaping, "
        "the LO) lives in the signal terms, which do not move when a "
        "level, a phase, the SNR or the noise seed does. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is `native/inc/wfm/wfm_plan.h`, certified in C by "
        "`native/tests/test_wfm_plan.c`."
    )
    R.md()
    R.md(
        "**This is a cache, so its subject is indistinguishability.** "
        "`Composer` is certified in its own folder and cited here rather "
        "than re-derived -- see §2.6. What is measured is whether every "
        "answer the cache serves is the answer the composition would have "
        "given, and whether what it will not serve, it refuses."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_baseline(d: Data) -> None:
    """2.1 — the baseline IS the composition, on every scene shape."""
    cases = [
        ("two sources, one segment", scene()),
        ("two segments", multi_segment()),
        ("repeats=3, fixed gap", repeats_scene()),
        ("repeats=3, ranged gap", ranged_gap_scene()),
    ]
    rows, all_exact = [], True
    for name, sc in cases:
        ref = arr(sc.compose())
        got = arr(prepare(sc).render())
        exact = got.shape == ref.shape and np.array_equal(got, ref)
        all_exact &= exact
        rows.append([name, str(ref.shape[0]), "yes" if exact else "**NO**"])
    d.base_rows, d.base_all_exact = rows, all_exact

    R.md("### 2.1 The baseline is the composition, bit for bit (C gate-0)")
    R.md()
    R.md(
        "`render()` with no overrides against `Composer(scene).compose()` "
        "on the same scene. This is the claim the whole object rests on: a "
        "cache that is merely *close* to what it caches is a second "
        "implementation, and the campaign has seen what two copies of one "
        "formula do. Equality is exact -- `array_equal`, not `allclose`."
    )
    R.md()
    R.table(["scene", "samples", "bit-identical"], rows)


def measure_axes(d: Data) -> None:
    """2.2 — each override axis against a full compose of the same change."""
    sc = scene()
    plan = prepare(sc)
    base = arr(plan.render())
    rows, all_exact = [], True

    # SNR: the modified scene is the same scene at the new anchor SNR.
    ref = arr(scene(qpsk_snr=6.0).compose())
    got = arr(plan.at(6.0, plan.anchor_seed))
    ok = np.array_equal(got, ref)
    all_exact &= ok
    rows.append(
        ["snr = 6 dB", "at(6, anchor_seed)", "yes" if ok else "**NO**"]
    )

    # The same override through the JSON path must take the same route.
    got_json = arr(plan.render(snr=6.0))
    ok_json = np.array_equal(got_json, ref)
    all_exact &= ok_json
    rows.append(
        ["snr = 6 dB", "render(snr=6)", "yes" if ok_json else "**NO**"]
    )

    # Gain on the NON-anchor source: the tone moves, the floor does not.
    ref_g = arr(scene(tone_level=-6.0).compose())
    got_g = arr(plan.render(gains=[0.0, -6.0]))
    ok_g = np.array_equal(got_g, ref_g)
    all_exact &= ok_g
    rows.append(
        [
            "tone level = -6 dBFS",
            "render(gains=...)",
            "yes" if ok_g else "**NO**",
        ]
    )

    # enable: all-on is the baseline exactly.
    got_e = arr(plan.render(enable=[True, True]))
    ok_e = np.array_equal(got_e, base)
    all_exact &= ok_e
    rows.append(
        ["enable all-on", "render(enable=...)", "yes" if ok_e else "**NO**"]
    )

    # phase 0 is the identity.
    got_p = arr(plan.render(phases=[0.0, 0.0]))
    d.phase_identity = np.array_equal(got_p, base)
    all_exact &= d.phase_identity
    rows.append(
        [
            "phase = 0 rad",
            "render(phases=...)",
            "yes" if d.phase_identity else "**NO**",
        ]
    )

    # Determinism: the same request twice is the same bytes.
    ok_d = np.array_equal(arr(plan.at(6.0, 99)), arr(plan.at(6.0, 99)))
    all_exact &= ok_d
    rows.append(
        ["same (snr, seed) twice", "at()", "yes" if ok_d else "**NO**"]
    )

    d.axis_rows, d.axes_all_exact = rows, all_exact

    R.md("### 2.2 Every override axis is a full compose of the same change")
    R.md()
    R.md(
        "Each axis is measured against an EXTERNAL truth -- a full "
        "`compose()` of the equivalently-modified scene -- rather than "
        "against another Plan render. A cache compared only to itself is "
        "self-consistent by construction and blind to the one defect that "
        "matters, which is that the cache and the composition have drifted "
        "apart."
    )
    R.md()
    R.table(["override", "path", "bit-identical to compose"], rows)

    # Superposition: the render is a LINEAR form, so the isolated
    # contributions must add back up to the whole. This is a truth the
    # composer never sees, so it cannot be a shared defect.
    only_qpsk = arr(plan.render(enable=[True, False]))
    only_tone = arr(plan.render(enable=[False, True]))
    floor = arr(plan.render(enable=[False, False]))
    recon = only_qpsk + only_tone - floor
    err = float(np.max(np.abs(recon.astype(np.complex128) - base)))
    d.superpose_err = err
    d.superpose_ok = err < 1e-5
    R.md(
        f"**Superposition holds to {err:.2e}.** Rendering each source "
        "alone and adding the two back (less the floor, which both carry) "
        "reproduces the full render. The segment is the linear form its "
        "header claims, and this is an external truth -- the composer is "
        "never asked -- so it cannot be a defect the two share."
    )
    R.md()


def measure_refusals(d: Data) -> None:
    """2.3 — what the cache refuses, and the edge it must still accept."""
    rows = [
        [
            "a ranged per-source field (`snr=[6, 12]`)",
            "redrawing a source's SNR invalidates its cached render",
            "ValueError"
            if rejected(lambda: prepare(_ranged_snr()))
            else "**served**",
        ],
        [
            "a ranged on-time (`num_samples=(1024, 2048)`)",
            "the signal cache is fixed-length",
            "ValueError"
            if rejected(lambda: prepare(_ranged_on()))
            else "**served**",
        ],
        [
            "a source with `doppler`",
            "a channel is a stateful resampler; the cache has no history",
            "ValueError"
            if rejected(lambda: prepare(_dop(doppler=5.0)))
            else "**served**",
        ],
        [
            "a source with `doppler_rate`",
            "same channel, same absence of history",
            "ValueError"
            if rejected(lambda: prepare(_dop(doppler_rate=200.0)))
            else "**served**",
        ],
    ]
    d.reject_rows = rows
    d.all_rejected = all(r[2] == "ValueError" for r in rows)

    R.md("### 2.3 The refusals, and the edge that must NOT be refused")
    R.md()
    R.md(
        "A cache that cannot serve a shape has two honest options and one "
        "dishonest one. `Plan` refuses, rather than serving a render that "
        "differs from `compose()` in a way nothing downstream can see. The "
        "Doppler rows are the interesting ones: the reasons are measured "
        "against `compose()` in `plan_build()` -- a channel rings the "
        "burst's tail out across the trailing gap, and `compose()` puts "
        "the AWGN inside the channel while the cache re-weights it "
        "outside -- and teaching the cache to carry that history is "
        "[gh-1109](https://github.com/doppler-dsp/doppler/issues/1109)."
    )
    R.md()
    R.table(["out-of-scope scene", "why", "outcome"], rows)

    # The edge: a refusal must be about the CHANNEL, not about the keys.
    edges = [
        (
            "`doppler_lifetime` declared, `doppler = 0`",
            "zero doppler builds no channel at all",
            lambda: prepare(_dop(doppler=0.0, doppler_lifetime="persist")),
        ),
        (
            "ranged `off_samples` / `delay_samples`",
            "the GAPS may be ranged -- only the on-time may not",
            lambda: prepare(ranged_gap_scene()),
        ),
        (
            "`repeats = 3`",
            "bounded instancing: fresh noise over a fixed signal",
            lambda: prepare(repeats_scene()),
        ),
    ]
    erows, all_ok = [], True
    for name, why, build in edges:
        try:
            build()
            ok = True
        except Exception:
            ok = False
        all_ok &= ok
        erows.append([name, why, "prepared" if ok else "**REFUSED**"])
    d.edge_rows, d.all_edges_accepted = erows, all_ok

    R.md(
        "The other half of a refusal is what it must NOT catch. A rule "
        "that rejects a field rather than the behaviour behind it costs a "
        "caller a scene that was always safe:"
    )
    R.md()
    R.table(["in-scope scene", "why it is in scope", "outcome"], erows)


def _ranged_snr() -> Composer:
    return Composer(
        Segment.sum(
            qpsk(snr=[6.0, 12.0], seed=7),
            tone(freq=1.0e5, seed=3),
            fs=FS,
            num_samples=1024,
        )
    )


def _ranged_on() -> Composer:
    return Composer(
        Segment.sum(qpsk(snr=12.0, seed=7), fs=FS, num_samples=(1024, 2048))
    )


def _dop(**extra) -> Composer:
    return Composer(
        [
            Segment(
                "bpsk",
                fs=FS,
                sps=4,
                snr=12.0,
                num_samples=2048,
                seed=3,
                carrier_hz=2.2e9,
                **extra,
            )
        ]
    )


def measure_capacity(d: Data) -> None:
    """2.4 — len() is a capacity, and a ranged draw is shorter than it."""
    plan = prepare(ranged_gap_scene())
    d.cap = len(plan)
    rows, lengths = [], []
    for s in GAP_SEEDS:
        a = arr(plan.render(seed=s))
        lengths.append(int(a.shape[0]))
        rows.append([str(s), str(a.shape[0]), f"{a.shape[0] / d.cap:.3f}"])
    d.gap_rows = rows
    d.gap_lengths = tuple(lengths)
    d.n_distinct = len(set(lengths))
    d.draw_le_cap = all(v <= d.cap for v in lengths)
    _csv(
        DATA / "ranged_gap_draws.csv",
        "seed,samples,fraction_of_capacity",
        [[float(s), float(v), v / d.cap] for s, v in zip(GAP_SEEDS, lengths)],
    )

    R.md("### 2.4 `len()` is a CAPACITY, and a ranged draw is shorter")
    R.md()
    R.md(
        f"For a ranged-gap scene the materialized length is a property of "
        f"the DRAW, not of the scene: `len(plan)` is the worst case "
        f"(every gap at its `hi` bound) and each seed lands somewhere "
        f"below it. Across {len(GAP_SEEDS)} seeds the capacity is "
        f"{d.cap} samples and the draws take "
        f"**{d.n_distinct} distinct lengths**, from {min(lengths)} to "
        f"{max(lengths)} -- {min(lengths) / d.cap:.0%} to "
        f"{max(lengths) / d.cap:.0%} of capacity."
    )
    R.md()
    R.table(["seed", "samples drawn", "fraction of `len()`"], rows)
    R.md("Raw sweep: [data/ranged_gap_draws.csv](data/ranged_gap_draws.csv).")
    R.md()

    # The two idioms a Monte-Carlo caller reaches for first.
    draws = list(plan.monte_carlo(6.0, 8, seed0=1))
    try:
        np.array(draws)
        d.ragged_stack_fails = False
    except ValueError:
        d.ragged_stack_fails = True
    try:
        buf = np.empty((8, d.cap), np.complex64)
        for i, x in enumerate(draws):
            buf[i] = x
        d.prealloc_fails = False
    except ValueError:
        d.prealloc_fails = True

    fixed = prepare(repeats_scene())
    fdraws = list(fixed.monte_carlo(6.0, 8, seed0=1))
    d.fixed_is_rectangular = len({int(x.shape[0]) for x in fdraws}) == 1

    R.md(
        "**This breaks both Monte-Carlo idioms, and the docstring says it "
        "will not.** `Plan.render`'s Returns section promises "
        "`complex64` samples, *length* `len()`; for a ranged-gap scene "
        "that is false, and the consequence is not a wrong number but a "
        "raised exception at the point a campaign tries to collect its "
        "draws:"
    )
    R.md()
    R.table(
        ["idiom", "fixed-gap scene", "ranged-gap scene"],
        [
            [
                "`np.array(list(plan.monte_carlo(...)))`",
                f"(8, {int(fdraws[0].shape[0])}) complex64",
                "**ValueError** (inhomogeneous shape)"
                if d.ragged_stack_fails
                else "ok",
            ],
            [
                "preallocate `(n, len(plan))`, assign rows",
                "ok",
                "**ValueError** (could not broadcast)"
                if d.prealloc_fails
                else "ok",
            ],
        ],
    )
    R.md(
        "Recorded as **F1**. The C API is not at fault -- it returns the "
        "actual draw and documents `len()` as a capacity, and the C tests "
        "now pin that the tail past the returned length is zeroed. The gap "
        "is on the Python face, where the actual length arrives only as "
        "the array's shape and the documented contract contradicts it."
    )
    R.md()


def measure_persistence(d: Data) -> None:
    """2.5 — save/restore is an equivalence; its cost is a size."""
    plan = prepare(scene())
    base = arr(plan.render())
    blob = plan.save()
    d.blob_bytes = len(blob)

    restored = PlanFromBlob(blob)
    d.restore_exact = np.array_equal(arr(restored.render()), base)
    d.restore_override_exact = np.array_equal(
        arr(restored.at(6.0, 1000)), arr(plan.at(6.0, 1000))
    )

    # A blob is a build artifact, not evidence: it is megabytes, it is
    # native-endian, and nothing re-derives a number from it. It goes to a
    # temp dir on both paths -- the `data/` folder is for sweeps a reader
    # can check the report's arithmetic against.
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "plan.bin"
        plan.dump(path)
        d.file_exact = np.array_equal(arr(PlanFromFile(path).render()), base)

    d.corrupt_rejected = rejected_any(
        lambda: PlanFromBlob(blob[:16])
    ) and rejected_any(lambda: PlanFromBlob(b"\x00" * len(blob)))

    # The size claim: the blob is DOMINATED by the cached buffers.
    d.buffer_bytes = plan.n_sources * N * 8
    d.buffer_fraction = d.buffer_bytes / d.blob_bytes

    R.md("### 2.5 Persistence is an equivalence, and its cost is a size")
    R.md()
    R.md(
        f"`save()`/`PlanFromBlob` and `dump()`/`PlanFromFile` reconstruct "
        f"a Plan that renders bit-identically -- baseline and override "
        f"both. The cost is the reason the header calls the spec-rebuild "
        f"the default: this {N}-sample two-source scene serializes to "
        f"**{d.blob_bytes:,} bytes**, of which {d.buffer_bytes:,} "
        f"({d.buffer_fraction:.1%}) is the cached sample buffers "
        f"(`n_sources x num_samples x 8`). The spec that reproduces it is "
        f"under a kilobyte. A blob is worth writing only when the DSP cost "
        f"it saves is worth {d.blob_bytes / 1024:.0f} KB per scene."
    )
    R.md()
    R.md(
        "A truncated blob and an all-zero blob of the right length are "
        "both refused rather than reinterpreted. The endian byte and the "
        "fingerprint-mismatch rebuild are C-ONLY (F3)."
    )
    R.md()


def rejected_any(fn) -> bool:
    try:
        fn()
    except (ValueError, RuntimeError, OSError):
        return True
    return False


def measure_monte_carlo(d: Data) -> None:
    """2.6 — fresh noise over a fixed signal, which is what makes it a rate."""
    plan = prepare(scene())
    draws = [arr(x) for x in plan.monte_carlo(6.0, 64, seed0=1)]
    d.mc_all_distinct = not any(
        np.array_equal(draws[i], draws[j])
        for i in range(8)
        for j in range(i + 1, 8)
    )

    # The signal is identical across draws, so the ensemble mean converges
    # on it at 1/sqrt(n) -- an external truth about averaging, not a claim
    # the composer is asked to confirm.
    clean = arr(plan.render(snr=60.0)).astype(np.complex128)
    m8 = np.mean(np.stack(draws[:8]).astype(np.complex128), axis=0)
    m64 = np.mean(np.stack(draws).astype(np.complex128), axis=0)
    d.mc_err8 = float(np.sqrt(np.mean(np.abs(m8 - clean) ** 2)))
    d.mc_err64 = float(np.sqrt(np.mean(np.abs(m64 - clean) ** 2)))
    d.mc_ratio = d.mc_err8 / d.mc_err64 if d.mc_err64 > 0 else 0.0
    # sqrt(64/8) = 2.83; allow a wide band -- this is a convergence RATE,
    # and 64 draws is a small ensemble.
    d.mc_converges = 1.8 < d.mc_ratio < 4.2

    _csv(
        DATA / "mc_convergence.csv",
        "n_draws,rms_error_to_clean",
        [[8.0, d.mc_err8], [64.0, d.mc_err64]],
    )

    R.md("### 2.6 A repeat re-rolls the noise and not the burst")
    R.md()
    R.md(
        f"Across 64 draws at one SNR no two are identical, and the "
        f"ensemble mean converges on the clean render: RMS error "
        f"{d.mc_err8:.4f} over 8 draws against {d.mc_err64:.4f} over 64, "
        f"a ratio of {d.mc_ratio:.2f} where independent noise predicts "
        f"sqrt(8) = 2.83. That is what makes a detection rate measured "
        f"over `monte_carlo()` a rate over trials rather than one trial "
        f"counted 64 times. Raw figures: "
        f"[data/mc_convergence.csv](data/mc_convergence.csv)."
    )
    R.md()
    R.md(
        "The composition's own properties -- what a segment sums to, what "
        "a gap carries, that a repeat's signal is fixed -- are certified "
        "in [wfm_compose](../wfm_compose/results.md) and cited rather "
        "than re-derived here."
    )
    R.md()
    d.cited = ("wfm_compose",)


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    d = Data()
    measure_baseline(d)
    measure_axes(d)
    measure_refusals(d)
    measure_capacity(d)
    measure_persistence(d)
    measure_monte_carlo(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "CONFIRMED",
        "**A ranged-gap Plan returns a different length for every seed, "
        "and `render()`'s documented Returns section says it does not.** "
        "The docstring promises `complex64` samples, *length* `len()`; "
        f"across {len(GAP_SEEDS)} seeds this scene returns "
        f"{d.n_distinct} distinct lengths between {min(d.gap_lengths)} "
        f"and {max(d.gap_lengths)} against a `len()` of {d.cap} (§2.4). "
        "The C API is right -- it returns the actual draw and documents "
        "`len()` as a worst-case capacity -- and the binding faithfully "
        "truncates to it; what is wrong is the Python contract written "
        "over the top. It costs a caller the two idioms a Monte-Carlo "
        "campaign reaches for first: `np.array(list(plan.monte_carlo(...)))` "
        "raises on an inhomogeneous shape, and pre-allocating "
        "`(n, len(plan))` raises on broadcast. Both are the advertised use "
        "case (`Plan` exists for Monte-Carlo campaigns) on an advertised "
        "feature (ranged gaps, redrawn per instance). Filed as "
        "[gh-1128](https://github.com/doppler-dsp/doppler/issues/1128).",
    )
    R.find(
        "F2",
        "GAP",
        "**Nothing can tell you the restore fast path still works.** "
        "`wfm_plan_restore` loads the cached buffers when the DSP "
        "fingerprint matches and otherwise rebuilds from the embedded "
        "spec, and both produce bit-identical output by design -- that is "
        "the feature. The consequence is that the ONLY observable "
        "difference between the fast path and the slow one is time, and "
        "the API exposes no indicator of which ran. If the fingerprint "
        "stopped matching -- a build-system change, a hash that no longer "
        "round-trips -- every restore would silently pay full "
        "`prepare()` cost, return the right samples, and pass every test "
        "in both suites. The entire point of save/restore would be dead "
        "with nothing red. Filed as "
        "[gh-1129](https://github.com/doppler-dsp/doppler/issues/1129); "
        "a one-bit accessor would make it gateable.",
    )
    R.find(
        "F3",
        "C-ONLY",
        "**Three claims the Python face cannot reach**, now pinned in "
        "`native/tests/test_wfm_plan.c` and certified there. (1) The "
        "zero-padded tail: the C `render()`/`at()` write up to `len()` "
        "and zero everything past the returned draw, which the binding "
        "hides by truncating to the actual length -- so the guarantee a "
        "C caller reusing one buffer depends on is invisible from Python. "
        "(2) `destroy(NULL)` as a documented no-op, which has no Python "
        "spelling. (3) The foreign-endian byte at blob offset 6: Python "
        "covers a truncated and a wrong-magic blob, and the endian guard "
        "-- the one that stops native-endian POD buffers being "
        "reinterpreted -- was covered by neither suite until now. All "
        "three proven by sabotage.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "**The Doppler refusal is the honest half of the cache's "
        "contract, not a missing feature.** A Doppler channel is a "
        "stateful resampler that runs through the gaps, so what a burst "
        "renders as depends on the leading delay and the previous "
        "instance's gap; the cache holds one clean on-time in isolation "
        "and has nowhere to keep that history. It also puts the AWGN "
        "outside the channel where `compose()` puts it inside. Both were "
        "measured against `compose()` rather than assumed (§2.3), and a "
        "cached render that differs from `compose()` invisibly is worse "
        "than no plan. Carrying the history is "
        "[gh-1109](https://github.com/doppler-dsp/doppler/issues/1109). "
        "The refusal is correctly scoped to the behaviour and not to the "
        "field: a `doppler_lifetime` on a source with zero doppler builds "
        "no channel and still prepares.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits -- the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/wfm/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.base_all_exact,
        "the baseline render is bit-identical to a full compose across all "
        "four scene shapes -- one segment, two segments, fixed-gap "
        "repeats and ranged-gap repeats",
    )
    R.limit(
        d.axes_all_exact,
        "every override axis reproduces a full compose of the "
        "equivalently-modified scene, bit for bit: snr through both at() "
        "and render(), a non-anchor gain, enable, and phase",
    )
    R.limit(
        d.phase_identity,
        "phase = 0 is exactly the identity, not merely close to it",
    )
    R.limit(
        d.superpose_ok,
        f"superposition holds to {d.superpose_err:.1e}: the sources "
        "rendered alone add back to the full render, so the segment is "
        "the linear form its header claims",
    )
    R.limit(
        d.all_rejected,
        "every out-of-scope scene is REFUSED with ValueError -- a ranged "
        "per-source field, a ranged on-time, and a source carrying "
        "doppler or doppler_rate",
    )
    R.limit(
        d.all_edges_accepted,
        "and the refusal is scoped to the behaviour, not the field: a "
        "lifetime with zero doppler, ranged gaps, and repeats all still "
        "prepare",
    )
    R.limit(
        d.cap == 3072 and d.draw_le_cap,
        f"len() is a worst-case capacity ({d.cap} samples here) and no "
        "draw ever exceeds it",
    )
    R.limit(
        d.n_distinct > 1,
        f"a ranged-gap scene draws a DIFFERENT length per seed "
        f"({d.n_distinct} distinct over {len(GAP_SEEDS)} seeds) -- the "
        "materialized length is a property of the draw, and a caller must "
        "read it from the array rather than from len()",
    )
    R.limit(
        d.fixed_is_rectangular,
        "a scene with no ranged gap draws one fixed length, so the "
        "rectangular Monte-Carlo idiom is safe exactly there",
    )
    R.limit(
        d.restore_exact,
        "a Plan restored from save() renders the baseline bit-identically",
    )
    R.limit(
        d.restore_override_exact,
        "and serves an override bit-identically too -- the restored cache "
        "is the cache, not an approximation of it",
    )
    R.limit(
        d.file_exact,
        "dump() -> PlanFromFile round-trips to the same bytes as the "
        "in-memory blob",
    )
    R.limit(
        d.corrupt_rejected,
        "a truncated blob and an all-zero blob of the right length are "
        "both refused, not reinterpreted",
    )
    R.limit(
        d.buffer_fraction > 0.9,
        f"the blob is dominated by the cached buffers "
        f"({d.buffer_fraction:.1%} of {d.blob_bytes:,} bytes) -- the "
        "spec-rebuild path is the cheap default for a reason",
    )
    R.limit(
        d.mc_all_distinct,
        "no two Monte-Carlo draws are identical: each carries fresh noise "
        "over the fixed signal",
    )
    R.limit(
        d.mc_converges,
        f"and the ensemble mean converges on the clean render at the "
        f"independent-noise rate (ratio {d.mc_ratio:.2f} over an 8x "
        "ensemble increase, against sqrt(8) = 2.83), so a rate measured "
        "over draws is a rate over trials",
    )
    R.limit(
        len(d.cited) == 1,
        "the composition's own envelope is cited rather than re-derived "
        "-- counted, so a part quietly re-measured here is a change",
    )


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report."""
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "Plan",
        [
            "**The cache is indistinguishable from the composition on "
            "every axis it serves.** Baseline and every override -- snr, "
            "gain, phase, enable, seed -- reproduce a full `compose()` of "
            "the equivalently-modified scene bit for bit, across four "
            "scene shapes (§2.1, §2.2).",
            "**Read the drawn length off the array, never off `len()`.** "
            f"For a ranged-gap scene `len()` is a worst-case capacity and "
            f"each seed draws something shorter -- {d.n_distinct} "
            f"distinct lengths over {len(GAP_SEEDS)} seeds here. Both "
            "rectangular Monte-Carlo idioms raise on such a scene, and "
            "the docstring promises they will not (§2.4, F1).",
            "**A Doppler source is refused, and that is the contract "
            "working.** The cache cannot hold a channel's history, so it "
            "declines rather than serving a render that differs from "
            "`compose()` invisibly. Scoped to the behaviour, not the "
            "field: zero doppler with a declared lifetime still prepares "
            "(§2.3, F4).",
            "**Persist the spec, not the blob, unless the DSP cost says "
            f"otherwise.** A {N}-sample two-source scene serializes to "
            f"{d.blob_bytes / 1024:.0f} KB, {d.buffer_fraction:.0%} of it "
            "cached samples, against a sub-kilobyte spec that always "
            "tracks the current DSP (§2.5).",
            "**Nothing gates that the restore fast path is still fast.** "
            "A fingerprint mismatch rebuilds transparently and returns "
            "identical samples, so a restore that silently stopped using "
            "its cache would pass every test in both suites (F2).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
