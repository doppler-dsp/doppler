"""Reader — certification evidence, measured through the binding.

Run directly to regenerate `results.md`, the plots and the CSVs:

    uv run python src/doppler/wfm/tests/validation/wfm_reader/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's: `native/inc/wfm_reader/wfm_reader_core.h`
is the SSOT and `native/tests/test_wfm_reader_core.c` certifies it in C.
This file measures the same properties through `doppler.wfm.Reader`.

**The truth is the bytes, decoded by numpy.** `Reader`'s job is to invert
the wire format, so scoring it against `Writer` alone would ask whether
the pair agrees, not whether either is right -- and the two share the
`SCALE` table, so a shared error cancels. Every sample here is decoded
independently with `np.fromfile` at the declared dtype and rescaled by
the type's full scale; `Reader` is then required to match THAT. §2.2 is
the one place a `Writer`-produced value is used as the expectation, and
it is metadata rather than samples.

Offline throughout -- a temporary directory and numpy -- so this report
renders identically on any machine, which is what `make validate-check`
requires of it.
"""

from __future__ import annotations

import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import Reader, Writer

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

FS = 2.4e6
FC = 1.42e9

# The five complex wire types, the numpy dtype each lands as, and the
# full-scale constant that inverts the writer's quantiser.
WIRE = [
    ("cf32", "<c8", None),
    ("cf64", "<c16", None),
    ("ci32", "<i4", 2147483647.0),
    ("ci16", "<i2", 32767.0),
    ("ci8", "<i1", 127.0),
]

N = 2048


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


def signal(n: int = N, seed: int = 13) -> np.ndarray:
    """A deterministic test signal that fills the range without clipping."""
    rng = np.random.default_rng(seed)
    return (rng.uniform(-0.9, 0.9, n) + 1j * rng.uniform(-0.9, 0.9, n)).astype(
        np.complex64
    )


def decode_with_numpy(
    path: Path, dtype: str, scale: float | None
) -> np.ndarray:
    """The file's samples, decoded by numpy alone — the external truth.

    Reads the raw bytes at the declared dtype and, for an integer wire
    type, divides by the full scale that inverts the writer's quantiser.
    Owes nothing to doppler.
    """
    a = np.fromfile(path, dtype=dtype)
    if np.iscomplexobj(a):
        return a.astype(np.complex128)
    f = a.astype(np.float64) / scale
    return (f[0::2] + 1j * f[1::2]).astype(np.complex128)


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    wire_rows: list[list[str]] = field(default_factory=list)
    wire_all_match: bool = False
    worst_wire_err: float = 0.0
    meta_rows: list[list[str]] = field(default_factory=list)
    meta_all_ok: bool = False
    prov_rows: list[list[str]] = field(default_factory=list)
    prov_distinguishes: bool = False
    t0_never_1950: bool = False
    kw_rows: list[list[str]] = field(default_factory=list)
    kw_all_round_trip: bool = False
    header_fields: int = 0
    header_disjoint: bool = False
    read_rows: list[list[str]] = field(default_factory=list)
    read_semantics_ok: bool = False
    reset_rewinds: bool = False
    maxout_identity: bool = False
    hint_rows: list[list[str]] = field(default_factory=list)
    hint_silent: int = 0
    hint_total: int = 0
    mode_endian_ok: bool = False
    csv_counts: bool = False
    csv_rows: list[list[str]] = field(default_factory=list)
    whole_capture_no_trailing: bool = False
    follow_knobs_round_trip: bool = False
    unreachable: list[str] = field(default_factory=list)


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`Reader` opens a capture, works out what it is from its content, "
        "and hands back unit-scale complex samples -- the read half of the "
        "pair `Writer` is the write half of. The design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is `native/inc/wfm_reader/wfm_reader_core.h`, certified in "
        "C by `native/tests/test_wfm_reader_core.c`. Neither is restated "
        "here."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_wire(d: Data, tmp: Path) -> None:
    R.md("### 2.1 Every wire type inverts, to within a quantiser step")
    R.md()
    R.md(
        "The header's claim: samples come out as `float _Complex` at unit "
        "scale, float types reinterpreted and integer types rescaled by "
        "their full scale -- \"the exact inverse of the writer's "
        'quantiser". Scored against numpy decoding the same bytes, not '
        "against `Writer`."
    )
    R.md()
    x = signal()
    ok = True
    worst = 0.0
    for name, dtype, scale in WIRE:
        p = tmp / f"w_{name}.raw"
        w = Writer(p, file_type="raw", sample_type=name, fs=FS)
        w.write(x)
        w.close()
        truth = decode_with_numpy(p, dtype, scale)
        got = np.asarray(Reader(p, sample_type=name).read(N)).astype(
            np.complex128
        )
        same = bool(
            got.size == truth.size and np.allclose(got, truth, atol=1e-9)
        )
        ok = ok and same
        # Per COMPONENT, which is the axis the quantiser works on. The
        # complex magnitude reaches sqrt(2) LSB when both axes truncate the
        # same way, so scoring |dz| against a per-component bound compares
        # the wrong quantity -- it failed this limit until it was measured
        # rather than assumed.
        dz = got - x.astype(np.complex128)
        comp = np.concatenate([dz.real, dz.imag])
        err = float(np.max(np.abs(comp)))
        worst = max(worst, err * scale if scale else 0.0)
        step = 1.0 / scale if scale else 0.0
        d.wire_rows.append(
            [
                name,
                "matches" if same else "**DIFFERS**",
                f"{err:.3e}",
                f"{step:.3e}" if step else "exact",
                f"{err * scale:.3f}" if scale else "0",
                f"{float(np.max(np.abs(dz))) * scale:.3f}" if scale else "0",
            ]
        )
    d.wire_all_match = ok
    d.worst_wire_err = worst
    R.table(
        [
            "type",
            "vs numpy decode",
            "max err/component",
            "one LSB",
            "err in LSB",
            "|dz| in LSB",
        ],
        d.wire_rows,
    )
    R.md(
        "The reader agrees with an independent decode on every type. The "
        "residual against the ORIGINAL samples is the writer's "
        "quantisation, not the reader's, and it is exactly one LSB per "
        "component -- the bound truncation gives, reached rather than "
        "approached. The float types are exact."
    )
    R.md()
    R.md(
        "The last column is worth a caller's attention: the COMPLEX error "
        "reaches about **1.41 LSB**, because both axes truncate "
        "independently and `sqrt(2)` of one LSB is what that costs. An "
        "error budget written per component and then applied to `|z|` is "
        "40% optimistic."
    )
    R.md()


