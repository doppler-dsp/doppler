#!/usr/bin/env python3
"""Fail when a C test re-defines something the shared harness provides.

This directory held **90 copies of `CHECK` in six incompatible variants** --
two arities, two failure semantics, and one whose condition was inverted --
while `dp_state_test.h` documented the macro as "already present in every
test_*_core.c". Twenty of those copies had their failure gate drift above
later assertions, so 75 checks printed FAIL and their tests still exited 0.

None of that was a knowledge problem. `dp_state_test.h` said what it needed
and the convention was written down; the copies accumulated anyway. So the
rule is a gate, not a paragraph.

What it forbids
---------------
1. Re-defining any macro the `dp_*.h` family defines (`DP_CHECK`,
   `DP_TEST_END`, `DP_STATE_ROUNDTRIP_TEST`, ...).
2. Defining an assertion macro of your own -- anything named `CHECK`,
   `REQUIRE`, `EXPECT` or `ASSERT`, with or without a suffix. That is the
   original sin, and it is what a new file scaffolded from an old template
   will do.
3. Re-implementing a helper the family exports (`dp_nearf`, `dp_cgauss`,
   `dp_dsss_capture`, ...), including under the historical names the
   consolidation retired.
4. Rolling a private random source: an inline xorshift step, an inline
   linear-congruential step, a hand-written Box-Muller, or either of the two
   uniform mappings. `dp_rng_test.h` is the sanctioned home and the only file
   exempt. **This rule and rule 5 scan `native/validation/` too** -- see
   "Scope is per rule" below.
5. Drawing twice from ONE generator state inside one expression. The two
   calls are indeterminately sequenced (C11 6.5.2.2p10), so the order is the
   compiler's -- and gcc and clang really do disagree, which means such a
   line draws a different noise stream under `make test` (gcc) than under
   `make coverage` (clang).

The forbidden list is DERIVED from the family on every run, not written here,
so a macro added to any member -- or a whole new member -- is covered without
editing this file.

Scope is per rule, not per script
---------------------------------
Rules 4 and 5 -- the randomness pair -- scan `native/tests/` AND
`native/validation/`. Everything else scans `native/tests/` only.

That split is the point rather than an oversight. Rules 1-3 and the assertion
ratchet are about who owns the ASSERTIONS, and a validation harness does not
assert; it prints a number. Rules 4 and 5 are about who owns the RANDOMNESS,
and that is a claim about the whole C-side harness.

`native/validation/` is in fact where a private generator does the MOST
damage, for the reason it is exempt from the others: with no assertion there
is no margin, and no failure. A generator that is quietly wrong becomes a
PUBLISHED figure that is quietly wrong.

The gate did not scan it until this widening -- while printing a summary
line that said `no private RNG` without qualification, over four harnesses
that had one. `check_stimulus_sources.py` had scanned both directories all
along, so the two SSOT gates disagreed about their own scope and the
narrower one was the one whose rule was stated as absolute.

What widening it found is the argument for having done it. `rx_dynamics.c`
drew both Gaussian components from ONE state inside one expression -- rule
5, a different noise stream under gcc than under clang -- and rule 5 could
not see that either, because its wrapper fold starts from the `dp_*` names
and that file's generator chain was private all the way down. **A private
generator does not just risk being wrong; it hides the other rules from the
code that uses it.**

Why (4) is here and not in check_stimulus_sources.py
---------------------------------------------------
That gate looked at hand-rolled Gaussian noise and declined: it "hits 72
files, most of them legitimately", and a ratchet that large is noise, and a
noisy gate gets switched off. Correct across the Python layer. Not correct
HERE, where the xorshift and Box-Muller copies were all migrated rather than
excused -- so for those shapes the rule is ABSOLUTE across both directories,
and an absolute rule is worth a gate.

The one ratchet is `scripts/.private-rng-ratchet`, and it holds only what
the LCG idiom found on its first run: four inline linear-congruential
streams the xorshift-only scan had never looked for, three of them in the
directory this docstring called zero. Migrating those moves their streams
and therefore the numbers measured against them, so they are listed with a
reason. The list may only shrink, and an entry matching nothing fails.

The cost of not having had it: the xorshift step was written out twenty times
and `gauss` five, and the fifth was a half-finished edit -- `(void)u1`, then
two uniforms drawn one shift apart from a two-shift recurrence. It delivered
mean +0.056 and variance 1.115 where it claimed N(0,1), so the file's only
AWGN test ran 0.47 dB hot on biased, heavy-tailed noise for as long as it
existed. Nothing failed. A private generator cannot be wrong in a way
anything notices, which is exactly why it may not be private.

The assertion ratchet
---------------------
The second half of this gate exists because naming was not the only way the
harness lost ground. A migration, a rebase, or a badly resolved conflict can
silently REDUCE the number of assertions in a file: it still compiles, the
survivors still pass, `ctest` still reports 100%, and nothing announces that
coverage went backwards. That is a smaller-denominator failure -- read the
count, not the percentage.

It is not hypothetical either. This branch was cut before
`feat(telemetry)` landed on `main`, and the consolidation would have dropped
**43 assertions across three files** (`test_RateConverter_core.c` -20,
`test_mpsk_receiver_core.c` -12, `test_agc_core.c` -11) with a green suite.
One of the three was spotted by review; the other two only by counting.

So: no test file may end up with fewer assertions than the base ref has,
unless it is listed in `native/tests/.assertion-ratchet-ignore` with a
reason. Deliberate removals are fine and rare; silent ones are the bug.

Usage
-----
    python scripts/check_tests_ssot.py                  # naming only
    python scripts/check_tests_ssot.py --base origin/main   # + the ratchet
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TESTS = ROOT / "native" / "tests"
VALIDATION = ROOT / "native" / "validation"
HEADER = TESTS / "dp_test.h"

#: The directories each rule scans. SCOPE IS PER RULE, not per script, and
#: the split is the point rather than an oversight.
#:
#: The randomness rules (4 and 5) are about `dp_rng_test.h`, which is a claim
#: about the whole C-side harness: `native/validation/` includes that header
#: already, and its files are where a private generator does the MOST damage,
#: because a validation harness REPORTS a number rather than asserting a
#: bound. A generator that is quietly wrong there becomes a published figure
#: that is quietly wrong -- with nothing to notice, since there is no
#: assertion to have margin.
#:
#: The rest of this gate is genuinely `native/tests`-specific. Re-defining a
#: `dp_*.h` macro, naming your own assertion, and the assertion ratchet are
#: all about who owns the ASSERTIONS, and `native/validation/` does not
#: assert -- it prints. Widening those by changing one glob would have made
#: the gate opine about a directory whose contract is different.
#:
#: Both scopes are given as globs of tracked files, so a new harness in
#: either directory is covered the day it lands.
RNG_DIRS = (TESTS, VALIDATION)

# Names the consolidation retired. Kept as a short explicit list because they
# no longer exist anywhere to be derived FROM -- that is the whole point.
RETIRED = [
    "_almost_eq",
    "_almost_eq_c",
    "almost_eq_c",
    "_feq",
    "_ceq",
    "ceq",
    "ALMOST_EQ",
    "ALMOST_EQ_C",
    "FEQC",
]

ASSERTION_LIKE = re.compile(r"^(CHECK|REQUIRE|EXPECT|ASSERT)\w*$")


def _tracked(*dirs: pathlib.Path) -> set[pathlib.Path]:
    """The files under `dirs` that are actually in the repository.

    This gate is a claim about what doppler's test suite contains, so it
    must read what the repository contains — not whatever happens to be
    sitting in the working tree.

    The distinction is not hypothetical. `jm apply` materialises its own
    create-only `jm_test.h`, which doppler does not use (the C tests run on
    `dp_test.h`, and this gate is what makes that true) and which defines
    the retired `ALMOST_EQ` / `ALMOST_EQ_C` spellings. It is gitignored, so
    it never lands — but a plain glob still found it and failed `make lint`
    with four violations in a file nobody had written and nobody could
    remove for good. Deriving from `git ls-files` is the same fix
    `validate-c` already uses after a glob there ran a stale binary whose
    source had been deleted.

    A file has to be staged before this sees it, which is the repository's
    existing tradeoff for new files and lint.
    """
    dirs = dirs or (TESTS,)
    rel = [str(d.relative_to(ROOT)) for d in dirs]
    out = subprocess.run(
        ["git", "ls-files", "--", *rel],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if out.returncode != 0:  # not a checkout (a tarball, say) — scan it all
        found: set[pathlib.Path] = set()
        for d in dirs:
            found |= set(d.glob("*.c")) | set(d.glob("*.h"))
        return found
    return {ROOT / line for line in out.stdout.split() if line}


def sources(*dirs: pathlib.Path) -> list[pathlib.Path]:
    """Every tracked `.c` and `.h` under `dirs` (default `native/tests/`)."""
    dirs = dirs or (TESTS,)
    keep = _tracked(*dirs)
    out: list[pathlib.Path] = []
    for d in dirs:
        out += [
            p for p in list(d.glob("*.c")) + list(d.glob("*.h")) if p in keep
        ]
    return sorted(out)


def family() -> list[pathlib.Path]:
    """Every shared harness header, which is every `dp_*.h` here.

    Derived by glob rather than listed, for the same reason the forbidden
    macro set is derived from the headers: a new family member is covered the
    day it lands, without an edit here that someone has to remember.

    The glob is `dp_*.h` and NOT `dp_*_test.h`, which is the obvious spelling
    and is wrong: `dp_test.h` -- the founding member, and the one that owns
    every assertion -- does not match it. Written that way, this gate parsed
    six headers, reported a cheerful count of them, and quietly stopped
    noticing a test that re-defined `DP_CHECK`. Caught by sabotage; it read as
    correct. Hence the assertion in `main`.
    """
    return sorted(p for p in _tracked() if p.match("native/tests/dp_*.h"))


def provided() -> tuple[dict[str, str], dict[str, str]]:
    """(macros, functions) the family defines, each mapped to its owner.

    This read `dp_test.h` alone until `dp_rng_test.h` landed, which meant the
    gate covered the assertions and nothing else: a test could re-implement
    `dp_cgauss` or re-declare `DP_STATE_ROUNDTRIP_TEST` and the checker had
    no opinion. Owning the name is the point of the family, not a property of
    one member of it.
    """
    macros: dict[str, str] = {}
    funcs: dict[str, str] = {}
    for header in family():
        text = header.read_text()
        for name in re.findall(r"^#define\s+(\w+)", text, re.M):
            macros.setdefault(name, header.name)
        for name in re.findall(r"^(dp_\w+)\s*\(", text, re.M):
            funcs.setdefault(name, header.name)
    return macros, funcs


#: Generator idioms that must not reappear in a test. `dp_rng_test.h` is the
#: sanctioned home and is exempt; everything else in `native/tests` gets its
#: randomness from there.
#:
#: This is the narrow version of a check `check_stimulus_sources.py`
#: deliberately declined to make. That gate says a hand-rolled Box-Muller
#: "hits 72 files, most of them legitimately", and a ratchet that large is
#: noise. True across the Python layer and the validation harnesses — and not
#: true HERE, where after the consolidation the count is zero and the rule is
#: absolute. A gate is only worth having where it can be a rule.
GENERATOR_IDIOMS = [
    (
        # The defining shape is that the SAME lvalue appears on both sides:
        # `x ^= x << 13`, `*st ^= *st >> 17`. Hence the backreference, and it
        # is load-bearing in both directions. Without the `*` form the pattern
        # missed nine of the twenty historical copies while looking like it
        # worked; with `*` but without the backreference it matched
        # `test_burst_demod_core.c`'s CRC-16 (`c ^= (bits[i] & 1u) << 15`),
        # which xors a DIFFERENT expression and is not a generator at all.
        # Both mistakes were made here, and both were caught by sabotage
        # rather than by reading.
        re.compile(r"(\*?\w+(?:\[\w+\])?)\s*\^=\s*\1\s*(?:<<|>>)\s*\d+"),
        "an inline xorshift step",
        "dp_xs32 / dp_xs64",
    ),
    (
        # The same shape spelled out: `x = x ^ (x << 13)`.
        re.compile(r"(\*?\w+)\s*=\s*\1\s*\^\s*\(?\s*\1\s*(?:<<|>>)\s*\d+"),
        "an inline xorshift step",
        "dp_xs32 / dp_xs64",
    ),
    (
        # `sqrt (-2.0 * log (u))` and `sqrt (-log (u))` were the only two
        # spellings the first version matched, which made a rule stated as
        # absolute into a ratchet on two literals. It missed `sqrtf`/`logf`
        # — the natural spelling in a float-heavy suite, and therefore the
        # one a new private Gaussian would actually use — as well as an
        # integer `-2 *` and `log1p`. Review caught it; the fix is to accept
        # any numeric factor and both float and double names.
        re.compile(r"sqrtf?\s*\(\s*-\s*[-\d.eEf*\s]*log(?:f|1p|1pf)?\s*\("),
        "a hand-rolled Box-Muller transform",
        "dp_gauss / dp_cgauss / dp_gauss64",
    ),
    (
        re.compile(r"/\s*4294967297\.0"),
        "the private (x + 1) / (2^32 + 1) uniform mapping",
        "dp_uni",
    ),
    (
        re.compile(r"/\s*9007199254740993\.0"),
        "the private 53-bit uniform mapping",
        "dp_uni64",
    ),
    (
        # `st = st * 6364136223846793005ULL + 1442695040888963407ULL`. The
        # second-most-obvious way to roll a generator, and the scan looked
        # for xorshift only -- so it printed `no private RNG` over four
        # live LCG streams, three of them in the directory where the rule
        # was stated as ABSOLUTE. Found on this rule's first run, which is
        # the argument for writing it rather than trusting the sentence.
        #
        # Same backreference discipline as the xorshift idioms, and
        # load-bearing for the same reason: the SAME lvalue on both sides
        # is what makes it a stream. `x = k * 1103515245u + 12345u`, the
        # one-shot index hash in `dp_mf_test.h`, multiplies a DIFFERENT
        # value and carries no state between calls -- it is a deterministic
        # bit pattern, not a random source, and reporting it would put
        # correct code on a ratchet.
        #
        # Five digits of multiplier is the discriminator. Every LCG in the
        # literature and in this tree uses a large odd constant (48271,
        # 69069, 1664525, 1103515245, 6364136223846793005); `n = n * 2 + 1`
        # is arithmetic and must stay silent.
        re.compile(r"(\*?\w+)\s*=\s*\1\s*\*\s*\d{5,}"),
        "an inline linear-congruential step",
        "dp_xs32 / dp_xs64",
    ),
    (
        re.compile(r"(\*?\w+)\s*\*=\s*\d{5,}"),
        "an inline linear-congruential step",
        "dp_xs32 / dp_xs64",
    ),
]

RNG_HOME = "dp_rng_test.h"

#: Pre-existing private generators, one per line, each with a reason.
#: A RATCHET: it may only shrink. See the file's own header.
RNG_RATCHET = ROOT / "scripts" / ".private-rng-ratchet"


def strip_comments(text: str) -> str:
    """Blank out C comments, preserving line numbering.

    Required, not tidiness: `dp_rng_test.h` documents the broken generator it
    replaced by QUOTING it, and `test_costas_core.c` explains in prose what it
    no longer does. A scanner that reads comments would fire on the
    documentation of the very rule it enforces — the failure mode where
    describing a detector's target blinds or trips the detector.

    String- and char-literal aware, which the first version was not. A regex
    split treats the `/*` inside `"a/*b"` as opening a comment and blanks
    everything up to the next `*/` in any later literal — taking real code
    with it and reporting zero violations for the span. A false NEGATIVE, in
    a gate whose entire value is being absolute, triggered by a test gaining
    a URL or a format string. Found by review, not by the sabotage battery,
    which only ever fed it well-formed code.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":  # literal: copied out verbatim
            quote, j = c, i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                if text[j] == "\n":  # unterminated; do not run away
                    break
                j += 1
            out.append(text[i:j])
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


