#!/usr/bin/env python3
"""Gate: stimulus and its measurement have ONE home, and it is the library.

doppler is a C library with a Python convenience layer, and it ships a
full-featured waveform generator (`wfm_synth_*` in C, `Synth`/`Composer` in
Python) plus a measurement stack (`ber_evm_db`, `ber_settle_syms`,
`ber_settle_from`). A test, validation harness or example that builds its own
pulse, sets its own level, or computes its own EVM is not just duplicating
code -- it is inventing a CONVENTION, and the convention is the part that goes
wrong silently.

Measured cost, 2026-08-10: `ratesync_demo.py` shaped its own RRC and then
scaled the result by `0.25 / max(|x|)` -- a PEAK normalisation. An RRC stream
peaks at ~1.582x its symbol amplitude, so symbols reached RateSync at ~0.158
against a contract written in unit symbol amplitude. A Gardner detector's
slope goes as A^2, so that is ~40x of loop gain, and the demo failed its own
lock assertion with nothing pointing at the level. wfmgen already defines unit
transmit power and an Es/N0 mode; the hand-rolled generator is what made the
level expressible-but-unstated.

Five signatures under four markers, each with a canonical primitive that
already exists:

  pulse      a private raised-cosine / RRC  ->  dp_wfm_synth_set_rrc, rrc_taps
  level      normalising a generated stream by its PEAK  ->  Synth(level=,
             snr=, snr_mode=), dp_wfm_snr_over_fs, dp_wfm_source_create_snr
  level      (C only) a hand per-component `/ sqrt(2)` on the unit-power
             `dp_cgauss`  ->  doppler's awgn at dp_awgn_amplitude_for_snr,
             or the composer's snr/snr_mode
  offset     a loop residual as a bare cycles/sample number  ->
             freq_offset_inside_bw / clock_offset_inside_bw in Python,
             dp_test_freq_offset_inside_bw in C (native/tests/dp_sym_test.h)
  evm        a private error-vector magnitude  ->  ber_evm_db, over a window
             from ber_settle_syms / ber_settle_from

Second measured cost, 2026-08-17 (doppler#843): `MpskReceiver`'s tests seeded
the carrier loop either ON the answer (`init_norm_freq` == the stimulus's own
offset, so the loop never left its initial state) or PAST its pull-in cliff.
A receiver whose carrier discriminator was wired to nothing passed six of
them. The bound is `bn_carrier / m` cycles per symbol -- the `m` because the
discriminator is an M-th power -- and nothing in the tree said the `m` out
loud, so the same literal was a 4x harder question at 8PSK than at BPSK.

This scans only the TEST, VALIDATION and EXAMPLE layers. The library itself is
the sanctioned home -- `wfm_synth_core.c` implementing an RRC is the point.

Third measured cost (doppler#689): `dp_cgauss` already has `E|z|^2 = 1`,
0.5 per component (dp_rng_test.h says so in as many words). Nine DSSS
noise sites scaled it by `sigma / sqrt(2.0)` as if it were unit variance
PER component, so every capture carried 3.01 dB less noise than its
`cn0_dbhz` label. The second `sqrt(2)` is the defect, and its spelling is
narrow enough to gate: the per-component `/ sqrt(2)` or `M_SQRT1_2`
multiplying a `dp_cgauss` draw on the same statement. A literal-scaled
draw, or `M_SQRT1_2` mixing two values with no draw in sight, is not it.

Deliberately NOT gated here, and the omission is the design: hand-rolled
Gaussian noise (`standard_normal`, Box-Muller) hits 72 files, most of them
legitimately -- assembling an expected array, or inert plumbing where the
sample values are irrelevant. A ratchet that large is noise, and a noisy gate
gets switched off. The level marker above already catches the case that
actually bites, which is an invented amplitude convention.

One more rule, file-level and with NO allowlist -- `once`. A synth sends a
bit pattern (or a dsss chip burst, or a payload) ONCE and then emits
silence (doppler#1718). A C harness that hands one a finite pattern must
also name `dp_wfm_synth_data_ended`, i.e. assert the pattern covered the
run. Measured cost, 2026-10-01: removing the cycle left a bench timing the
silence, a validation record reporting `bits` at 0.6% power, and three acq
validators scoring a silent tail, while two others went red with nothing
pointing at the source -- `MpskReceiver: refused every point`. Tile a
periodic stimulus with `dp_tx_bits_for_run` (native/tests/dp_tx_test.h).
Python cannot reach `data_ended`; its consumers check the power they claim.

**The allowlist is a RATCHET. It may only shrink.** A new occurrence fails the
gate. Removing one means deleting its line, which the failure message shows.

Usage:  python3 scripts/check_stimulus_sources.py [--root DIR]
Exit 0 when the set of occurrences matches the allowlist exactly.
"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from collections import Counter
from pathlib import Path
from typing import NamedTuple

ROOT = Path(__file__).resolve().parent.parent
#: The ratchet, relative to the root the gate scans.
ALLOW = "scripts/.stimulus-sources-allow"

#: Only the layers that CONSUME the library. `native/src` and `native/inc`
#: are where the canonical implementations live.
SCAN_DIRS = ("native/tests", "native/validation", "src/doppler")

SUFFIXES = (".c", ".h", ".py")

#: The C layers that consume the library: the scope of the `once` rule and
#: of the C noise-level signature. Benchmarks and examples render stimulus
#: too, so they are in it, where the Python-shaped markers never looked.
C_DIRS = (
    "native/tests",
    "native/validation",
    "native/benchmarks",
    "native/examples",
)

#: marker -> (pattern, what to call instead)
MARKERS: dict[str, tuple[re.Pattern[str], str]] = {
    "pulse": (
        re.compile(
            r"def\s+_?rrc\w*\s*\(|def\s+_?rc\s*\(|"
            r"static\s+(inline\s+)?double\s+_?(rrc|rc)\w*\s*\("
        ),
        "generate with wfmgen -- dp_wfm_synth_set_rrc() in C, or "
        "Synth(pulse='rrc', rrc_beta=..., rrc_span=...) in Python. "
        "dp_wfm_rrc_taps()/rrc_taps() gives the taps if you need them raw.",
    ),
    # A NUMERIC backoff against the peak, e.g. `x *= 0.25 / max(|x|)`.
    #
    # Scaling to a FRACTION of the peak invents a level: the symbol amplitude
    # that results depends on the pulse's PAPR and on beta, so the object
    # receives something nobody stated. Normalising to UNIT peak
    # (`y / max(|y|)`, no constant) is deliberately NOT matched -- that is the
    # display/comparison convention used to overlay an S-curve or a frequency
    # response, and it is not a stimulus level at all. That distinction is
    # what keeps this marker sharp enough to be worth gating: broadened to
    # every peak normalisation it fires on 5 more files, all of them
    # legitimate, and a noisy gate gets switched off.
    "level": (
        re.compile(
            r"[*/]=\s*[\d.]+\s*/\s*np\.max\s*\(\s*np\.abs|"
            r"[*/]\s*[\d.]+\s*/\s*np\.max\s*\(\s*np\.abs|"
            r"[*/]=\s*[\d.]+\s*/\s*\w*max\s*\(\s*\w*abs"
        ),
        "do not normalise a generated stream by its PEAK -- the symbol "
        "amplitude then depends on the pulse's PAPR and on beta, so the "
        "object receives a level nobody stated. wfmgen owns this: Synth's "
        "`level`, `snr` and `snr_mode` (auto/fs/ebno/esno), with sqrt(sps) "
        "scaling for unit transmit power.",
    ),
    # An offset seeded into a tracking loop without saying what it is a
    # fraction OF.
    #
    # `bn_carrier` and `bn_timing` are normalised to the SYMBOL rate, and the
    # carrier discriminator is an M-th power, so the carrier loop's
    # acquisition bound is `bn_carrier / m` cycles per symbol. A bare
    # `foff = 0.0008` therefore says nothing checkable: the same number is a
    # different question at every `m` and every `sps`. Both ends of the range
    # read as a passing test -- seeded on truth the loop never leaves its
    # initial state, seeded past the bound the result is which way the
    # transient happened to push the integrator. doppler#843 found both, in
    # the same object, on the same day.
    #
    # Matched where the value is a NUMBER, and only on the quantity the LOOP
    # is handed. Those two restrictions are what keep it sharp, and both were
    # arrived at by running the alternatives:
    #
    #   - The RESIDUAL is the thing that has to be bounded, not the stimulus's
    #     absolute carrier error. A real link's offset is whatever the Doppler
    #     is; what the loop must acquire is `foff - init_norm_freq`. So the
    #     harness's `freq_offset=` / `clock_offset=` kwargs and the C rx
    #     configs' `.foff` / `->foff` FIELDS are matched (in all three the
    #     receiver is seeded at the centre, so the field IS the residual),
    #     while a local `foff = 0.0015` naming the stimulus is not. Gating the
    #     stimulus fired on 20 correct lines, which is how that came out.
    #   - A hand-rolled `0.5 * bn / sps` is the same defect as `0.0008` -- it
    #     was the shape that reached the C BER certification -- so the match
    #     is on a leading numeric literal, not on a bare one. Anything routed
    #     through `freq_offset_inside_bw` / `dp_test_freq_offset_inside_bw`
    #     names `u` and passes: there is nothing to allowlist for correct code.
    "offset": (
        re.compile(
            r"(?:(?:\.|->)\s*foff|\bfreq_offset|\bclock_offset)\s*=\s*"
            r"[-+]?(?:\d+\.?\d*|\.\d+)"
        ),
        "state the offset in units of the loop's own acquisition bound -- "
        "freq_offset_inside_bw(bn, m, frac) in Python, "
        "dp_test_freq_offset_inside_bw(bn, m, frac) in C (dp_sym_test.h), "
        "and clock_offset_inside_bw(bn, frac) for the timing loop. Tests are "
        "held to at or under the bound; see doppler#843 for the cliffs.",
    ),
    "evm": (
        re.compile(
            # `\w*` before `evm`, not just an optional `_`: a one-word
            # prefix defeated this. `self_evm_db()` sat in
            # async_dsss_receiver_spec_demo.py -- a line-for-line twin of
            # ber_evm_db, returning the identical -10.1 dB -- while this
            # marker reported ZERO occurrences. A gate that reads as
            # coverage and is not is the failure this file exists to name.
            r"def\s+\w*evm(_db)?\s*\(|"
            r"static\s+(inline\s+)?double\s+\w*evm(_db)?\w*\s*\("
        ),
        "measure with ber_evm_db() -- doppler.ber.ber_evm_db in Python, "
        "dp_sym_test.h's dp_test_evm_db_hard_range() in C -- over a window "
        "from ber_settle_syms()/ber_settle_from(), never a fixed fraction "
        "of the record.",
    ),
}


#: marker -> the primitives that ARE the sanctioned home. A function whose
#: body reaches one of these is a wrapper around the library, not a second
#: copy of it, however its name reads.
CANONICAL: dict[str, set[str]] = {
    "pulse": {
        "rrc_taps",
        # The Python face of the analytic pulses. Omitting these was a real
        # hole for one commit: the gate told callers to use the library and
        # then reported the ones that did.
        "rrc_h",
        "rc_h",
        "wfm_rrc_h",
        "wfm_rc_h",
        "dp_wfm_rrc_taps",
        "dp_wfm_rrc_polyphase_bank",
        "dp_wfm_synth_set_rrc",
        # The generator itself, and the module-level factories that return
        # one. `qpsk(pulse="rrc", ...)` IS wfmgen; a harness calling it is
        # doing exactly what this gate asks for. Names like `noise`, `pn`
        # and `bits` are ordinary words, which is why a bare identifier is
        # not enough -- see `_doppler_imports` below.
        "Synth",
        "Composer",
        "Segment",
        "Timeline",
        "tone",
        "noise",
        "pn",
        "bpsk",
        "qpsk",
        "chirp",
        "bits",
    },
    "evm": {
        "ber_evm_db",
        "dp_test_evm_db_hard",
        "dp_test_evm_db_hard_range",
    },
}


class Signature(NamedTuple):
    """One pattern the gate reports, under a marker, over a scope.

    A marker names the CONVENTION being invented (`level`, `pulse`, ...)
    and keys the allowlist; a signature is one spelling of it. `level` has
    two, because a level is invented in two ways that share nothing but the
    name: normalising a stream by its peak (Python), and scaling a
    unit-power noise draw as if it were unit variance per component (C).
    Each carries its own `instead`, so the refusal names the primitive for
    the spelling it caught.
    """

    marker: str
    pattern: re.Pattern[str]
    instead: str
    dirs: tuple[str, ...] = SCAN_DIRS
    suffixes: tuple[str, ...] = SUFFIXES
    #: Match whole C statements (`c_statements()`) instead of lines.
    statements: bool = False


#: The noise LEVEL invented by hand (doppler#689). `dp_cgauss` is unit
#: power, `E|z|^2 = 1` with 0.5 per component; `sigma / sqrt(2.0) *
#: dp_cgauss(...)` treats it as unit variance per component and so delivers
#: half the noise power the `sigma` beside it states -- 3.01 dB, on nine
#: sites, every one of them labelled with a C/N0 it did not have.
#:
#: Read per STATEMENT, not per line (`c_statements()`): the repo's GNU
#: clang-format breaks a long product BEFORE its `*`, so a line-wise match
#: never saw a wrapped site, and the comment skip dropped its continuation
#: as a `*` prose line. A statement fires when it draws `dp_cgauss(` AND
#: carries a 1/sqrt(2) in any spelling and any order: a division by
#: sqrt(2) (`sqrt`/`sqrtf`, `2`, `2.0`, `2.0f`, `M_SQRT2`, `1.414...`) or a
#: factor of sqrt(1/2) (`M_SQRT1_2`, `sqrt(0.5)`, `0.707...`,
#: `M_SQRT2 / 2`), with any float suffix and any casts. The factor and the
#: draw must share one PRODUCT term (`top_terms()`): a scaled QPSK symbol
#: PLUS correctly-scaled noise is not the defect.
#:
#: What it does NOT see, by design: a sigma divided on an EARLIER statement
#: (`s = sigma / sqrt(2); x = s * dp_cgauss(&st);`) -- that needs data flow,
#: and the fix it would demand is the same call either way. And a
#: multiplication by sqrt(2) (`M_SQRT2 * ... dp_cgauss`) is a different,
#: louder scaling with legitimate uses (test_detection_core.c builds a
#: real-valued Gaussian that way); it does not fire.
#: Any run of C casts: `(double)`, `(unsigned long)`, `((float))`.
_CAST = r"(?:\(+\s*[A-Za-z_][\w ]*\)+\s*)*"
#: A floating literal suffix.
_SFX = r"[fFlL]?"
_TWO = rf"2(?:\.0*)?{_SFX}"
_ROOT2 = (
    rf"(?:sqrt[fl]?\s*\(\s*{_CAST}{_TWO}\s*\)|\bM_SQRT2\b"
    rf"|\b1\.4142\d*{_SFX})"
)
_ROOT_HALF = (
    rf"(?:\bM_SQRT1_2\b|sqrt[fl]?\s*\(\s*{_CAST}0?\.50*{_SFX}\s*\)"
    rf"|\b0\.707\d*{_SFX}|\bM_SQRT2\s*/\s*{_TWO}(?![\w.]))"
)
NOISE_LEVEL = re.compile(
    rf"(?=.*\bdp_cgauss\s*\()(?=.*(?:/\s*{_CAST}{_ROOT2}|{_ROOT_HALF}))"
)


#: A type name as a cast spells it: C's arithmetic keywords and qualifiers,
#: or a `_t` typedef, with any pointer stars.
_CTYPE = (
    r"(?:(?:const|volatile|unsigned|signed|short|long|int|char|float"
    r"|double|_Complex|complex|_Bool|bool|\w+_t)\s*\**\s*)+"
)


def top_terms(stmt: str) -> list[str]:
    """Split a statement at `+`, `-` and `=` outside every bracket.

    The 1/sqrt(2) and the draw must share one PRODUCT term to be the
    defect: `M_SQRT1_2 * (b0 + I * b1) + sigma * dp_cgauss (&st)` is a
    unit-power QPSK symbol plus correctly-scaled noise, not a hand level.

    Only a BINARY `+`/`-` splits: one that follows an operand (an
    identifier, a number, `)` or `]`). A unary sign (`* -dp_cgauss`), an
    exponent's sign (`1e-3`), `->`, `++` and `--` stay inside the term,
    and so does a sign after a CAST (`(float _Complex) -dp_cgauss`): a
    `)` closing a group that is only a type name is not an operand. A hex
    float's exponent (`0x1p-3`) is not recognised and would split; no such
    literal is in the tree. Nor is a cast to a typedef without `_t`
    (`(cplx) -dp_cgauss`): telling it from a parenthesised value needs type
    information.
    """
    terms: list[str] = []
    cur: list[str] = []
    depth = 0
    for k, c in enumerate(stmt):
        depth += (c in "([") - (c in ")]")
        prev = "".join(cur).rstrip()
        after_operand = bool(prev) and (
            prev[-1].isalnum() or prev[-1] in "_)]"
        )
        exponent = re.search(r"(?<![\w.])(?:\d+\.?\d*|\.\d+)[eE]$", prev)
        cast = re.search(rf"\(\s*{_CTYPE}\s*\)$", prev)
        binary = after_operand and not exponent and not cast
        # `->`, and the first half of `++`/`--` (the second follows no
        # operand, so is never binary).
        sign = c in "+-" and binary and stmt[k + 1 : k + 2] not in (">", c)
        if depth == 0 and (sign or c == "="):
            terms.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    terms.append("".join(cur))
    return terms


def c_statements(text: str) -> list[tuple[int, str]]:
    """`(first line, whitespace-collapsed text)` of each C statement.

    Comments are blanked (newlines kept, so line numbers survive), a
    preprocessor directive with its `\\` continuations is one statement,
    and everything else splits at `;`, `{` and `}`. A wrapped expression is
    therefore read whole, which a line-wise scan cannot do.
    """

    def blank(m: re.Match[str]) -> str:
        return re.sub(r"[^\n]", " ", m.group(0))

    def scrub(m: re.Match[str]) -> str:
        """A literal empties (`"/*"` must not open a comment, nor hide the
        code after it); a comment blanks to spaces."""
        t = m.group(0)
        return t[0] * 2 if t[0] in "\"'" else blank(m)

    text = re.sub(
        r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|/\*.*?\*/|//[^\n]*',
        scrub,
        text,
        flags=re.S,
    )
    out: list[tuple[int, str]] = []
    buf: list[str] = []
    start = 0

    def flush() -> None:
        joined = " ".join("".join(buf).split())
        # A loop body's statement opens with its header's tail (`i++) x[i]
        # = ...`, after the header's own `;`s); drop through the unmatched
        # `)` so the key is the statement itself.
        depth = 0
        for k, c in enumerate(joined):
            depth += (c == "(") - (c == ")")
            if depth < 0:
                joined = joined[k + 1 :].lstrip()
                depth = 0
        if joined:
            out.append((start, joined))
        buf.clear()

    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.lstrip().startswith("#"):
            flush()
            start, directive = i + 1, [line]
            while directive[-1].rstrip().endswith("\\") and i + 1 < len(lines):
                i += 1
                directive.append(lines[i])
            buf.append(" ".join(d.rstrip().rstrip("\\") for d in directive))
            flush()
            i += 1
            continue
        for ch in line + "\n":
            if ch in ";{}":
                flush()
                continue
            if not buf and ch.isspace():
                continue  # a statement starts at its first real character
            if not buf:
                start = i + 1
            buf.append(ch)
        i += 1
    flush()
    return out


#: Every signature the gate scans for: the four markers above over the
#: consuming layers, plus the C noise level over the C layers.
SIGNATURES: tuple[Signature, ...] = (
    *(Signature(m, rx, why) for m, (rx, why) in MARKERS.items()),
    Signature(
        "level",
        NOISE_LEVEL,
        "do not scale dp_cgauss by hand -- it is already unit power "
        "(E|z|^2 = 1, 0.5 per component), so a per-component / sqrt(2) "
        "halves the noise the sigma beside it states (doppler#689). Call "
        "doppler's awgn at the level dp_awgn_amplitude_for_snr() gives "
        "(awgn_core.h) in C, or set the composer's snr/snr_mode on the "
        "source.",
        C_DIRS,
        (".c", ".h"),
        statements=True,
    ),
)


def delegating_defs(src: str) -> set[tuple[str, int]]:
    """`(marker, def-lineno)` for every function that reaches the library.

    The gate matches a function by NAME, which cannot tell a private
    reimplementation from a correctly-written wrapper. `validate.py`'s
    `evm_db()` is `ber_evm_db()` over a `ber_settle_syms()` window with a
    raise instead of the primitive's 0.0-dB "no lock" sentinel; its
    `rrc_bpsk()` reaches `rrc_taps` through two local helpers. Both are the
    behaviour this gate exists to encourage, and allowlisting them would put
    the best code in the tree on a debt list.

    Delegation is followed TRANSITIVELY through module-level functions,
    because the good factoring is exactly the one that hides the primitive a
    level or two down (`rrc_bpsk` -> `shaped_stream` -> `analytic_rrc` ->
    `rrc_taps`). A one-level check would report that chain as private and
    push the author toward inlining, which is the wrong lesson.
    """
    try:
        tree = ast.parse(src)
    except SyntaxError:  # not our file to judge
        return set()

    # Names this module actually imported FROM doppler. A canonical name only
    # counts as delegation if it came from the library, because several of
    # them are ordinary words: a private pulse with a local called `noise`
    # must not exempt itself, and `bits`/`pn`/`tone` are the same hazard.
    doppler_imports: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.ImportFrom) and (node.module or "").startswith(
            "doppler"
        ):
            for alias in node.names:
                doppler_imports.add(alias.asname or alias.name)

    funcs = {
        n.name: n
        for n in ast.walk(tree)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
    }

    def identifiers(node: ast.AST) -> set[str]:
        """Names the code actually REFERENCES.

        Read from the AST, never from the source text: a regex over the
        segment also matches prose, so a private implementation whose
        docstring merely mentions `rrc_taps` would exempt itself. That is
        not hypothetical -- it is what the mutation test caught when this
        function was first written the easy way.
        """
        out: set[str] = set()
        for sub in ast.walk(node):
            if isinstance(sub, ast.Name):
                out.add(sub.id)
            elif isinstance(sub, ast.Attribute):
                out.add(sub.attr)
        return out

    words = {name: identifiers(node) for name, node in funcs.items()}

    def reaches(name: str, marker: str, seen: set[str]) -> bool:
        if name in seen:
            return False
        seen.add(name)
        used = words.get(name, set())
        if CANONICAL[marker] & used & doppler_imports:
            return True
        return any(
            reaches(callee, marker, seen)
            for callee in (used & set(funcs)) - {name}
        )

    return {
        (marker, node.lineno)
        for name, node in funcs.items()
        for marker in CANONICAL
        if reaches(name, marker, set())
    }


def signature_default_lines(src: str) -> set[int]:
    """Lines holding a function PARAMETER's default value.

    A signature default is the API declaring its own zero -- "no offset unless
    you ask for one" -- not a site that seeds a loop. `freq_offset=0.0` on
    `demod()` is the harness saying the caller must opt in, and its docstring
    already says so in as many words; reporting it would put an allowlist entry
    against correct code, which this file's own README warns dilutes the
    ratchet.

    Read from the AST rather than by eye: a default may sit on the `def` line
    or on any continuation line inside the parens, and both spellings appear
    here.
    """
    try:
        tree = ast.parse(src)
    except SyntaxError:
        return set()
    lines: set[int] = set()
    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        a = node.args
        for default in (*a.defaults, *(d for d in a.kw_defaults if d)):
            lines.update(range(default.lineno, (default.end_lineno or 0) + 1))
    return lines


def occurrences(root: Path = ROOT) -> list[tuple[str, str, int, str, str]]:
    """Every (marker, file, line-no, line, instead) in the consuming layers.

    Each file is read against the signatures whose scope holds it: the
    Python-shaped markers over `SCAN_DIRS`, the C noise level over `C_DIRS`.
    """
    found: list[tuple[str, str, int, str, str]] = []
    for d in sorted({d for sig in SIGNATURES for d in sig.dirs}):
        base = root / d
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if "__pycache__" in path.parts:
                continue
            sigs = [
                sig
                for sig in SIGNATURES
                if d in sig.dirs and path.suffix in sig.suffixes
            ]
            if not sigs:
                continue
            rel = path.relative_to(root).as_posix()
            text = path.read_text(errors="replace")
            delegating = (
                delegating_defs(text) if path.suffix == ".py" else set()
            )
            sig_defaults = (
                signature_default_lines(text)
                if path.suffix == ".py"
                else set()
            )
            for sig in sigs:
                if not sig.statements:
                    continue
                for n, stmt in c_statements(text):
                    if any(sig.pattern.search(t) for t in top_terms(stmt)):
                        found.append((sig.marker, rel, n, stmt, sig.instead))
            sigs = [sig for sig in sigs if not sig.statements]
            for n, line in enumerate(text.splitlines(), 1):
                # PROSE is not code. The `offset` marker reads an ordinary
                # assignment, which is a shape that also turns up in the
                # sentence describing it -- `rx_nda_tap.c` explains its
                # `foff = 0` row in a docblock, and reporting that comment as
                # a violation would teach authors to stop explaining their
                # stimulus. The earlier three markers match `def`/declaration
                # syntax and were never affected either way.
                stripped = line.lstrip()
                if stripped.startswith(("*", "//", "#", "/*")):
                    continue
                for sig in sigs:
                    marker = sig.marker
                    if not sig.pattern.search(line):
                        continue
                    if (marker, n) in delegating:
                        continue
                    if marker == "offset" and n in sig_defaults:
                        continue
                    found.append((marker, rel, n, line.strip(), sig.instead))
    return found


#: The C layers that hand a synth a pattern: the `once` rule's scope. The
#: synth's own unit tests are in it too -- they test the latch, so they
#: name `data_ended` already.
ONCE_DIRS = C_DIRS

#: A CALL that hands the synth a finite pattern, or the data mode that
#: carries one. A bare symbol in a binding table (`(jm_any_fn)dp_...,`) is
#: not a call.
ONCE_SETS = re.compile(
    r"\bdp_wfm_synth_set_(bits|dsss_chips)\s*\(|\bWFM_DSSS_DATA_BITS\b"
)
ONCE_CHECK = re.compile(r"\bdp_wfm_synth_data_ended\s*\(")


def unchecked_patterns(root: Path = ROOT) -> list[tuple[str, int, str]]:
    """Every C file that sets a finite pattern and never asks if it ended.

    Returns the FIRST setting line of each such file, as (file, line-no,
    line). Comment lines are skipped on both sides, as in `occurrences()`.
    """
    bad: list[tuple[str, int, str]] = []
    for d in ONCE_DIRS:
        base = root / d
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            code = [
                (n, line)
                for n, line in enumerate(
                    path.read_text(errors="replace").splitlines(), 1
                )
                if not line.lstrip().startswith(("*", "//", "/*"))
            ]
            sets = [(n, ln) for n, ln in code if ONCE_SETS.search(ln)]
            if sets and not any(ONCE_CHECK.search(ln) for _, ln in code):
                n, ln = sets[0]
                bad.append((path.relative_to(root).as_posix(), n, ln.strip()))
    return bad


def load_allow(root: Path = ROOT) -> Counter[str]:
    """How many times each `marker::file::snippet` key is allowed.

    A key is a line's TEXT, so two identical lines in one file share it.
    Counting the entries, one per site, is what stops a third copy of an
    allowlisted line riding in on the first two's entry: three identical
    noise draws in one test file are three sites, and the ratchet has to see
    a fourth.
    """
    allowed: Counter[str] = Counter()
    path = root / ALLOW
    if not path.exists():
        return allowed
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, _, _reason = line.partition("|")
        allowed[key.strip()] += 1
    return allowed


def key_for(marker: str, rel: str, line: str) -> str:
    """Line-number-free identity, so unrelated edits don't churn the list."""
    return f"{marker}::{rel}::{' '.join(line.split())}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="tree to scan; its allowlist is <root>/" + ALLOW,
    )
    root = ap.parse_args().root.resolve()

    unchecked = unchecked_patterns(root)
    if unchecked:
        print(
            "FAIL: a finite pattern with no check that it covered the run.\n"
            "\n"
            "A synth sends a pattern ONCE, then silence (doppler#1718). A\n"
            "harness whose pattern runs dry measures the silence, and says\n"
            "nothing. Tile a periodic stimulus with dp_tx_bits_for_run\n"
            "(native/tests/dp_tx_test.h) and assert, after the run:\n"
            "  DP_CHECK (!dp_wfm_synth_data_ended (syn));\n"
        )
        for rel, n, line in unchecked:
            print(f"  [once] {rel}:{n}\n      {line}\n")
        return 1

    found = occurrences(root)
    allowed = load_allow(root)

    seen: Counter[str] = Counter()
    new: list[tuple[str, str, int, str, str]] = []
    for marker, rel, n, line, instead in found:
        k = key_for(marker, rel, line)
        seen[k] += 1
        if seen[k] > allowed[k]:
            new.append((marker, rel, n, line, instead))

    # An entry per site: more entries than sites is a site that went away.
    stale = sorted((allowed - seen).elements())

    if new:
        print(
            "FAIL: a private stimulus or measurement appeared.\n"
            "\n"
            "doppler is a C library with Python convenience, and it already\n"
            "ships the generator (wfmgen) and the metrics (ber). A private\n"
            "copy invents a CONVENTION -- a level, a window, a scale -- and\n"
            "the convention is what goes wrong silently.\n"
        )
        for marker, rel, n, line, instead in new:
            print(f"  [{marker}] {rel}:{n}\n      {line}")
            print(f"      -> {instead}\n")
        m0, r0, _, l0, _ = new[0]
        print(
            "If this genuinely cannot go through the library, add it to\n"
            f"  {ALLOW}\n"
            "with a reason, in the form:\n"
            f"  {key_for(m0, r0, l0)} | why the library cannot express this"
        )
        return 1

    if stale:
        print(
            "FAIL: the allowlist is a ratchet and has gone stale -- these\n"
            "entries no longer match anything. Delete them; the rule just\n"
            "got stricter and that is the direction it is allowed to move.\n"
        )
        for k in stale:
            print(f"  {k}")
        return 1

    by_marker = {
        m: sum(1 for f in found if f[0] == m)
        for m in dict.fromkeys(sig.marker for sig in SIGNATURES)
    }
    detail = ", ".join(f"{m} {c}" for m, c in by_marker.items())
    print(
        f"stimulus-sources gate: OK -- {len(found)} occurrence(s) "
        f"({detail}), all allowlisted"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
