#!/usr/bin/env python3
"""Rename the retired correlators' C names to the one correlator's (#2752).

Step 3.2 made `corr/` and `corr2d/` one C object, `correlator/`, and step
3.3 makes `detector/` and `detector2d/` its two detector flavors
(docs/design/correlator.md, section 5). The files were moved with `git mv`;
this rewrites what they and their callers SAY, from one table, so the
rename is reviewable as a list rather than as a diff of a thousand lines.

The rules:

- **The table is the whole change.** Each entry is a literal old name and
  its new one. A symbol is matched as a whole word, so `dp_corr2d_reset`
  never touches `dp_corr2d_reset_pinned_vacuously` (a finding key, which
  is a stable name and keeps its spelling). Nothing outside the table is
  rewritten.
- **Every entry must match, or already have been applied.** An entry whose
  old name matches nothing AND whose new name appears nowhere in scope is a
  mistake in the table, and the run fails before writing a byte. An entry
  whose new name is already there has been applied, so a re-run on a
  renamed tree (to take in a path added to the scope) rewrites only what
  is left, and a run with nothing left prints "nothing to rename" and
  exits 0.
- **The scope is the live tree.** History is not rewritten: the
  changelog, the published benchmarks, the dated measurement records, and
  the design page, which describes main as it was measured. `docs/c-api`
  is generated from the headers and is regenerated, not edited. The
  validation report is out of scope too (EXCLUDE names its directory,
  `validation/correlator/`, where git mv put it after the first run): its
  validator and the links to it were edited by hand. The `corr/` sources
  this step deletes are not rewritten, since nothing of them survives.
  `example-projects/` is in scope: its C names the same symbols, and
  `make uno-q-check` builds it.

Usage::

    uv run python scripts/codemod_correlator_rename.py          # rewrite
    uv run python scripts/codemod_correlator_rename.py --check  # dry run

Both print each entry's match count. Exit 1 if any entry matched nothing
and was never applied.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Paths first: they contain the directory names the symbols below share.
PATHS: list[tuple[str, str]] = [
    ("doppler/corr2d/corr2d_core.h", "doppler/correlator/correlator_core.h"),
    ("doppler/corr/corr_core.h", "doppler/correlator/correlator_core.h"),
    ("src/corr2d/corr2d_map.h", "src/correlator/correlator_map.h"),
    ("../corr2d/corr2d_map.h", "../correlator/correlator_map.h"),
    ("src/corr2d/corr2d_core.c", "src/correlator/correlator_core.c"),
    (
        "tests/corr2d/test_corr2d_core.c",
        "tests/correlator/test_correlator_core.c",
    ),
    (
        "tests/corr2d/test_corr2d_symbols.c",
        "tests/correlator/test_correlator_symbols.c",
    ),
    ("tests/corr2d/test_corr_equiv.c", "tests/correlator/test_corr_equiv.c"),
    (
        "benchmarks/corr2d/bench_corr2d_core.c",
        "benchmarks/correlator/bench_correlator_core.c",
    ),
    # Step 3.3. The private header first: its directory prefix is shared.
    (
        "doppler/detector/det_private.h",
        "doppler/correlator/correlator_private.h",
    ),
    ("detector/det_private.h", "correlator/correlator_private.h"),
    (
        "doppler/detector2d/detector2d_core.h",
        "doppler/correlator/correlator_core.h",
    ),
    (
        "doppler/detector/detector_core.h",
        "doppler/correlator/correlator_core.h",
    ),
]

# Whole words. The constructors become the flavors' (section 4's table);
# every other function keeps its verb under the one prefix.
_2D_VERBS = [
    "destroy", "reset", "set_ref", "execute", "execute_max_out",
    "execute_fold", "execute_all", "execute_all_max_out",
    "execute_coherent", "execute_coherent_max_out", "execute_power_sum",
    "execute_power_sum_max_out", "execute_power_max",
    "execute_power_max_max_out", "execute_peaks", "execute_peaks_max_out",
    "bank_fold", "bank_power_max", "bank_power_max_max_out", "bank_detect",
    "bank_detect_max_out", "state_bytes", "get_state", "set_state",
]  # fmt: skip
_1D_VERBS = [
    "destroy", "reset", "set_ref", "execute", "execute_max_out",
    "state_bytes", "get_state", "set_state",
]  # fmt: skip
# Step 3.3: the detectors' verbs, under the one prefix for both flavors.
_DET_VERBS = [
    "destroy", "reset", "set_ref", "set_threshold", "push", "consumed",
    "state_bytes", "get_state", "set_state",
]  # fmt: skip

WORDS: list[tuple[str, str]] = [
    ("dp_corr2d_create_delay_doppler", "dp_correlator_create_delay_doppler"),
    ("dp_corr2d_create", "dp_correlator_create_2d"),
    ("dp_corr_create", "dp_correlator_create_1d"),
    ("dp_corr2d_state_t", "dp_correlator_state_t"),
    ("dp_corr_state_t", "dp_correlator_state_t"),
    ("dp_corr2d_fold_t", "dp_correlator_fold_t"),
    *[(f"dp_corr2d_{v}", f"dp_correlator_{v}") for v in _2D_VERBS],
    *[(f"dp_corr_{v}", f"dp_correlator_{v}") for v in _1D_VERBS],
    ("dp_detector2d_create", "dp_correlator_create_detector_2d"),
    ("dp_detector_create", "dp_correlator_create_detector_1d"),
    ("dp_detector2d_state_t", "dp_correlator_state_t"),
    ("dp_detector_state_t", "dp_correlator_state_t"),
    ("det_result2d_t", "dp_correlator_hit_t"),
    ("det_result_t", "dp_correlator_hit_t"),
    *[(f"dp_detector2d_{v}", f"dp_correlator_{v}") for v in _DET_VERBS],
    *[(f"dp_detector_{v}", f"dp_correlator_{v}") for v in _DET_VERBS],
    ("det_private.h", "correlator_private.h"),
    ("detector2d_core", "correlator_core"),
    ("detector_core", "correlator_core"),
    ("DP_CORR2D_CORE_H", "DP_CORRELATOR_CORE_H"),
    ("DP_CORR2D_MAP_H", "DP_CORRELATOR_MAP_H"),
    ("corr2d_map.h", "correlator_map.h"),
    ("corr2d_core", "correlator_core"),
    ("corr_core", "correlator_core"),
]

# Manifest component names, as `depends_on` spells them.
MANIFEST: list[tuple[str, str]] = [
    ('name = "corr2d"', 'name = "correlator"'),
    ('name = "corr"', 'name = "correlator"'),
]

INCLUDE = [
    "native/**/*.c",
    "native/**/*.h",
    "native/**/*.cmake",
    "native/**/CMakeLists.txt",
    "native/.c-header-code-ignore",
    "native/inc/.example-arity-ratchet",
    "example-projects/**/*.c",
    "example-projects/**/*.h",
    "objects/*.toml",
    "src/**/*.py",
    "src/**/*.md",
    "scripts/.*",
    "docs/**/*.md",
]

EXCLUDE = re.compile(
    r"^(native/src/corr/|native/inc/doppler/corr/|docs/c-api/"
    # Step 3.3: what it deletes, what jm re-renders and the report it moves
    # by hand.
    r"|native/src/detector2?d?/|native/inc/doppler/detector2?d?/"
    r"|native/tests/detector2?d?/|native/benchmarks/detector2?d?/"
    r"|native/src/spectral/spectral_ext_detector2?d?\.c$"
    r"|objects/detector2?d?\.toml$"
    r"|src/doppler/spectral/tests/validation/detector2d/"
    r"|docs/design/correlator\.md$|docs/design/[^/]*-measurements"
    r"|src/doppler/spectral/tests/validation/correlator/"
    # Two ratchet lists name the OLD symbols on purpose: the design page's
    # exemptions (it describes main as measured) and the set_ref row's
    # `replaces:` record. Rewriting them would erase what they record.
    r"|scripts/\.doc-symbol-exempt$|scripts/\.mutator-state-exempt$"
    # The codemod's own test names the old symbols as its fixtures; the
    # retired-names list records history in its comments; and the
    # allow-list names the deleted detector sources, whose rows come off.
    r"|src/doppler/tests/test_codemod_|scripts/\.retired-names$"
    r"|scripts/\.alloc-helper-allow$"
    r"|scripts/codemod_)"
)


def _pattern(old: str, word: bool) -> re.Pattern[str]:
    body = re.escape(old)
    return re.compile(rf"(?<![\w]){body}(?![\w])" if word else body)


def _table() -> list[tuple[str, re.Pattern[str], str]]:
    rows = [(o, _pattern(o, False), n) for o, n in PATHS]
    rows += [(o, _pattern(o, True), n) for o, n in WORDS]
    rows += [(o, _pattern(o, False), n) for o, n in MANIFEST]
    return rows


def _files() -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "-z", "--", *INCLUDE],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout
    return [ROOT / p for p in out.split("\0") if p and not EXCLUDE.match(p)]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--check", action="store_true", help="count, write nothing"
    )
    args = ap.parse_args(argv)

    table = _table()
    counts = {old: 0 for old, _, _ in table}
    applied = {old: False for old, _, _ in table}
    new_text: dict[Path, str] = {}
    for path in _files():
        text = path.read_text(encoding="utf-8")
        orig = text
        for old, pat, new in table:
            # The new name in the file as found means the entry was applied
            # by an earlier run; read before this run rewrites anything, so
            # this run's own output never counts as proof.
            if new in orig:
                applied[old] = True
            text, n = pat.subn(new, text)
            counts[old] += n
        if text != orig:
            new_text[path] = text

    width = max(len(o) for o in counts)
    for old, _, new in table:
        print(f"  {counts[old]:5d}  {old:<{width}}  ->  {new}")
    unmatched = [o for o, c in counts.items() if c == 0 and not applied[o]]
    print(
        f"codemod_correlator_rename: {sum(counts.values())} match(es) in "
        f"{len(new_text)} file(s), {len(table)} entries"
    )
    if unmatched:
        print("FAIL: entries that matched nothing (fix the table):")
        for o in unmatched:
            print(f"  {o}")
        return 1
    if not new_text:
        print("codemod_correlator_rename: nothing to rename")
        return 0
    if not args.check:
        for path, text in new_text.items():
            path.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