#: A draw call from `dp_rng_test.h`, with the state it advances.
DRAW = re.compile(
    r"\bdp_(?:xs32|xs64|uni|uni64|bit|gauss|gauss64|cgauss)\s*\(\s*&?(\w+)"
)


def double_draws() -> list[str]:
    """Fail an expression that draws twice from ONE generator state.

    Two calls in one expression are indeterminately sequenced (C11
    6.5.2.2p10) -- no undefined behaviour, but the order is the compiler's,
    and gcc and clang really do choose differently: for
    `(..) + (..) * I` gcc evaluates the imaginary operand first and clang the
    real one, at -O0 and -O2 alike. doppler compiles these tests with both
    (`make test` is gcc, `make coverage` is clang), so such a line draws a
    different noise stream per job -- silently, since the assertions are
    statistical and both orders pass.

    Eleven sites across seven files had this when the check was written,
    seven of them genuinely divergent. That is too many to hold with a
    paragraph, which is the only reason this is a gate: the review that found
    the class judged a regex not obviously worth it, and it would have been
    right at three sites.

    Statements are split on `;` and only a REPEATED state name counts, so
    `dp_bit (&bst) + dp_gauss (&nst)` -- two streams, order irrelevant -- is
    silent, which is the form the header teaches.

    A local wrapper counts as a draw. `test_corr2d_core.c` writes
    `_rand_uniform (&seed) + _rand_uniform (&seed) * I`, where the draw is one
    call deeper; a scan that only knew the `dp_*` names walked straight past
    it, and review had to point at it. Any `static` function in the file whose
    body draws is folded into the set first, so the check follows one level of
    indirection instead of trusting that nobody wraps.

    That fold reads function definitions in GNU style -- name at the start of
    a line, brace on its own line -- because that is what `clang-format`
    produces and `make lint` runs it before this gate. A wrapper written all
    on one line would be missed. Stated rather than hidden: it is a real
    precondition, and it is why the sabotage for this rule has to be written
    in the repo's own style to mean anything. A one-line sabotage passed and
    proved nothing.
    """
    bad = []
    for path in sources(*RNG_DIRS):
        code = strip_comments(path.read_text())
        names = ["dp_(?:xs32|xs64|uni|uni64|bit|gauss|gauss64|cgauss)"]
        for _ in range(3):  # to a fixpoint; three levels is generous
            drawer = re.compile(r"\b(?:" + "|".join(names) + r")\s*\(")
            found = set()
            for m in re.finditer(
                r"^(\w+)\s*\([^;]*?\)\s*\n\{(.*?)\n\}", code, re.M | re.S
            ):
                if drawer.search(m.group(2)):
                    found.add(m.group(1))
            fresh = [re.escape(f) for f in found if re.escape(f) not in names]
            if not fresh:
                break
            names += fresh
        draw = re.compile(r"\b(?:" + "|".join(names) + r")\s*\(\s*&?(\w+)")
        line, buf, start = 1, [], 1
        for ch in code:
            if ch == "\n":
                line += 1
            if ch == ";":
                states = draw.findall("".join(buf))
                dupes = {s for s in states if states.count(s) > 1}
                if dupes:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{start}: draws twice from "
                        f"'{sorted(dupes)[0]}' in one expression — the order "
                        f"is the compiler's (gcc and clang differ). Draw into "
                        f"named locals first."
                    )
                buf, start = [], line
            else:
                buf.append(ch)
    return bad