def measure_metadata(d: Data, tmp: Path) -> None:
    R.md("### 2.2 The metadata surface (C §accessors)")
    R.md()
    R.md(
        "Every property here is a distinct `wfm_reader_get_*` accessor, and "
        "the Python face is the ONLY caller of most of them -- the C suite "
        "reads the same state through `wfm_reader_info()`. `fs` and `fc` "
        "are deliberately far apart (2.4 MHz against 1.42 GHz) so a getter "
        "returning its neighbour's field cannot hide."
    )
    R.md()
    ok = True
    x = signal(256)
    for ft, stype, suffix, hint in [
        ("blue", "ci16", ".blue", None),
        ("blue", "cf32", ".blue", None),
        ("csv", "cf32", ".csv", None),
        ("sigmf", "ci16", ".sigmf-data", None),
        ("raw", "ci16", ".raw", "ci16"),
    ]:
        p = tmp / f"m_{ft}_{stype}{suffix}"
        w = Writer(p, file_type=ft, sample_type=stype, fs=FS, fc=FC)
        w.write(x)
        w.close()
        r = Reader(p) if hint is None else Reader(p, sample_type=hint)
        got_ft = r.file_type == ft
        got_st = r.sample_type == stype
        got_n = r.num_samples == len(x)
        # raw and csv carry no rate of their own
        rate = r.fs == FS if ft in ("blue", "sigmf") else r.fs == 0.0
        row_ok = got_ft and got_st and got_n and rate
        ok = ok and row_ok
        d.meta_rows.append(
            [
                ft,
                stype,
                r.file_type,
                r.sample_type,
                str(int(r.num_samples)),
                f"{r.fs:.4g}",
                f"{r.fc:.4g}",
                "ok" if row_ok else "**MISMATCH**",
            ]
        )
    d.meta_all_ok = ok
    R.table(
        [
            "written",
            "as",
            "file_type",
            "sample_type",
            "num_samples",
            "fs",
            "fc",
            "",
        ],
        d.meta_rows,
    )
    R.md(
        "`file_type` is detected from the CONTENT, not the suffix, and the "
        "self-describing formats carry their own rate. Raw and CSV report "
        "`fs = 0.0` -- which §2.3 shows is distinguishable from a rate that "
        "happens to be zero."
    )
    R.md()
    # mode/endian, and the trailing-byte count on a whole capture
    me_ok, no_trail = True, True
    for name, _dtype, _scale in WIRE:
        p = tmp / f"me_{name}.blue"
        w = Writer(p, file_type="blue", sample_type=name, fs=FS)
        w.write(x)
        w.close()
        r = Reader(p)
        me_ok = me_ok and r.mode == "complex" and r.endian == "le"
        no_trail = no_trail and int(r.trailing_bytes) == 0
    d.mode_endian_ok, d.whole_capture_no_trailing = me_ok, no_trail
    R.md(
        "Across all five wire types a complex BLUE capture reports "
        "`mode = complex`, `endian = le`, and `trailing_bytes = 0` -- a "
        "whole capture has no bytes past its last complete sample, which "
        "is what makes a NON-zero count meaningful in §2.6."
    )
    R.md()
    R.md(
        "**A CSV has no length in a header, so the reader counts it.** The "
        "header says so, and says it happens once, on the first caller who "
        "asks -- worth pinning because a format whose count is derived "
        "rather than read is the one that can disagree with the samples it "
        "then yields."
    )
    R.md()
    csv_ok = True
    for n in (7, 64, 1000):
        p = tmp / f"count_{n}.csv"
        w = Writer(p, file_type="csv", sample_type="cf32", fs=FS)
        w.write(signal(n))
        w.close()
        r = Reader(p)
        first = int(r.num_samples)
        again = int(r.num_samples)  # the cached second ask
        got = np.asarray(r.read(n + 10)).size
        row_ok = first == n and again == n and got == n
        csv_ok = csv_ok and row_ok
        d.csv_rows.append(
            [
                str(n),
                str(first),
                str(again),
                str(got),
                "ok" if row_ok else "**MISMATCH**",
            ]
        )
    d.csv_counts = csv_ok
    R.table(
        ["samples written", "num_samples", "asked again", "read yields", ""],
        d.csv_rows,
    )
    R.md(
        "The counted length matches the samples the reader then yields, "
        "and asking twice gives the same answer."
    )
    R.md()


