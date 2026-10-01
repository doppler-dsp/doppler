#!/usr/bin/env python3
"""wfmgen — certification evidence.

wfmgen is a TOOL, not an object: no `_core.h`, no binding, and `wfmgen_core`
is app-only. So the campaign's usual SSOT — the C header's prose — does not
exist here, and the owner's substitution stands in its place: **the CLI
contract plus `docs/design/wfmgen.md`**, measured by running the built binary
against the in-process library render.

That design page turns out to be an unusually good SSOT, because it annotates
each of its nine design goals with the gate that keeps it true. §1's table is
that inventory, with one column added that the page cannot have: whether the
named gate, when RUN, actually pins the claim. A doc's self-report of its own
gating is a hypothesis.

What that turned up is §2's subject. Goal 2 — "the same scene expressed
through any of the four renders byte-identically", with the C API named
PRIMARY — was pinned at two legs of four.
"""

from __future__ import annotations

import hashlib
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from doppler.tests._repo import build_dir, exe
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_wfmgen_certify"
)

# The Field parser's corpus harness (phase 7). This report runs its
# `--check` subset -- a few ms -- and states what it found; the full corpus
# is `make validate-c`.
EXPLORE = exe(
    build_dir(__file__) / "native/validation/validate_wfm_field_explore"
)

R = Report()

# The scene geometry the harness, the CLI and Python are each given. It is
# deliberately the same one `test_byte_parity_vs_wfmgen` already sweeps, so
# this report measures the shipped comparison rather than a second matrix
# that could drift from it.
FS = "1e6"
FREQ = "1e5"
COUNT = 1024
SNR = "100"
SCENES = ["tone", "noise", "pn", "bpsk", "qpsk"]


def _md5(path: Path) -> str:
    return hashlib.md5(path.read_bytes()).hexdigest()


def _wfmgen_bin() -> str:
    """The CLI, via the ONE locator the library and five test modules use."""
    from doppler.wfm import cli as wfm_cli

    return wfm_cli._runnable()


def _render_three_ways(scene: str, tmp: Path) -> tuple[str, str, str]:
    """(C, CLI, Python) md5 of one scene. Raises rather than skipping.

    A missing binary FAILS here, per validation.md: this report's entire
    content comes from these three renders, so a skip and a pass would read
    identically in a log.
    """
    from doppler.wfm.compose import Composer, Writer

    c_out = tmp / f"c_{scene}.iq"
    subprocess.run([str(HARNESS), "--render", scene, str(c_out)], check=True)

    cli_out = tmp / f"cli_{scene}.iq"
    subprocess.run(
        [
            _wfmgen_bin(),
            "--type",
            scene,
            "--fs",
            FS,
            "--freq",
            FREQ,
            "--count",
            str(COUNT),
            "--snr",
            SNR,
            "--sample-type",
            "cf32",
            "--endian",
            "le",
            "-o",
            str(cli_out),
        ],
        check=True,
        capture_output=True,
    )

    x = Composer(
        type=scene,
        fs=float(FS),
        freq=float(FREQ),
        num_samples=COUNT,
        snr=float(SNR),
    ).compose()
    py_out = tmp / f"py_{scene}.iq"
    with Writer(
        py_out,
        fs=float(FS),
        file_type="raw",
        sample_type="cf32",
        endian="le",
    ) as w:
        w.write(x)

    return _md5(c_out), _md5(cli_out), _md5(py_out)


def _measure(tmp: Path) -> dict[str, tuple[str, str, str]]:
    if not HARNESS.is_file():
        raise SystemExit(
            f"{HARNESS} not built — run `make build`. This validator FAILS "
            "rather than skipping: its entire content comes from that binary."
        )
    return {s: _render_three_ways(s, tmp) for s in SCENES}


def _explore() -> dict[str, int]:
    """The corpus harness's `--check` counts. Raises rather than skipping."""
    if not EXPLORE.is_file():
        raise SystemExit(
            f"{EXPLORE} not built — run `make build`. This validator FAILS "
            "rather than skipping."
        )
    r = subprocess.run(
        [str(EXPLORE), "--check"], capture_output=True, text=True
    )
    m = re.search(
        r"accepted (\d+), round trips (\d+), findings (\d+)", r.stdout
    )
    if not m:
        raise SystemExit(f"unexpected harness output:\n{r.stdout}")
    return {
        "exit": r.returncode,
        "accepted": int(m[1]),
        "round_trips": int(m[2]),
        "findings": int(m[3]),
    }