def rng_occurrences() -> list[tuple[str, str, int, str]]:
    """Every private-generator idiom across `RNG_DIRS`.

    Each occurrence as `(what, rel, line_number, source_line)`.
    """
    found: list[tuple[str, str, int, str]] = []
    for path in sources(*RNG_DIRS):
        if path.name == RNG_HOME:
            continue
        code = strip_comments(path.read_text())
        for n, line in enumerate(code.splitlines(), 1):
            for pattern, what, _use in GENERATOR_IDIOMS:
                if pattern.search(line):
                    found.append(
                        (what, str(path.relative_to(ROOT)), n, line.strip())
                    )
    return found


def rng_key(what: str, rel: str, line: str) -> str:
    """Line-number-free identity, so an unrelated edit does not churn the
    ratchet. Same shape as check_stimulus_sources.py's allowlist key."""
    return f"{what}::{rel}::{' '.join(line.split())}"


def generators() -> list[str]:
    """Fail any harness that rolls its own random source.

    Scans `native/tests/` AND `native/validation/`. The gate looked only at
    `native/tests` until this widening, while printing a summary line that
    said `no private RNG` without qualification -- and four validation
    harnesses had one, in the directory where a private generator does the
    most damage, because a validation harness REPORTS a number instead of
    asserting a bound. There is no assertion there to have margin, so a
    generator that is quietly wrong becomes a published figure that is
    quietly wrong. `check_stimulus_sources.py` had scanned both directories
    all along, so the two SSOT gates disagreed about their own scope and the
    narrower one was the one whose rule was stated as absolute.

    Widening it found the second half of the same defect: `rx_dynamics.c`
    drew both Gaussian components from ONE state inside one expression --
    rule 5, indeterminately sequenced, a different noise stream under gcc
    than under clang -- and rule 5 could not see it either, because its
    wrapper fold starts from the `dp_*` names and this file's chain was
    private all the way down. A private generator does not just risk being
    wrong; it hides the OTHER rules from the code that uses it.

    The four were migrated rather than ratcheted (all bit-exact: three were
    `dp_xs32`/`dp_cgauss`/`dp_bit` spelled out by hand, and
    `ber_despreader.c`'s (13, 7, 17) triple on a `uint64_t` is `dp_xs64`),
    so the rule stays ABSOLUTE for the shapes it already knew.

    What is on the ratchet is what the NEW idiom found: four inline LCG
    streams that the xorshift-only scan had never looked for. Migrating
    those moves their streams, which moves published numbers, so they are
    listed with a reason and the list may only shrink.
    """
    found = rng_occurrences()
    allowed: dict[str, str] = {}
    if RNG_RATCHET.exists():
        for raw in RNG_RATCHET.read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            key, _, reason = line.partition("|")
            allowed[key.strip()] = reason.strip()

    bad: list[str] = []
    seen: set[str] = set()
    for what, rel, n, line in found:
        k = rng_key(what, rel, line)
        seen.add(k)
        if k in allowed:
            continue
        use = next(u for _p, w, u in GENERATOR_IDIOMS if w == what)
        bad.append(
            f"{rel}:{n}: {what} — use {use} from {RNG_HOME}.\n"
            f"      If it genuinely cannot, add to "
            f"{RNG_RATCHET.relative_to(ROOT)}:\n"
            f"      {k} | why"
        )

    # A ratchet may only shrink, so an entry matching nothing is a failure,
    # not a tidy-up: it means the rule got stricter and the list did not
    # follow, and the next reader cannot tell a live excuse from a dead one.
    for k in sorted(set(allowed) - seen):
        bad.append(
            f"{RNG_RATCHET.relative_to(ROOT)}: '{k}' matches nothing — "
            f"the ratchet went stale. Delete the line; the rule just got "
            f"stricter, which is the direction it may move."
        )
    return bad