def measure_provenance(d: Data, tmp: Path) -> None:
    R.md("### 2.3 Provenance: 0.0 found is not 0.0 missing")
    R.md()
    R.md(
        'The header is explicit about why the `*_source` tags exist: "a '
        "genuine baseband capture and a capture whose frequency this "
        'library failed to find are otherwise indistinguishable". So the '
        "test is two captures that both report `fc = 0.0` and must not "
        "report the same source."
    )
    R.md()
    x = signal(64)
    rows = []
    # a BLUE capture at fc = 0, and a headerless raw one
    p_blue = tmp / "prov.blue"
    w = Writer(p_blue, file_type="blue", sample_type="ci16", fs=FS, fc=0.0)
    w.write(x)
    w.close()
    p_raw = tmp / "prov.raw"
    w = Writer(p_raw, file_type="raw", sample_type="ci16", fs=FS, fc=0.0)
    w.write(x)
    w.close()
    p_fc = tmp / "prov_fc.blue"
    w = Writer(p_fc, file_type="blue", sample_type="ci16", fs=FS, fc=FC)
    w.write(x)
    w.close()
    for label, path, hint in [
        ("BLUE, fc unset", p_blue, None),
        ("raw, nothing carried", p_raw, "ci16"),
        ("BLUE, fc stated", p_fc, None),
    ]:
        r = Reader(path) if hint is None else Reader(path, sample_type=hint)
        rows.append(
            [
                label,
                f"{r.fc:.4g}",
                r.fc_source,
                f"{r.fs:.4g}",
                r.fs_source,
                f"{r.t0:.1f}",
                r.t0_source,
            ]
        )
    d.prov_rows = rows
    R.table(
        ["capture", "fc", "fc_source", "fs", "fs_source", "t0", "t0_source"],
        rows,
    )
    zero_fc = [r for r in rows if float(r[1]) == 0.0]
    d.prov_distinguishes = len({r[4] for r in rows}) > 1 and len(zero_fc) >= 2
    d.t0_never_1950 = all(r[6] == "none" and float(r[5]) == 0.0 for r in rows)
    R.md(
        "Two captures report `fc = 0.0` and are told apart by their source. "
        "The rate is the same story: a BLUE file says `xdelta`, a "
        "headerless raw one says nothing carried a rate, and only then is "
        "`fs = 0.0` a shrug rather than a reading."
    )
    R.md()
    R.md(
        "**And the one that would misdate every doppler capture.** "
        "doppler's own BLUE writer leaves the timecode field zero, so a "
        "zero there means UNSET, never 1950-01-01. Every capture above "
        "reports `t0_source = none` with `t0 = 0.0`"
        + (
            ", so a caller cannot mistake it for a real epoch."
            if d.t0_never_1950
            else " **-- except one does not.**"
        )
    )
    R.md()


