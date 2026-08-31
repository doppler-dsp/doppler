"""Composer — certification evidence for the COMPOSITION.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_compose/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`.

**This is the composition, so its subject is the SEAM.** Synth, Writer,
Reader and Frame are certified in their own folders and this report
cites their limits rather than re-deriving them: nothing here re-measures
a waveform's SNR contract or a frame's layout. What it measures is what
only appears when the parts are put together --

- that the standalone `Synth` face and the composed face are the SAME
  waveform, which is the claim four functions in `wfm_compose.h` exist
  to make true and which nothing compared;
- that summing is addition, and a segment's off-time carries the noise
  floor its on-time had rather than a hole;
- that a repeat gets fresh noise over a fixed signal.

The order is the campaign's: `native/inc/wfm/wfm_compose.h` is the SSOT
and `native/tests/test_wfm_compose.c` certifies it in C.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import Composer, Segment, Synth

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

FS = 1.0e6
N = 4000

CODE = np.array([1, 0, 0, 1, 1, 0, 1, 0], np.uint8)
PAY = np.array([1, 0, 1, 1, 0], np.uint8)

# The source declarations both faces are asked to build. `dsss` is the one
# that matters: for every other type the shared SNR helper is a
# pass-through, so only dsss can show the two faces drifting.
FACE_CASES = [
    ("tone", {"type": "tone", "freq": 1.0e5, "snr": 100.0}),
    ("bpsk", {"type": "bpsk", "sps": 8, "snr": 100.0, "seed": 3}),
    (
        "qpsk",
        {"type": "qpsk", "sps": 4, "snr": 9.0, "snr_mode": "esno", "seed": 5},
    ),
    ("pn", {"type": "pn", "sps": 2, "snr": 100.0, "pn_length": 9, "seed": 7}),
    ("noise", {"type": "noise", "snr": 100.0, "seed": 11}),
    (
        "dsss",
        {
            "type": "dsss",
            "sps": 2,
            "snr": 9.0,
            "snr_mode": "esno",
            "seed": 3,
            "acq_code": CODE,
            "acq_reps": 3,
            "data_code": CODE,
            "bits": PAY,
        },
    ),
]


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def composed(**kw) -> np.ndarray:
    """One segment, composed."""
    return np.asarray(Composer([Segment(fs=FS, **kw)]).compose())


def power(x: np.ndarray) -> float:
    """Mean power, in double."""
    z = np.asarray(x).astype(np.complex128)
    return float(np.mean(np.abs(z) ** 2))


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    face_rows: list[list[str]] = field(default_factory=list)
    faces_all_exact: bool = False
    dsss_face_exact: bool = False
    sum_exact: bool = False
    sum_rows: list[list[str]] = field(default_factory=list)
    gap_clean_zero: bool = False
    gap_carries_floor: bool = False
    gap_on: float = 0.0
    gap_off: float = 0.0
    gap_ratio: float = 0.0
    repeats_len_exact: bool = False
    repeats_noise_fresh: bool = False
    repeats_signal_fixed: bool = False
    repeat_rows: list[list[str]] = field(default_factory=list)
    gap_continues: bool = False
    floor_once_ratio: float = 0.0
    floor_placed_once: bool = False
    concat_exact: bool = False
    json_exact: bool = False
    level_rows: list[list[str]] = field(default_factory=list)
    level_exact: bool = False
    delay_rows: list[list[str]] = field(default_factory=list)
    delay_exact: bool = False
    cited: list[str] = field(default_factory=list)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Composer` renders a scene: segments in time, each carrying one or "
        "more sources, each source a `Synth`. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md) "
        "and [wfmgen-composition.md]"
        "(../../../../../../docs/design/wfmgen-composition.md); the API is "
        "`native/inc/wfm/wfm_compose.h`, certified in C by "
        "`native/tests/test_wfm_compose.c`."
    )
    R.md()
    R.md(
        "**This is the composition, so its subject is the seam.** The parts "
        "are certified separately and their limits are cited rather than "
        "re-derived -- see §2.5. What is measured here is only what appears "
        "when they are put together."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_faces(d: Data) -> None:
    R.md("### 2.1 The two faces are the same waveform (C §seam)")
    R.md()
    R.md(
        "Four functions in `wfm_compose.h` exist for no reason except to "
        "stop the two synth-construction faces drifting, and each says so: "
        '`wfm_source_create_snr` is *"the one create-time entry point '
        "shared by the composer and the standalone-Synth bridge, so every "
        'face agrees to the bit"*. Nothing compared them. The C suite '
        'deferred it (*"covered from Python, where that face actually '
        'lives"*) and the Python three-faces test compares '
        "kwargs-`Composer`, `from_json` and the CLI -- three spellings of "
        "the COMPOSER path."
    )
    R.md()
    R.md(
        "So: the same declaration built as a standalone `Synth` and as a "
        "one-source `Segment`, compared sample for sample."
    )
    R.md()
    ok, dsss_ok = True, False
    for label, kw in FACE_CASES:
        a = np.asarray(Synth(fs=FS, **kw).steps(N))
        b = composed(num_samples=N, **kw)
        n = min(len(a), len(b))
        same = bool(np.array_equal(a[:n], b[:n]))
        ok = ok and same
        if label == "dsss":
            dsss_ok = same
        d.face_rows.append(
            [
                label,
                str(kw.get("snr_mode", "auto")),
                f"{kw['snr']:.0f}",
                "bit-exact" if same else "**DIFFERS**",
            ]
        )
    d.faces_all_exact, d.dsss_face_exact = ok, dsss_ok
    R.table(
        ["type", "snr_mode", "snr dB", "standalone vs composed"], d.face_rows
    )
    R.md(
        "**The `dsss` row is the one that can fail.** For every other type "
        "the shared SNR helper is a pass-through, so a face that bypassed "
        "it entirely would still agree -- measured, by sabotage: skipping "
        "the helper in the bridge left the five other rows green. A dsss "
        "source at a data-symbol Es/N0 is where the pre-referral runs, and "
        "it is the only shape that can see the drift."
    )
    R.md()


def measure_sum(d: Data) -> None:
    R.md("### 2.2 Summing is addition, exactly")
    R.md()
    R.md(
        "Two sources in one segment must be the sum of the same two sources "
        "rendered alone -- no scaling, no normalisation applied behind the "
        "caller's back. Scored against numpy's `+` on the two solo renders."
    )
    R.md()
    a_kw = {"type": "tone", "freq": 1.0e5, "snr": 100.0}
    b_kw = {"type": "tone", "freq": 2.0e5, "snr": 100.0}
    a = composed(num_samples=N, **a_kw)
    b = composed(num_samples=N, **b_kw)
    seg = Segment.sum(
        Synth(fs=FS, **a_kw), Synth(fs=FS, **b_kw), fs=FS, num_samples=N
    )
    c = np.asarray(Composer([seg]).compose())
    d.sum_exact = bool(np.array_equal(c, a + b))
    d.sum_rows = [
        ["solo A", f"{power(a):.4f}"],
        ["solo B", f"{power(b):.4f}"],
        ["summed segment", f"{power(c):.4f}"],
        ["A + B in numpy", f"{power(a + b):.4f}"],
    ]
    R.table(["render", "mean power"], d.sum_rows)
    R.md(
        "Bit-identical to the numpy sum"
        + ("." if d.sum_exact else " **-- except it is not.**")
        + " Two unit-power tones at different frequencies sum to about "
        "2.0, which is the point: the composer does not quietly divide by "
        "the source count, so a caller's declared levels survive and "
        "headroom stays theirs to set."
    )
    R.md()
    R.md("`level` is the caller's knob for that, in dB, and it is exact:")
    R.md()
    lv_ok = True
    for lv in (0.0, -6.0, -20.0):
        y = composed(
            type="tone", freq=0.0, num_samples=2000, snr=100.0, level=lv
        )
        got, want = power(y), 10.0 ** (lv / 10.0)
        lv_ok = lv_ok and abs(got / want - 1.0) < 1e-4
        d.level_rows.append(
            [f"{lv:.0f}", f"{got:.6f}", f"{want:.6f}", f"{got / want:.4f}"]
        )
    d.level_exact = lv_ok
    R.table(
        ["level dB", "measured power", "10^(dB/10)", "ratio"], d.level_rows
    )
    R.md()
    R.md(
        "**Segments concatenate in order, and a scene round-trips through "
        "JSON.** Both are what make a scene a declaration rather than a "
        "script."
    )
    R.md()
    segs = [
        Segment(type="tone", freq=1.0e5, fs=FS, num_samples=1000, snr=100.0),
        Segment(type="tone", freq=2.0e5, fs=FS, num_samples=1500, snr=100.0),
    ]
    x = np.asarray(Composer(segs).compose())
    solo = [np.asarray(Composer([s]).compose()) for s in segs]
    d.concat_exact = bool(
        len(x) == 2500 and np.array_equal(x, np.concatenate(solo))
    )
    y = np.asarray(Composer.from_json(Composer(segs).to_json()).compose())
    d.json_exact = bool(np.array_equal(x, y))
    R.table(
        ["property", "result"],
        [
            [
                "two segments concatenate to 2500 samples, in order",
                "exact" if d.concat_exact else "**DIFFERS**",
            ],
            [
                "the scene survives to_json -> from_json",
                "exact" if d.json_exact else "**DIFFERS**",
            ],
        ],
    )
    R.md()


def measure_gap(d: Data) -> None:
    R.md("### 2.3 A gap carries the floor its on-time had")
    R.md()
    R.md(
        "A segment's off-time is where the composition can differ from any "
        "part on its own: a source that is silent must still leave the "
        "scene's noise where it was, or a receiver's AGC and its detector "
        "see a hole that no real link has. A clean source's gap is exactly "
        "zero; a noisy one's is its own floor, continued."
    )
    R.md()
    rows = []
    for label, kw in [
        ("clean", {"snr": 100.0}),
        ("noisy", {"snr": 6.0, "snr_mode": "fs"}),
    ]:
        x = composed(
            type="tone",
            freq=0.0,
            num_samples=2000,
            off_samples=1000,
            seed=4,
            **kw,
        )
        on, off = x[:2000], x[2000:3000]
        p_on, p_off = power(on), power(off)
        rows.append(
            [
                label,
                str(len(x)),
                f"{p_on:.4f}",
                f"{p_off:.6f}",
                "yes" if np.all(np.asarray(off) == 0) else "no",
            ]
        )
        if label == "clean":
            d.gap_clean_zero = bool(np.all(np.asarray(off) == 0))
        else:
            d.gap_on, d.gap_off = p_on, p_off
            # the on-time is a unit-power tone plus noise, so its noise is
            # p_on - 1; the gap should carry the same
            noise_on = p_on - 1.0
            d.gap_ratio = p_off / noise_on if noise_on > 0 else 0.0
            d.gap_carries_floor = abs(d.gap_ratio - 1.0) < 0.10
    R.table(
        ["source", "total samples", "P(on)", "P(off)", "gap all zero"], rows
    )
    R.md(
        f"The clean gap is exactly zero -- no AWGN child exists, so there "
        f"is nothing to continue. The noisy one carries "
        f"{d.gap_off:.4f} against the {d.gap_on - 1.0:.4f} of noise in its "
        f"own on-time, a ratio of {d.gap_ratio:.3f}: the same floor, not a "
        "hole and not a second, louder one."
    )
    R.md()
    R.md(
        "**And it is the same noise, not merely the same amount.** The gap "
        "must consume the sub-sequence the on-time would have drawn, or a "
        "capture is not reproducible across a face that renders the gap "
        "differently. Rendered two ways -- 1000 on then 3000 off, against "
        "4000 on -- and the gap compared with the on-time it replaced:"
    )
    R.md()
    kw = {"type": "noise", "snr": 100.0, "seed": 4}
    a = composed(num_samples=1000, off_samples=3000, **kw)
    b = composed(num_samples=4000, **kw)
    gap, ont = np.asarray(a[1000:4000]), np.asarray(b[1000:4000])
    d.gap_continues = bool(np.array_equal(gap, ont))
    R.md(
        "Bit-exact"
        + (
            " -- the gap is the on-time's own noise stream, continued."
            if d.gap_continues
            else " **-- except it is not.**"
        )
    )
    R.md()
    R.md(
        "**A multi-source segment places the floor once.** Two sources do "
        "not mean two noise floors: the segment carries one, anchored, or "
        "a scene's SNR would depend on how many signals happened to share "
        "a segment."
    )
    R.md()
    s = {"type": "tone", "snr": 6.0, "snr_mode": "fs", "seed": 4}
    one = composed(freq=0.0, num_samples=20000, **s)
    seg = Segment.sum(
        Synth(fs=FS, freq=0.0, **s),
        Synth(fs=FS, freq=2.0e5, **s),
        fs=FS,
        num_samples=20000,
    )
    two = np.asarray(Composer([seg]).compose())
    n1, n2 = power(one) - 1.0, power(two) - 2.0
    d.floor_once_ratio = n2 / n1 if n1 > 0 else 0.0
    d.floor_placed_once = abs(d.floor_once_ratio - 1.0) < 0.10
    R.table(
        ["segment", "mean power", "signal", "noise"],
        [
            ["one source", f"{power(one):.4f}", "1", f"{n1:.4f}"],
            ["two sources", f"{power(two):.4f}", "2", f"{n2:.4f}"],
        ],
    )
    R.md(
        f"The noise is {d.floor_once_ratio:.3f} of the one-source figure -- "
        "one floor, not one per source (which would read 2.0)."
    )
    R.md()
    R.md(
        "**A source's `delay_samples` moves it inside its segment**, and "
        "the segment grows by exactly that much rather than the signal "
        "being clipped off the front. What sits before it is the floor, "
        "which for a clean source is silence."
    )
    R.md()
    ok = True
    for dly in (0, 200, 500):
        y = composed(
            type="tone",
            freq=0.0,
            num_samples=1000,
            snr=100.0,
            delay_samples=dly,
        )
        head_zero = bool(np.all(np.asarray(y[:dly]) == 0)) if dly else True
        after = power(np.asarray(y[dly : dly + 500]))
        row_ok = len(y) == 1000 + dly and head_zero and abs(after - 1.0) < 1e-6
        ok = ok and row_ok
        d.delay_rows.append(
            [
                str(dly),
                str(len(y)),
                str(1000 + dly),
                "silent" if head_zero else "**not silent**",
                f"{after:.4f}",
            ]
        )
    d.delay_exact = ok
    R.table(
        ["delay", "rendered", "expected", "before the delay", "P(after)"],
        d.delay_rows,
    )
    R.md()


def measure_repeats(d: Data) -> None:
    R.md("### 2.4 A repeat is fresh noise over a fixed signal")
    R.md()
    R.md(
        "`repeats` is what makes a burst train out of one declaration, and "
        "the useful behaviour is asymmetric: the signal must be the same "
        "burst every time, or a receiver cannot be scored against one "
        "truth, while the noise must NOT be, or every instance is the same "
        "trial and a detection rate over them means nothing."
    )
    R.md()
    # 20 dB over fs, not 6: at 6 dB the per-component sigma is 0.354
    # against a +-1 symbol, so noise alone flips a decision about once in
    # 400 -- and "every symbol matches" would then be measuring the SNR,
    # not whether the burst moved. At 20 dB a flip needs ~14 sigma, so any
    # disagreement below IS the signal moving. The noise is still plainly
    # there: the instances differ (see the rms column).
    per = 800
    x = composed(
        type="bpsk",
        sps=8,
        num_samples=per,
        snr=20.0,
        snr_mode="fs",
        seed=9,
        repeats=3,
    )
    d.repeats_len_exact = len(x) == 3 * per
    inst = [np.asarray(x[i * per : (i + 1) * per]) for i in range(3)]
    clean = composed(type="bpsk", sps=8, num_samples=per, snr=100.0, seed=9)
    sig = np.asarray(clean)
    fresh = not any(
        np.array_equal(inst[i], inst[j]) for i in range(3) for j in range(i)
    )
    d.repeats_noise_fresh = fresh
    # the SIGNAL is fixed: every instance's symbol decisions match the clean
    # render's, which noise at this level cannot flip
    centres = slice(4, per, 8)
    want = np.sign(np.real(sig[centres]))
    fixed = True
    for i, r in enumerate(inst):
        got = np.sign(np.real(r[centres]))
        agree = float(np.mean(got == want))
        fixed = fixed and agree == 1.0
        d.repeat_rows.append(
            [
                str(i),
                f"{float(np.sqrt(np.mean(np.abs(r - sig) ** 2))):.4f}",
                f"{agree * 100:.1f}%",
            ]
        )
    d.repeats_signal_fixed = fixed
    R.table(
        [
            "instance",
            "rms(instance - clean)",
            "symbols matching the clean render",
        ],
        d.repeat_rows,
    )
    R.md(
        "Every instance differs from every other, and every instance's "
        "symbol decisions are the clean render's -- the noise moved and the "
        "burst did not. Measured at 20 dB over fs on purpose: at a level "
        "where noise itself flips decisions, 'every symbol matches' would "
        "be measuring the SNR rather than whether the burst is fixed."
    )
    R.md()


def measure_cited(d: Data) -> None:
    R.md("### 2.5 What this report cites rather than re-derives")
    R.md()
    R.md(
        "The composition's report may cite its children's limits, and "
        "should: re-measuring a part through the composition certifies the "
        "same engine twice and presents the second run as independent "
        "evidence."
    )
    R.md()
    d.cited = [
        (
            "**Synth** -- the SNR contract in all three references, the "
            "pulse-shaping power and band, the waveform types, and the "
            "state hand-off. §2.1 here relies on the waveform being right; "
            "it measures only that both faces produce the SAME one."
        ),
        (
            "**Frame** -- the layout, the declared covers, and that the "
            "check needs no payload truth. A framed source composed here "
            "is the same descriptor, and §2.1's dsss row carries one."
        ),
        (
            "**Writer / Reader** -- everything past the composer's output "
            "buffer. A composed scene written and read back is those two "
            "objects' envelope, not this one's."
        ),
    ]
    for c in d.cited:
        R.md(f"- {c}")
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Each heading names the C section "
        "it tracks where there is one; the numbering is this report's own."
    )
    R.md()
    d = Data()
    measure_faces(d)
    measure_sum(d)
    measure_gap(d)
    measure_repeats(d)
    measure_cited(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The functions whose only job is that the faces agree had "
        "nothing asserting the agreement.** `wfm_source_create_snr` -- "
        '*"the one create-time entry point shared by the composer and the '
        'standalone-Synth bridge, so every face agrees to the bit"* -- and '
        '`wfm_source_attach_frame` -- *"called from the same two places '
        'for the same reason"* -- both had ZERO mentions in any C test in '
        "the tree. And the comparison itself existed nowhere: the C suite "
        "deferred it to Python, and Python's three-faces test compares "
        "three spellings of the composer path. Closed by pinning "
        "`create_snr` against the arithmetic the header states (not "
        "against `wfm_snr_over_fs`, which is the same conversion and would "
        "move with it) and by one `memcmp` between the two faces. Four "
        "sabotages red on the helper; a fifth on the bridge.",
    )
    R.find(
        "F2",
        "FIXED",
        "**A face-agreement test is blind unless it uses the shape the "
        "shared code actually changes.** The first version of §2.1 covered "
        "five waveform types, and for every one of them "
        "`wfm_source_create_snr` is a pass-through -- so sabotaging the "
        "bridge to skip the shared helper ENTIRELY, which is exactly the "
        "drift the section exists to catch, left it green. Only a dsss "
        "source at a data-symbol Es/N0 reaches the pre-referral. Found by "
        "sabotage, fixed by adding that case, and recorded because it "
        "generalises: a test that two paths agree proves nothing on inputs "
        "where the paths do not differ, and 'more cases' is not the same "
        "as the RIGHT case.",
    )
    R.find(
        "F3",
        "BY DESIGN",
        "**Summing does not normalise.** Two unit-power sources in one "
        "segment give about 2.0, not 1.0 -- the composer adds and leaves "
        "the level to the caller. That is the right default for a stimulus "
        "generator (a declared level must survive) and it is the thing a "
        "caller most often expects otherwise, so it is measured (§2.2) "
        "rather than left to be discovered against a clipped capture. "
        "Headroom is `Writer`'s knob and is certified there.",
    )
    R.find(
        "F4",
        "C-ONLY",
        "**`wfm_compose_build_render` and `wfm_compose_from_file` are not "
        "on the Python face**, and neither had a C test. `from_file` is "
        "the CLI's `--from-file` path -- Python reaches the same parser "
        "through `Composer.from_json`, which IS tested and is compared "
        "against the CLI byte-for-byte in `test_dsss_source.py`. "
        "`build_render` is the renderer the Plan cache and the streaming "
        "composer share; it is exercised through both, and `Plan` is the "
        "next object in this campaign, where it is the subject rather than "
        "a detail.",
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
        d.faces_all_exact,
        "a standalone Synth and a one-source Segment are the SAME waveform, "
        "sample for sample, across six source types",
    )
    R.limit(
        d.dsss_face_exact,
        "including a dsss source at a data-symbol Es/N0 -- the only shape "
        "where the shared SNR referral runs, and so the only one that can "
        "catch the two faces drifting",
    )
    R.limit(
        d.sum_exact,
        "a two-source segment is bit-identical to the numpy sum of the two "
        "sources rendered alone",
    )
    R.limit(
        d.gap_clean_zero,
        "a clean source's off-time is exactly zero -- no AWGN child exists, "
        "so there is nothing to continue",
    )
    R.limit(
        d.gap_carries_floor,
        f"a noisy source's off-time carries its own on-time floor "
        f"(ratio {d.gap_ratio:.3f}) -- a gap is silence in the SIGNAL, not "
        "a hole in the noise",
    )
    R.limit(
        d.repeats_len_exact,
        "repeats=3 renders exactly three instances of the declared span",
    )
    R.limit(
        d.repeats_noise_fresh,
        "no two instances are identical -- each repeat gets fresh noise, so "
        "a detection rate over them is a rate over trials",
    )
    R.limit(
        d.repeats_signal_fixed,
        "and every instance's symbol decisions are the clean render's: the "
        "noise moved and the burst did not",
    )
    R.limit(
        d.gap_continues,
        "and it is the SAME noise, bit-exact: a gap consumes the "
        "sub-sequence the on-time would have drawn, not a fresh one",
    )
    R.limit(
        d.floor_placed_once,
        f"a two-source segment carries ONE noise floor, not one per source "
        f"(ratio {d.floor_once_ratio:.3f} against the one-source figure)",
    )
    R.limit(
        d.level_exact,
        "level is exact in dB: the rendered power is 10^(level/10) at every "
        "level measured",
    )
    R.limit(
        d.concat_exact,
        "segments concatenate in declaration order, and the total span is "
        "the sum of theirs",
    )
    R.limit(
        d.json_exact,
        "a scene survives to_json -> from_json bit-for-bit, so a declared "
        "scene is a record rather than a script",
    )
    R.limit(
        d.delay_exact,
        "delay_samples moves a source inside its segment and lengthens the "
        "render by exactly that much -- the signal is delayed, not clipped",
    )
    R.limit(
        len(d.cited) == 3,
        "three children's envelopes are cited rather than re-derived -- "
        "counted, so a part quietly re-measured here is a change",
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
        "Composer",
        [
            "**The standalone and composed faces are one waveform.** The "
            "same declaration built as a `Synth` or as a one-source "
            "`Segment` is bit-identical across six types -- which four "
            "functions in the header exist to guarantee and which nothing "
            "compared until now (§2.1, F1).",
            "**Summing does not normalise.** Two unit-power sources give "
            "about 2.0; the composer adds and the level stays yours. Set "
            "headroom at the writer, not by expecting the composer to "
            "scale (§2.2, F3).",
            "**A gap is silence in the signal, not a hole in the noise.** A "
            f"noisy source's off-time carries its own floor to "
            f"{abs(d.gap_ratio - 1) * 100:.0f}%, and a clean source's gap "
            "is exactly zero (§2.3).",
            "**A repeat re-rolls the noise and not the burst.** Every "
            "instance differs, and every instance's symbols are the clean "
            "render's -- which is what makes a detection rate over "
            "instances a rate over trials (§2.4).",
            "**A face-agreement test is blind on inputs where the paths do "
            "not differ.** Five of the six types here would have passed a "
            "bridge that skipped the shared SNR helper entirely. Only the "
            "dsss case reaches it, and 'more cases' would not have found "
            "that -- the right case did (F2).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