IGNORE = TESTS / ".assertion-ratchet-ignore"

# What counts as an assertion, on either side of the migration. Both sets are
# counted the same way -- per line, skipping `#define` -- so the comparison is
# between call sites, never between a definition and a call.
OLD_ASSERT = re.compile(r"(?<![A-Za-z0-9_])CHECK\w*\s*\(")
NEW_ASSERT = re.compile(
    r"(?<![A-Za-z0-9_])(DP_CHECK|DP_REQUIRE|DP_CHECK_MSG|DP_REQUIRE_MSG"
    r"|DP_CHECK_NEAR|DP_RECORD_FAIL|DP_STATE_ROUNDTRIP_TEST)\s*\("
)


def unreported_checks() -> list[str]:
    """Fail a test that accumulates failures and never reports them.

    `DP_CHECK` counts into `dp_test_fails_` and returns; only `DP_TEST_END`
    turns that counter into a non-zero exit. A file that ends with its own
    `printf ("... OK ...")` and `return 0` therefore runs every check,
    records every failure, and exits 0 — so each of its `DP_CHECK`s is
    decoration, and `ctest` reports the test as passing while it asserts
    nothing that can fail.

    Measured, not hypothetical, and in both of the ways it bites.
    `test_frame_meter_core.c` shipped 3 `DP_CHECK`s that could not fail.
    `test_wfm_frame.c` had none — it asserted entirely through `DP_REQUIRE`,
    which does return 1 — so its epilogue was latent rather than broken, and
    it went off the moment 12 `DP_CHECK`s were added to it. That is the worse
    half: the file looked fine, and writing an ordinary assertion into it
    produced an assertion that could not fail.

    Found by sabotage, not by review — making `preamble_reps = 0` emit a
    preamble instead of nothing changed the layout and every test still
    passed. It is exactly the shape `DP_TEST_END`'s own "ASSERTED NOTHING"
    guard exists to catch and cannot, because a file that never calls it
    never runs that guard either.

    `DP_REQUIRE` is not affected: it returns 1 on the spot. So the rule is
    narrow — a file using the ACCUMULATING flavour must end by reporting the
    accumulator — and it is absolute rather than a ratchet, because after
    the fix the count is zero.

    Registration-free: derived by scanning, so a new test written from an old
    template is covered the moment it lands.
    """
    bad: list[str] = []
    for path in sources():
        if path.suffix != ".c":
            continue
        text = strip_comments(path.read_text())
        if not re.search(r"\bDP_CHECK\w*\s*\(", text):
            continue
        if "DP_TEST_END" in text:
            continue
        n = len(re.findall(r"\bDP_CHECK\w*\s*\(", text))
        bad.append(
            f"{path.relative_to(ROOT)}: {n} DP_CHECK(s) and no "
            f"DP_TEST_END — the failure counter is never reported, so "
            f"none of them can fail the test"
        )
    return bad