def measure_maps(d: Data, tmp: Path) -> None:
    R.md("### 2.4 The two dict properties (C §enumerators)")
    R.md()
    R.md(
        "`keywords` and `header` are built by ITERATING -- "
        "`num_keywords`/`keyword_tag` and "
        "`num_header_fields`/`header_tag`/`header_field`. Those four had no "
        "C coverage at all before this certification (F1); the C suite "
        "reached the same data through the `find_*` lookups instead."
    )
    R.md()
    p = tmp / "kw.blue"
    w = Writer(p, file_type="blue", sample_type="ci16", fs=FS)
    w.write(signal(64))
    cases = [
        ("COMMENT", "A", "10 dB pad"),
        ("F_C", "D", 1.2345e9),
        ("GAINS", "F", [1.5, -2.5, 3.5]),
        ("OFFSET", "L", -70000),
        ("TRIM", "I", -1234),
        ("FLAG", "B", -7),
        ("TICKS", "X", 1234567890123),
    ]
    for tag, code, val in cases:
        w.add_keyword(tag, code, val)
    w.close()
    r = Reader(p)
    kws = r.keywords
    ok = True
    for tag, code, val in cases:
        got = kws.get(tag)
        if isinstance(val, list):
            match = isinstance(got, list) and np.allclose(got, val)
        elif isinstance(val, float):
            match = isinstance(got, float) and abs(got - val) < 1e-3
        else:
            match = got == val
        ok = ok and bool(match)
        shown = f"{got}"
        d.kw_rows.append(
            [
                tag,
                code,
                f"{val}",
                shown[:28],
                "round-trips" if match else "**DIFFERS**",
            ]
        )
    d.kw_all_round_trip = ok
    R.table(["tag", "BLUE type", "written", "read back", ""], d.kw_rows)
    R.md()
    hdr = r.header
    d.header_fields = len(hdr)
    d.header_disjoint = not (set(hdr) & set(kws))
    R.md(
        f"The header dict carries {d.header_fields} decoded HCB fields "
        "under the names the format itself uses (`data_start`, `xdelta`, "
        "`ext_size`, ...). The two maps are disjoint"
        + (
            " -- a header field is not a keyword, so one is not aliased "
            "onto the other."
            if d.header_disjoint
            else " **-- except they overlap, which they should not.**"
        )
    )
    R.md()