# A malformed Field on the command line, and the words its refusal must
# contain. Each is a rule the grammar states; the CLI must exit 2 with a
# sentence naming it rather than build nothing and say nothing.
CLI_REFUSALS = [
    ("pn:12:1", "POLY"),  # D12: a 1-bit register has no m-sequence
    ("pn:31:5:32", "REG"),  # #1624: a SEED wider than its register
    ("01a1", "0 or 1"),  # a typo in a literal is refused, not filtered
    ("data:1024", "no bits of its own"),  # named, not a typo
    ("pn:4000000000:5", "261120"),  # #1622: past the Field bound, named
]


def _cli_refusal(spec: str, tmp: Path) -> tuple[int, str]:
    r = subprocess.run(
        [
            _wfmgen_bin(),
            "--type",
            "bits",
            "--data",
            spec,
            "-o",
            str(tmp / "refused.cf32"),
        ],
        capture_output=True,
        text=True,
    )
    return r.returncode, r.stderr.strip()


def build(write: bool = True) -> Report:
    import tempfile

    with tempfile.TemporaryDirectory() as td:
        got = _measure(Path(td))
        corpus = _explore()
        refusals = [
            (sp, w, *_cli_refusal(sp, Path(td))) for sp, w in CLI_REFUSALS
        ]

    R.md("# wfmgen — certification evidence")
    R.md()
    R.md("## 1. The tool")
    R.md()
    R.md(
        "The waveform generator: sequencing, levels, framing and I/O over "
        "signal primitives it does not re-implement. It is a TOOL rather "
        "than an object — no `_core.h`, no binding, `wfmgen_core` exports "
        "nothing out-of-line — so the campaign's usual SSOT, a header's "
        "prose, does not exist. The substitution is the **CLI contract plus "
        "the design page**, measured by running the built binary against the "
        "in-process library render."
    )
    R.md()
    R.md("Design and contract, not restated here:")
    R.md()
    R.md(
        "- [wfmgen — the waveform generator]"
        "(../../../../../docs/design/wfmgen.md) — the SSOT for every claim "
        "below\n"
        "- `native/src/app/wfmgen.c` — the CLI contract itself\n"
        "- `native/validation/wfmgen_certify.c` — the C leg measured in §2\n"
        "- `src/doppler/wfm/tests/test_compose.py` — the CLI and Python legs"
    )
    R.md()
    R.md("### Claim coverage — the design page's nine goals")
    R.md()
    R.md(
        "The page annotates each goal with the gate that keeps it true, "
        "which is most of step 1 already done. The `verified` column is what "
        "this certification adds: the result of RUNNING each named gate "
        "rather than reading the annotation. A doc's self-report of its own "
        "gating is a hypothesis, and one of the nine did not survive it."
    )
    R.md()
    R.table(
        ["#", "goal", "gate the page names", "verified"],
        [
            [
                "1",
                "Full-featured across the three axes",
                "`check_wfmgen_flag_docs.py`",
                "**runs** — 52 flags documented, 51 exercised; the one "
                "left, `-o`, is short and the gate reads long flags only "
                "(F4)",
            ],
            [
                "2",
                "Identical from four APIs, byte-for-byte",
                "drift-check, flag matrix, C example, doc fences",
                "**PARTIAL — 2 of 4 legs** before this pass; see §2 and F1",
            ],
            [
                "3",
                "Fast enough to be the inner loop",
                "`bench-coverage-check`",
                "**half** — the page says so: coverage is gated, the numbers "
                "are not",
            ],
            [
                "4",
                "A scene round-trips",
                "`test_cli_record_replays.py`",
                "**runs, passes**",
            ],
            [
                "5",
                "A ranged field records its span",
                "`test_cli_ranged.py`",
                "**runs, passes**",
            ],
            [
                "6",
                "Every draw is readable back",
                "`test_compose.py`",
                "**runs, passes**",
            ],
            [
                "7",
                "One engine, one pull path",
                "`test_wfm_compose.c`",
                "**runs, passes** (157/157)",
            ],
            [
                "8",
                "Adding a knob cannot fork an API",
                "drift-check, enum tables, flag docs/matrix",
                "**runs** — and since #853 item 9 it holds for every face: "
                "the CLI table, `--help`, the JSON reader and the schema are "
                "generated from one surface table (F9)",
            ],
            [
                "9",
                "0 dBFS is unit average power",
                "`TestQuantization`",
                "**runs, passes**",
            ],
        ],
    )
    R.md()

    R.md("### Claim coverage — the Field, the grammar every face reads")
    R.md()
    R.md(
        "#853 put every frame field on ONE text form, read by one parser. "
        "That part of the tool DOES have a header, so it is certified the "
        "campaign's usual way: every prose claim in `wfm_frame.h` "
        "(`dp_wfm_field_parse`/`_format`/`_bits`/`_render`, "
        "`dp_wfm_frame_fixed`), `wfm_compose.h` (`dp_wfm_frame_from_json`, "
        "`dp_wfm_frame_free`) and `cvt_core.h` (`dp_bytes_to_bin`) was "
        "enumerated and mapped onto the C test that pins it; every claim "
        "that was absent or held only at one vector got a C test, and each "
        "new test was sabotaged and seen to go red."
    )
    R.md()
    R.table(
        ["surface", "claims", "before: pinned / literals / absent", "now"],
        [
            [
                "`dp_wfm_field_*`",
                "70",
                "52 / 11 / 7",
                "70 pinned (round trip and canonical form by the corpus, "
                "§2.3; literal storage by LSan under `make test-asan`)",
            ],
            [
                "`dp_wfm_frame_fixed`",
                "15",
                "7 / 3 / 5",
                "15 pinned; *every face reaches it* holds three ways: the "
                "`Frame` face in C, the CLI's wire bits against `Frame`'s "
                "(`test_frame_source.py`), and the scene against the CLI "
                "by the record replays",
            ],
            [
                "`dp_wfm_frame_from_json`, `_free`",
                "15",
                "0 / 8 / 7",
                "14 pinned; *a malformed description is refused* at the "
                "cases written",
            ],
            ["`dp_bytes_to_bin`", "10", "7 / 3 / 0", "10 pinned, every octet"],
        ],
    )
    R.md()
    R.md(
        "The inventory found three defects, all fixed here (F6-F8). The "
        "sabotages -- a SEED or tap above REG allowed, `why` copied per "
        "call, `*1` written back, the NUL left out of the fits check, "
        "`from_json(NULL)` with no reason, LITTLE ignored, a refusal "
        "writing one byte -- each took exactly its check red; `reps == 0` "
        "read as zero was caught first by the geometry tests that rely on "
        "it."
    )
    R.md()
    R.md("## 2. Characterisation")
    R.md()
    R.md("### 2.1 The four APIs, rendered and compared")
    R.md()
    R.md(
        "One scene geometry — `--fs 1e6 --freq 1e5 --count 1024 --snr 100`, "
        "written as raw little-endian `cf32` — expressed three ways and "
        "hashed. The C leg builds a `wfm_source_t` STRUCT and calls "
        "`dp_wfm_compose_create`; the CLI leg parses flags; the Python leg "
        "goes "
        "through `Composer` and `Writer`."
    )
    R.md()
    R.md(
        "The C leg goes through structs and **not** "
        "`dp_wfm_compose_from_json` "
        "on purpose. Routing it through JSON would put all three legs behind "
        "one parser, and a consistency test is structurally blind to any "
        "defect its paths share — the failure mode `validation.md` step 2 "
        "warns about, and the one that let resamp's control accumulator run "
        "the wrong recurrence for an entire release."
    )
    R.md()
    R.table(
        ["scene", "C (struct API)", "CLI", "Python", "agree"],
        [
            [
                s,
                f"`{c[:12]}…`",
                f"`{b[:12]}…`",
                f"`{p[:12]}…`",
                "**yes**" if c == b == p else "**NO**",
            ]
            for s, (c, b, p) in got.items()
        ],
    )
    R.md()
    R.md(
        "The fourth API, the JSON scene, is pinned against the CLI by "
        "`test_cli_record_replays.py` (`--record` → `--from-file`) rather "
        "than re-measured here."
    )
    R.md()
    R.md("### 2.2 What the matrix does not distinguish")
    R.md()
    n_distinct = len({c for c, _, _ in got.values()})
    R.md(
        f"The five scenes produce **{n_distinct} distinct waveforms**, not "
        "five: `pn` and `bpsk` hash identically at these defaults, because a "
        "BPSK source driven by the default PN pattern at one sample per "
        "symbol IS the pn waveform. That is correct behaviour and worth "
        "stating, because the parametrisation reads as five independent "
        "cases and the coverage is narrower than it looks."
    )
    R.md()
    R.md("### 2.3 The Field corpus")
    R.md()
    R.md(
        "`native/validation/wfm_field_explore.c` (phase 7, recorded in "
        "frame-description-measurements.md §F.6) drives generated, mutated "
        "and hand-edge text through the one reader, the one writer and the "
        "one door to bits, holding for every accepted text: "
        "parse → format → parse is the same field, the text is canonical, "
        "and both texts render the same bits, each 0 or 1. It asserts "
        "through `dp_test.h`, including that it checked something -- most "
        "generated text reached the grammar -- so a generator that "
        "produced nothing acceptable fails rather than finding nothing. "
        "This report "
        "runs its `--check` subset; the full corpus (200 000 generated + "
        "200 000 mutations) is `make validate-c`."
    )
    R.md()
    R.table(
        ["run", "accepted", "round trips held", "findings"],
        [
            [
                "`--check`, seed `0x9E3779B97F4A7C15`",
                str(corpus["accepted"]),
                str(corpus["round_trips"]),
                str(corpus["findings"]),
            ]
        ],
    )
    R.md()
    R.md("### 2.4 A malformed Field on the command line")
    R.md()
    R.table(
        ["`--bits`", "exit", "stderr"],
        [[f"`{sp}`", str(rc), f"`{err}`"] for sp, _, rc, err in refusals],
    )
    R.md()

    R.md("## 3. Review")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "**The C leg of goal 2 was pinned by nothing.** The design page "
        "calls the C API primary and promises all four render identically; "
        "Python↔CLI was pinned (`test_byte_parity_vs_wfmgen`) and JSON↔CLI "
        "was pinned (`test_cli_record_replays.py`), and the C leg was not. "
        "What read as coverage was `native/examples/wfmgen_demo.c` §5 — "
        "*'the same declaration composes byte-identically'* — which composes "
        "one scene TWICE IN ONE PROCESS and `memcmp`s the buffers. That is "
        "determinism, a real property and not this one: it would pass "
        "unchanged if the C API and the CLI had diverged completely. Fixed "
        "by `wfmgen_certify.c` plus `test_c_api_byte_parity_vs_wfmgen`, and "
        "proven by two DISCRIMINATING sabotages — a carrier off by 1e-5 "
        "relative takes 4 of 5 red and correctly leaves `noise` green; a "
        "wrong PN default takes exactly `pn`/`bpsk`/`qpsk` red.",
    )
    R.find(
        "F2",
        "FIXED",
        "**The defaults goal 2 depends on are declared once and restated "
        "twice, two of them as enum indices** (#1142). `just-makeit.toml` "
        "declares all eleven with `default =`; `wfmgen.c` repeats them as a "
        "struct literal inside `main()`; a C caller of "
        "`dp_wfm_compose_create` "
        "gets a zero-initialised struct and neither. Measured: every value "
        "agrees today, and NO gate holds them there — both scripts that "
        "parse `wfmgen.c` read its flag table, not this literal. Worse, "
        "`modulation` and `crc` are restated as indices (`1`, `1`) while the "
        "manifest names them as strings, so the literal silently depends on "
        "the ORDER of `[[enum]] bitmod` and `[[enum]] crc`: prepend an entry "
        "— append-only protects the tail, not the head — and the manifest "
        "still means `bpsk` while `1` becomes the new second entry. This is "
        "the rot `wfm_names.h` was created to end (doppler#760), one level "
        "up, in a literal no enum gate scans. FIXED by generation: "
        "`scripts/gen_wfm_defaults.py` renders the manifest's declaration "
        "into `wfm_defaults.h`, resolving an enum-valued default through "
        "the field's OWN declared `enum` key rather than by searching for "
        'a matching string -- `"bpsk"` sits in two enums, and a '
        "search-by-value picks the wrong one. Sabotage-proven the way the "
        "issue describes: PREPEND an entry to `[[enum]] bitmod` and the "
        "generated header follows the NAME (`.modulation = 1` becomes `2`) "
        "while the gate goes red, where the hand-written `1` would have "
        "stayed `1` and silently meant the new second entry.",
    )
    R.find(
        "F3",
        "FIXED",
        "**The only evidence for goal 2 could report green having run "
        "nothing.** `test_compose.py` carried a sixth, private copy of the "
        "wfmgen locator that globbed `build*/**/wfmgen` and returned `None` "
        "into a `skipif`, while `cli._runnable()` — used by five other test "
        "modules — raises with the path. A missing binary therefore turned "
        "the byte-parity claim into a SKIP, which reads identically to a "
        "pass. Replaced with the shared locator, resolved lazily so only the "
        "CLI tests fail rather than the whole module erroring at collection. "
        "Proven by hiding the binary: exactly the 18 CLI tests go red with "
        "`FileNotFoundError` naming the path.",
    )
    R.find(
        "F4",
        "FIXED",
        "**Fourteen of the 67 flags were documented and exercised by "
        "nothing.** The flag-docs gate printed two numbers and only the "
        "first was ever quoted: 67 documented, and *53 of 67 exercised*. "
        "#1143 made the count a RATCHET checked both ways, and #1149 paid it "
        "down: every real capability got a runnable line on the page "
        "already explaining it. Today, after #853 retired the frame sugar "
        "flags: **52 documented, 51 exercised**. The one left is `-o`, "
        "which IS used under docs/ -- the gate tokenises long flags only, "
        "so a short one is unexercised by construction. That is a fact "
        "about the checker, recorded in its ratchet file, not a gap in the "
        "docs.",
    )
    R.find(
        "F5",
        "FIXED",
        "**The limits gate does not discover by glob, though the process "
        "page twice says it does** (#1144). `make validate-check` globs; the "
        "tree-wide `test_validation_limits.py` asserts only the names typed "
        "into a hand-written `OBJECTS` dict. So a new validator renders its "
        "report, passes the staleness gate AND the report-format gate, and "
        "has every one of its limits asserted by nobody. This report sat in "
        "exactly that state for one commit: 11 limits measured, "
        "`validate-check` reporting 33 reports up to date, and "
        "`-k 'validation_limits and wfmgen'` collecting zero tests. Fixed "
        "by discovery: `OBJECTS` is now globbed from the folders beside the "
        "gate, so the page's promise is true of both gates rather than one, "
        "and `_discover()` fails loudly on an empty match because a glob "
        "that silently finds nothing is the same defect one level up. "
        "Proven by dropping a throwaway validator in place and watching the "
        "gate collect it with no registration -- the same selection returned "
        "zero tests before. The campaign's own recurring shape, found in "
        "the campaign's own infrastructure.",
    )
    R.find(
        "F6",
        "FIXED",
        "**A SEED, POLY or Gold tap wider than REG was silently masked** "
        "(#1624, found by this inventory). `wfm_frame.h` says REG is the "
        "register width; the generators mask every number to it, so "
        "`pn:31:5:32` -- whose low five bits are zero -- rendered 31 zeros, "
        "the all-zero register that `0 selects 1` exists to avoid, reached "
        "by a spelling that default does not catch. `pn:10:5:0:0x40` was a "
        "register with no feedback. A constant field still looks like a "
        "field: the #1602 class, one spelling further out. Refused at parse "
        "now, naming REG; five rows in the refusal table were red first, "
        "and the corpus generator reduces each drawn seed into its "
        "register so its draws stay the recorded ones.",
    )
    R.find(
        "F7",
        "FIXED",
        "**`dp_wfm_frame_from_json(NULL, &why)` refused with no reason.** "
        "The header promises a static reason on every failure; the NULL "
        "path returned before setting one. The entry point `wfmgen --frame "
        "FILE` uses had no direct C test at all -- the scene tests reached "
        "only the reader it shares -- so seven of its fifteen claims were "
        "held by nobody. Now pinned directly, the NULL reason red first.",
    )
    R.find(
        "F8",
        "FIXED",
        "**A header claim described a case that cannot happen.** "
        "`dp_wfm_field_bits` said a sizing call checks the grammar only, so "
        "'a Gold pair that is not a preferred pair' sizes and then fails to "
        "render. `dp_gold_create` checks no such thing, and after D12 and "
        "F6 every text the grammar accepts renders -- the branch that "
        "reported the failure was dead code with a sentence attached. The "
        "claim is now the true one, *every accepted text renders*, pinned "
        "over every accepted table row in C and every accepted text of the "
        "corpus (§2.3); the dead branch is gone.",
    )
    R.find(
        "F9",
        "FIXED",
        "**The design page said goal 8 held for two faces.** #853 item 9 "
        "generated the CLI option table, `--help`, the JSON reader, the "
        "schema and the options reference from one surface table, and the "
        "page still read *today this holds for Python and the C defaults "
        "only*, with `wfm_surface.h` listed as replacing *nothing yet*. A "
        "design page that under-states its own gating is the mirror of one "
        "that over-states it, and §1 reads the page first. Corrected, with "
        "the gates named: `gen_wfm_defaults.py --check` in `make lint` and "
        "`test_wfm_surface_roundtrip.py`.",
    )
    R.md()

    R.md("## 4. Limits")
    R.md()
    R.md("Claims a caller may rely on.")
    R.md()

    for s, (c, b, p) in got.items():
        R.limit(
            c == b,
            f"`{s}`: the C struct API and the CLI render byte-identically",
        )
        R.limit(
            c == p,
            f"`{s}`: the C struct API and Python render byte-identically",
        )
    R.limit(
        corpus["findings"] == 0 and corpus["exit"] == 0,
        "the Field corpus (`--check`): every accepted text round-trips, "
        "formats canonically and renders the same bits as its canonical "
        "form",
    )
    R.limit(
        corpus["round_trips"] == corpus["accepted"] > 0,
        "the Field corpus holds its properties for EVERY text it accepts, "
        "not a subset",
    )
    for sp, want, rc, err in refusals:
        R.limit(
            rc == 2 and want in err,
            f"`wfmgen --data {sp}` exits 2 with a sentence naming `{want}`",
        )
    R.limit(
        HARNESS.is_file(),
        "the C measurement harness is built (a missing one FAILS, never "
        "skips)",
    )

    R.executive(
        "wfmgen",
        source=(
            "Generated by `validate.py` in this folder. wfmgen is a tool "
            "with no binding, so the C leg is measured by "
            "`native/validation/wfmgen_certify.c` and the other two by "
            "running the shipped CLI and the Python composer; nothing is "
            "modelled. Re-run to regenerate."
        ),
        takeaways=[
            "**The four-API byte-identity promise holds, and now three of "
            "its legs are measured rather than asserted** (§2.1). It was "
            "pinned at two of four; the C leg — the API the design page "
            "calls PRIMARY — was covered by a test that composes one scene "
            "twice in one process, which is determinism, not agreement (F1).",
            "**A default now has one home, and an enum-valued one follows its "
            "NAME** (F2, #1142, now fixed). The values goal 2 depends on were "
            "declared in `just-makeit.toml`, restated by hand in `wfmgen.c`, "
            "and absent from a C caller's zero-initialised struct, with no "
            "gate comparing them — two of them as enum INDICES, so prepending "
            "to `[[enum]] bitmod` would have changed a default with nothing "
            "failing. They are generated from the manifest now, and the "
            "byte-parity limits in §2.1 are what proved the change altered "
            "no waveform.",
            "**A skip and a pass read the same in a log** (F3). The single "
            "test carrying goal 2's evidence could skip itself, via a "
            "private copy of a locator that already existed and fails "
            "loudly. Prefer the shared one; a private copy is not a "
            "harmless duplicate when it changes the failure mode.",
            "**A validator used to be gated only if someone remembered to "
            "register it** (F5, #1144, now fixed). `validation.md` promised "
            "both gates discover by glob and only the staleness one did, so "
            "this report rendered, passed two gates, and had its limits "
            "asserted by nobody for one commit. Both glob now — but the "
            "habit is still worth keeping: check that "
            "`-k 'validation_limits and <obj>'` collects something before "
            "believing a green run.",
            "**The Field grammar is certified header-first** (§1): 110 "
            "prose claims across the parser, the fixed frame, the JSON "
            "frame reader and `bytes_to_bin`, 19 of them held by nobody "
            "before this pass and 109 pinned now. The inventory found a "
            "silent-mask defect in the grammar (F6, #1624), a refusal with "
            "no reason (F7) and a header sentence about a case that cannot "
            "happen (F8).",
            "**The phase-7 corpus is a committed harness now** (§2.3), "
            "and this report runs its subset rather than quoting a scratch "
            "run. On the parser it was recorded against, the recorded seed "
            "reproduces the record's 259 037 round trips exactly; since "
            "F6 each drawn seed is reduced into its register, so the texts "
            "differ where a seed was out of range and the draws do not.",
            "**The five-scene matrix is four distinct waveforms** (§2.2): "
            "`pn` and `bpsk` are byte-identical at these defaults, so the "
            "parametrisation reads broader than the coverage is.",
        ],
    )
    R.summary()
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