def count_assertions(text: str) -> int:
    n = 0
    for line in text.splitlines():
        if re.match(r"\s*#\s*define", line):
            continue
        n += len(OLD_ASSERT.findall(line)) + len(NEW_ASSERT.findall(line))
    return n


def ratchet(base: str) -> list[str]:
    """Fail any test file that has fewer assertions than `base` has.

    `base` is resolved to the **merge base** with the working tree, not
    taken as given. The question this gate asks is "did THIS branch remove
    assertions", and the only honest baseline for that is where the branch
    started. Compared against the tip of `origin/main` instead, a branch
    that is merely BEHIND fails for assertions someone else ADDED --
    naming a file the branch never touched, with a message that says a
    test lost coverage.

    Measured twice on one branch: `test_mpsk_receiver_core.c` at -13 while
    `main` was 19 commits ahead, then `test_mpsk_core.c` at -10 after a
    soft-demapping commit landed 30 commits ahead. Neither file had been
    edited on the branch. Both cleared by rebasing, which is the tell: a
    gate whose verdict changes when you rebase is measuring the gap to
    `main`, and `git` already reports that.

    The cost of getting this wrong is not the false alarm, it is the habit
    it teaches -- a gate that cries wolf for a reason unrelated to the diff
    trains the reader to rebase-and-ignore, which is exactly how a REAL
    lost assertion would slip past. The ratchet's own docstring says a
    suite can go green while covering less; a ratchet can go red while
    nothing was lost, and that is the same defect from the other side.

    Falls back to `base` itself when no merge base exists (unrelated
    histories, a shallow clone that fetched only a tip): a baseline that
    cannot be computed is not silently skipped.
    """
    ref = base
    mb = subprocess.run(
        ["git", "merge-base", "HEAD", base],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if mb.returncode == 0 and mb.stdout.strip():
        base = mb.stdout.strip()
        shown = f"the merge base with {ref} ({base[:9]})"
    else:
        shown = ref

    rev = subprocess.run(
        ["git", "rev-parse", "--verify", f"{base}^{{commit}}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if rev.returncode != 0:
        raise LookupError(
            f"base ref {base!r} does not resolve.\n"
            "  A ratchet that cannot read its baseline has not passed, so\n"
            "  this is a failure rather than a skip — but it is NOT a lost\n"
            "  assertion, and reporting it as one sends the reader hunting\n"
            "  a regression that is not there.\n"
            "  Fetch it:  git fetch --no-tags --depth=1 origin \\\n"
            "               +refs/heads/main:refs/remotes/origin/main"
        )

    # Two forms, and the difference matters. A bare `<file>  <reason>` is a
    # permanent, unbounded excuse -- the file's ratchet is off from then on,
    # for any future drop. A `<file> -> <dest>  <reason>` says the assertions
    # MOVED, and is checked: `dest` must have gained at least what `file`
    # lost, over the same base. A move is the common case (splitting one
    # object's tests out of another's) and it is not a removal, so writing it
    # as one both lies in the ignore file and disarms a ratchet that should
    # stay armed.
    excused: dict[str, tuple[str | None, str]] = {}
    if IGNORE.exists():
        for line in IGNORE.read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            name, _, rest = line.partition(" ")
            rest = rest.strip()
            dest = None
            if rest.startswith("-> "):
                dest, _, rest = rest[3:].partition(" ")
            excused[name] = (dest, rest.strip())

    listing = subprocess.run(
        ["git", "ls-tree", "-r", "--name-only", base, "native/tests/"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.split()

    def at_base(rel: str) -> int:
        """Assertion count for a repo-relative path at the base ref. A file
        the base does not have counts zero, which is what a move into a
        brand-new file needs."""
        r = subprocess.run(
            ["git", "show", f"{base}:{rel}"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        return count_assertions(r.stdout) if r.returncode == 0 else 0

    bad = []
    for rel in (f for f in listing if f.endswith(".c")):
        was = subprocess.run(
            ["git", "show", f"{base}:{rel}"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if was.returncode != 0:
            continue
        now = ROOT / rel
        if not now.exists():
            continue  # deletion is a visible act, not a silent loss
        before, after = (
            count_assertions(was.stdout),
            count_assertions(now.read_text()),
        )
        if after >= before:
            continue
        key = pathlib.Path(rel).name
        if key not in excused:
            bad.append(
                f"{rel}: {before} assertions at {shown}, {after} here "
                f"(-{before - after})"
            )
            continue
        dest, _why = excused[key]
        if dest is None:
            continue  # stated, permanent removal
        drel = f"{TESTS.relative_to(ROOT)}/{dest}"
        dpath = ROOT / drel
        if not dpath.exists():
            bad.append(
                f"{rel}: excused as moved to {dest}, which does not exist"
            )
            continue
        lost_here = before - after
        gained = count_assertions(dpath.read_text()) - at_base(drel)
        if gained < lost_here:
            bad.append(
                f"{rel}: -{lost_here} assertions, excused as moved to "
                f"{dest} — but {dest} gained only {gained} since {shown}, "
                f"so {lost_here - gained} went nowhere"
            )
    return bad


def main() -> int:
    # An empty result set is not a pass, in either half of this gate. The
    # glibc gate went green on a missing .so and the C tarball gate on an
    # empty tarball; both looked exactly like this code path.
    for required in (HEADER, TESTS / RNG_HOME):
        if not required.exists():
            print(
                f"check_tests_ssot: {required} is missing — nothing to "
                "enforce,"
            )
            print("  so this gate has not run, so it has not passed.")
            return 1

    macros, funcs = provided()
    if not macros or not funcs:
        print("check_tests_ssot: parsed NO names out of the dp_*.h")
        print("  family — the scan found nothing, so it did not run.")
        return 1

    # The family glob must actually reach the founding members. A glob that
    # silently misses one leaves this gate reporting a healthy count while
    # covering less than it used to -- which is what `dp_*_test.h` did to
    # `dp_test.h`, and is the same smaller-denominator failure the assertion
    # ratchet below exists to catch.
    for anchor, macro in (
        ("dp_test.h", "DP_CHECK"),
        (RNG_HOME, "DP_RNG_TWO_PI"),
    ):
        if macros.get(macro) != anchor:
            print(
                f"check_tests_ssot: {macro} is not attributed to {anchor} — "
                "the family"
            )
            print("  glob is not reaching it, so this gate covers less than")
            print("  it claims. Not a pass.")
            return 1

    owners = {p.name for p in family()}
    bad: list[str] = []

    # A tracked test may not include the GITIGNORED harness. `jm apply`
    # materialises `native/tests/jm_test.h` as a create-only file, so a
    # scaffold that includes it compiles on the machine that ran apply and
    # nowhere else -- the header is in .gitignore, so CI checks out a tree
    # where it does not exist. Measured: six generated module tests landed
    # this way and every build job failed with `jm_test.h: No such file or
    # directory`, while the full local gate set was green.
    #
    # The docstring on _tracked() has claimed since it was written that
    # "this gate is what makes that true". It was not: _tracked() only
    # kept the untracked header out of its OWN scan. This is the check
    # that makes the sentence true.
    for path in sources():
        for n, line in enumerate(path.read_text().splitlines(), 1):
            if re.match(r'\s*#\s*include\s*"jm_test\.h"', line):
                bad.append(
                    f"{path.relative_to(ROOT)}:{n}: includes jm_test.h, "
                    "which is gitignored — it exists only on a machine "
                    "that ran `jm apply`, so this builds here and fails "
                    "in CI. Use dp_test.h (DP_CHECK / DP_TEST_END)."
                )

    for path in sources():
        # A family header legitimately defines what it owns. It must still not
        # re-define what a SIBLING owns, so only its own names are skipped.
        mine = path.name if path.name in owners else None
        for n, line in enumerate(path.read_text().splitlines(), 1):
            m = re.match(r"\s*#\s*define\s+(\w+)", line)
            if m:
                name = m.group(1)
                if name in macros and macros[name] != mine:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-defines "
                        f"{name}, which {macros[name]} provides"
                    )
                elif ASSERTION_LIKE.match(name):
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: defines its own "
                        f"assertion {name} — use DP_CHECK / DP_REQUIRE"
                    )
                elif name in RETIRED:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: {name} was "
                        f"retired — use dp_nearf / dp_cnearf"
                    )
            m = re.match(
                r"\s*(?:static\s+)?(?:inline\s+)?\w[\w \t*]*?(\w+)\s*\($",
                line.rstrip(),
            )
            fn = re.match(r"^(\w+)\s*\(", line)
            for cand in (
                m.group(1) if m else None,
                fn.group(1) if fn else None,
            ):
                if cand in RETIRED:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-implements "
                        f"{cand} — it was retired"
                    )
                elif cand in funcs and funcs[cand] != mine:
                    bad.append(
                        f"{path.relative_to(ROOT)}:{n}: re-implements "
                        f"{cand} — {funcs[cand]} exports it"
                    )

    bad += generators()
    bad += double_draws()
    bad += unreported_checks()

    if bad:
        print("check_tests_ssot: the shared harness is the single definition.")
        for b in bad:
            print(f"  {b}")
        print(f"\n  {len(bad)} violation(s). See native/tests/README.md.")
        return 1

    base = None
    for i, a in enumerate(sys.argv):
        if a == "--base" and i + 1 < len(sys.argv):
            base = sys.argv[i + 1]
    try:
        lost = ratchet(base) if base else []
    except LookupError as exc:
        print(f"check_tests_ssot: {exc}")
        return 1
    if lost:
        print("check_tests_ssot: a test file LOST assertions.")
        for line in lost:
            print(f"  {line}")
        print("\n  A suite can go green while covering less. If a removal is")
        print("  deliberate, list the file in")
        print(f"  {IGNORE.relative_to(ROOT)} with the reason.")
        return 1

    scanned = len([p for p in sources() if p.suffix == ".c"])
    # Say what was SCANNED and what is RATCHETED, not just "no private RNG".
    # That line printed unqualified while four validation harnesses rolled
    # their own, because the scan had never looked outside native/tests --
    # a summary that names its scope cannot make that claim again, and a
    # summary that names the ratchet count cannot read as zero.
    rng_scanned = len(sources(*RNG_DIRS))
    held = sum(
        1
        for raw in (
            RNG_RATCHET.read_text().splitlines()
            if RNG_RATCHET.exists()
            else []
        )
        if raw.strip() and not raw.strip().startswith("#")
    )
    dirs = ", ".join(str(d.relative_to(ROOT)) for d in RNG_DIRS)
    print(
        f"check_tests_ssot: OK — {scanned} tests, "
        f"{len(macros)} macros and {len(funcs)} helpers owned by "
        f"{len(family())} dp_*.h harness headers"
    )
    print(
        f"  randomness: {rng_scanned} file(s) across {dirs} — no new "
        f"private RNG"
        + (
            f", {held} ratcheted (may only shrink)"
            if held
            else ", and none held on the ratchet"
        )
    )
    if base:
        print(f"  assertions: no file lost any vs the merge base with {base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