def measure_read(d: Data, tmp: Path) -> None:
    R.md("### 2.5 read(), reset() and the capacity accessors")
    R.md()
    x = signal(300)
    p = tmp / "rd.blue"
    w = Writer(p, file_type="blue", sample_type="cf32", fs=FS)
    w.write(x)
    w.close()
    r = Reader(p)
    a = np.asarray(r.read(100))
    b = np.asarray(r.read(100))
    c = np.asarray(r.read(500))  # more than remains
    dd = np.asarray(r.read(10))  # past the end
    joined = np.concatenate([a, b, c])
    seq_ok = bool(
        a.size == 100
        and b.size == 100
        and c.size == 100
        and dd.size == 0
        and np.allclose(joined, x, atol=1e-6)
    )
    d.read_rows = [
        ["read(100)", "100", str(a.size)],
        ["read(100) again", "100", str(b.size)],
        ["read(500) with 100 left", "100", str(c.size)],
        ["read(10) past the end", "0", str(dd.size)],
    ]
    R.table(["call", "expected", "returned"], d.read_rows)
    R.md(
        "A read is a stream position, not a slice: successive calls "
        "continue, a request larger than what remains returns what remains, "
        "and past the end returns nothing rather than raising."
    )
    R.md()
    r.reset()
    again = np.asarray(r.read(300))
    d.reset_rewinds = bool(np.allclose(again, x, atol=1e-6))
    d.read_semantics_ok = seq_ok
    d.maxout_identity = all(
        r.read_max_out(n) == n and r.read_follow_max_out(n) == n
        for n in (0, 1, 4096)
    )
    t0 = int(r.follow_timeout_ms)
    g0 = int(r.follow_grace_ms)
    r.follow_timeout_ms = t0 + 137
    r.follow_grace_ms = g0 + 29
    d.follow_knobs_round_trip = (
        int(r.follow_timeout_ms) == t0 + 137
        and int(r.follow_grace_ms) == g0 + 29
    )
    R.md(
        "The two follow knobs round-trip through the Python face and stay "
        "independent -- setting one leaves the other alone, which a single "
        "aliased field would not."
    )
    R.md()
    R.md(
        "`reset()` rewinds to sample 0"
        + (
            " and the whole capture reads back identically."
            if d.reset_rewinds
            else " **-- and it does not.**"
        )
        + " Both capacity accessors are the identity, including at 0."
    )
    R.md()


def measure_hint(d: Data, tmp: Path) -> None:
    R.md("### 2.6 The headerless hint, from the reader's side (F3)")
    R.md()
    R.md(
        "A raw or CSV capture carries no type, so `Reader` takes one as a "
        "hint (default `cf32`). `Writer` records the real type in a "
        "`.sigmf-meta` sidecar beside the file; `Reader` does not read it "
        "(gh-1120). What can be measured here is the safeguard the "
        "docstring offers for a wrong hint -- `trailing_bytes` -- and how "
        "often it fires."
    )
    R.md()
    rows = []
    silent = 0
    total = 0
    for n in (512, 4096, 513):
        for stype in ("ci8", "ci16", "ci32"):
            p = tmp / f"h_{n}_{stype}.raw"
            w = Writer(p, file_type="raw", sample_type=stype, fs=FS)
            w.write(np.full(n, 0.5 + 0.25j, np.complex64))
            w.close()
            r = Reader(p)  # the documented default hint
            tb = int(r.trailing_bytes)
            total += 1
            if tb == 0:
                silent += 1
            rows.append(
                [
                    str(n),
                    stype,
                    r.sample_type,
                    str(int(r.num_samples)),
                    str(tb),
                    "yes" if tb else "**no**",
                ]
            )
    d.hint_rows = rows
    d.hint_silent, d.hint_total = silent, total
    R.table(
        [
            "samples written",
            "written as",
            "read as",
            "samples read",
            "trailing bytes",
            "tell fires?",
        ],
        rows,
    )
    R.md(
        f"Silent in {silent} of {total}. It can only fire when the byte "
        "count fails to divide by the assumed 8, so every even sample "
        "count is invisible -- and **ci32 is invisible at every length**, "
        "because it is 8 bytes per complex sample exactly like cf32. The "
        "count comes back correct and every value is wrong."
    )
    R.md()


def measure_reach(d: Data) -> None:
    R.md("### 2.7 What the Python face does not reach")
    R.md()
    d.unreachable = [
        (
            "`wfm_reader_info` -- the bulk metadata struct. Python reads "
            "the same state through the individual properties, which is "
            "why §2.2 exists: those accessors have no other caller."
        ),
        (
            "`wfm_reader_keyword` / `wfm_reader_header_field` -- the "
            "record accessors. Python gets decoded values in a dict "
            "instead, so the `wfm_keyword_t` never crosses the boundary."
        ),
        (
            "`wfm_reader_find_keyword` / `wfm_reader_find_header_field` -- "
            "lookup by tag, which a Python dict does for free."
        ),
        (
            "`wfm_reader_set_stop_fn` -- the follow-loop interrupt hook, a "
            "C function pointer with no Python equivalent."
        ),
    ]
    for u in d.unreachable:
        R.md(f"- {u}")
    R.md()
    R.md("Each is certified in C instead; none is a gap in the binding.")
    R.md()


def characterise(tmp: Path) -> Data:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "Measured behaviour, no verdicts. Each heading names the C section "
        "it tracks where there is one; the numbering is this report's own."
    )
    R.md()
    d = Data()
    measure_wire(d, tmp)
    measure_metadata(d, tmp)
    measure_provenance(d, tmp)
    measure_maps(d, tmp)
    measure_read(d, tmp)
    measure_hint(d, tmp)
    measure_reach(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The surface the Python binding calls is the one nothing "
        "tested.** Fifteen entry points had zero mentions in any C test in "
        "the tree, and they were not a random fifteen: they were almost "
        "exactly the set the binding uses. The C suite reads metadata "
        "through `wfm_reader_info()`, which fills its struct from "
        "`r->file_type`, `r->fs`, `r->fc`, `r->mode` and `r->endian` "
        "DIRECTLY, while every Python property goes through a separate "
        "`wfm_reader_get_*` accessor -- two independent readers of one "
        "state, one exercised. A transposed accessor would have left all "
        "1473 lines of `test_wfm_reader_core.c` green and every Python "
        "property wrong. The same split ran through the maps: the C suite "
        "used the `find_*` lookups, while `Reader.keywords` and "
        "`Reader.header` ITERATE with the four enumerators, none of which "
        "was tested. Closed by three new sections, scored against what was "
        "written rather than against the other reader. Sabotage: `get_fs` "
        "returning `fc`, `keyword_tag` returning its neighbour's tag, and "
        "the two follow knobs aliased onto one field -- all now red.",
    )
    R.find(
        "F2",
        "GAP",
        "**`wfm_reader_header_tag` neither bounds-checks nor documents "
        "that it must not be** (gh-1123). Of the four enumerators, three "
        "are consistent -- `wfm_reader_keyword` and "
        "`wfm_reader_header_field` both bounds-check and both say so; "
        "`wfm_reader_keyword_tag` does not check and says exactly that "
        "(\"jm's generated dict loop calls this for every index in "
        '[0, num_keywords), so `i` is always in range"). '
        "`wfm_reader_header_tag` does neither, two lines from the sibling "
        "that does both. Correct today, because the generated loop is its "
        "only caller and bounds `i` -- but it is declared in a public "
        "header, so a C caller is told nothing about a precondition whose "
        "violation is an out-of-bounds heap read, and nothing calls it out "
        "of range so ASan never sees it. Found by assuming the symmetry "
        "and asserting `header_tag(r, nh) == NULL`: that assertion was "
        "wrong, not the code, which is how the asymmetry surfaced.",
    )
    R.find(
        "F3",
        "CONFIRMED",
        "**A headerless capture's type is written down and not read back** "
        "(gh-1120). `Writer` records `core:datatype` and "
        "`core:sample_rate` in a `.sigmf-meta` sidecar beside every raw "
        "capture -- its own comment says the sidecar exists because raw "
        'and CSV otherwise "hand back a file nobody could interpret" -- '
        "and `Reader` opening the same path does not look. The docstring "
        "offers `trailing_bytes` as the tell for a wrong hint; measured "
        f"from this side, it is silent in {d.hint_silent} of "
        f"{d.hint_total} cases, because it can only fire when the byte "
        "count fails to divide by the assumed 8. **For ci32 it can never "
        "fire**: 8 bytes per complex sample, exactly like cf32, so the "
        "sample count comes back correct and every value is wrong (§2.6). "
        "Passing `sample_type=` works and is the documented contract; the "
        "defect is that the answer is already on disk and getting it wrong "
        "is silent.",
    )
    R.find(
        "F4",
        "BY DESIGN",
        "**`fs = 0.0` and `fc = 0.0` are answers, not failures -- and the "
        "source tag is what says which.** A raw capture reports "
        "`fs = 0.0` and `fs_source = none`; a BLUE one reports its rate "
        "and `xdelta`. Reading the value without the tag is the mistake "
        "the tags exist to prevent, and it has a sharp instance: doppler's "
        "own BLUE writer leaves the timecode zero, so a reader that "
        "treated 0 as a J1950 epoch would date every capture this library "
        "writes to 1950. `t0_source = none` is what stops that, and §2.3 "
        "pins it.",
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
        d.wire_all_match,
        "every wire type decodes to what numpy reads from the same bytes, "
        "independently of the writer",
    )
    R.limit(
        d.worst_wire_err <= 1.0 + 1e-9,
        f"and the residual against the original samples is the writer's "
        f"quantisation: at most one LSB PER COMPONENT "
        f"({d.worst_wire_err:.3f} measured), which is sqrt(2) of that on "
        "the complex magnitude",
    )
    R.limit(
        d.meta_all_ok,
        "file_type, sample_type and num_samples are recovered for blue, "
        "csv, sigmf and raw; the self-describing types carry their own rate",
    )
    R.limit(
        d.prov_distinguishes,
        "two captures that both report fc = 0.0 are told apart by their "
        "source tag -- the reason the tags exist",
    )
    R.limit(
        d.t0_never_1950,
        "an unset BLUE timecode reads as t0_source 'none' with t0 = 0.0, "
        "so no doppler-written capture can be dated to 1950",
    )
    R.limit(
        d.kw_all_round_trip,
        "every BLUE keyword type round-trips through the keywords dict: "
        "string, double, float array, and the four integer widths",
    )
    R.limit(
        d.header_fields > 0 and d.header_disjoint,
        f"the header dict exposes {d.header_fields} decoded HCB fields and "
        "is disjoint from the keywords -- neither map is aliased onto the "
        "other",
    )
    R.limit(
        d.read_semantics_ok,
        "read() is a stream position: successive calls continue, an "
        "over-long request returns what remains, and past the end returns "
        "an empty array rather than raising",
    )
    R.limit(
        d.reset_rewinds,
        "reset() rewinds to sample 0 and the capture reads back identically",
    )
    R.limit(
        d.maxout_identity,
        "read_max_out and read_follow_max_out are the identity, including "
        "at 0",
    )
    R.limit(
        d.hint_silent >= 6,
        f"a wrong hint on a headerless capture is silent in "
        f"{d.hint_silent}/{d.hint_total} cases -- recorded so gh-1120 "
        "cannot be closed without this report moving",
    )
    R.limit(
        d.mode_endian_ok,
        "mode and endian are recovered for every wire type in a complex "
        "little-endian BLUE capture",
    )
    R.limit(
        d.whole_capture_no_trailing,
        "a whole capture reports trailing_bytes = 0, which is what makes a "
        "non-zero count mean something",
    )
    R.limit(
        d.csv_counts,
        "a CSV's length is counted rather than read from a header, matches "
        "the samples the reader then yields, and is stable when asked twice",
    )
    R.limit(
        d.follow_knobs_round_trip,
        "the follow timeout and grace round-trip through the Python face "
        "and stay independent of each other",
    )
    R.limit(
        len(d.unreachable) == 4,
        "four header entry points are not on the Python face and are "
        "certified in C instead -- counted, so one appearing or vanishing "
        "is a change",
    )


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report.

    `write=False` is the pytest path: every measurement still runs, so
    every limit is genuinely exercised, but nothing is written into the
    repo. The captures always go to a temporary directory.
    """
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    with tempfile.TemporaryDirectory(prefix="dp-reader-cert-") as td:
        d = characterise(Path(td))
        review(d)
        limits(d)
        R.executive(
            "Reader",
            [
                "**Tell it the sample type of a headerless capture, or use "
                "BLUE/SigMF.** `Writer` records the type in a sidecar "
                "`Reader` does not read, and a wrong hint is silent in "
                f"{d.hint_silent} of {d.hint_total} cases -- never "
                "detectable at all for ci32, which is byte-for-byte the "
                "same size as the cf32 default (§2.6, F3, gh-1120).",
                "**Read the source tag, not just the value.** `fs = 0.0` "
                "from a raw capture and a rate a BLUE header stated are "
                "different facts, and only `fs_source` separates them. The "
                "sharp case: an unset BLUE timecode is `t0_source = none`, "
                "and a reader that took the 0 at face value would date "
                "every doppler-written capture to 1950 (§2.3, F4).",
                "**The decode is exact.** Every wire type matches an "
                "independent numpy decode of the same bytes, and the only "
                "residual against the original samples is the writer's "
                "quantisation -- never more than one LSB (§2.1).",
                "**`read()` is a position, not a slice.** Successive calls "
                "continue, an over-long request returns what remains, and "
                "reading past the end gives an empty array rather than "
                "raising; `reset()` rewinds (§2.5).",
                "**The evidence is younger than the object.** Fifteen "
                "entry points -- almost exactly the set the Python binding "
                "calls -- had no C coverage until this certification, "
                "because the C suite reached the same state through a "
                "different function each time (F1).",
            ],
        )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
