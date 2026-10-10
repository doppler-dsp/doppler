#!/usr/bin/env python3
"""Gate: a telemetry probe name is joined in ONE place, `dp_tlm_name_join()`.

Every instrumented object names its probes `"<prefix>.<suffix>"` in a
`DP_TLM_NAME_MAX`-byte buffer, and the prefix is the caller's. A bare
`snprintf (name, DP_TLM_NAME_MAX, "%s.%s", prefix, suffix)` truncates that
silently: the probe registers under a shortened name, two probes that
differed only past the cut collapse onto one registry entry, and the attach
still reports success. Ten `set_telemetry` implementations did exactly that
until #676; #1898 gave the join one home in `dp_tlm_core.c`, which refuses
instead of truncating, and `dp_tlm_core.h` says that helper "is the one
place that join happens". This gate is what makes the sentence true (#1944).

**What it refuses.** A string literal that BEGINS with a `%s` conversion
followed by a `.`, the format of a hand-written `"<prefix>.<suffix>"` join.
That includes the spellings with a precision or a position (`"%.*s."`,
`"%1$s."`), and a literal the compiler joins from pieces (`"%s" ".e"`).
`%%` is a literal percent, not a conversion. Anchored at the start because
a probe name starts with its prefix, while a file name such as
`"tlm_capture_%s.tlm"` or `"%s/rx-dyn-%s.tlm"`, which files in scope do
format, has the same `%s.` in its middle. It is refused in any C file
that takes part in probe naming: one that calls `dp_tlm_probe()`, or calls
or defines a PROBE ATTACH, meaning a `*_set_telemetry()` whose declaration
takes a `dp_tlm_t`. That second half reaches a composite, which builds a
child's prefix (`"rx.car"`) and never registers a probe itself. Any
formatter is caught, not only `snprintf`, because a private wrapper around
one is the same copy one hop away.

**What it does not.** Comments are skipped, so documentation may quote the
old spelling, and comments and literals are read by `_c_source.py`.
`dp_tlm_core.c` is the sanctioned home. A `*_set_telemetry()` that takes no
`dp_tlm_t` is not a probe attach: `dp_event_log_set_telemetry()` names a
record FILE, and its file may format `"%s.tlm"` (the same rule as
`NOT_A_PROBE_PREFIX` in `test_tlm_prefix_refusal.py`). A file that never
touches telemetry may format `"%s.sigmf-meta"` or a JSON path as it likes.
No allowlist: every site was converted when the gate landed.

**Blind spots**, things it cannot see by construction:

- a name built without a format: `strcat`, `strncat`, `memcpy`;
- a format `#define`d in a file that is not in scope, or joined from a
  macro (`"%s" SEP "e"`), because the literal is not whole where it is used;
- the separator passed as an argument (`"%s%s%s", p, ".", s`);
- a join that does not start its literal (`"rx.%s.e"`);
- a literal split by a backslash-newline between `%s` and the dot;
- the percent or the dot spelled as an escape (`"\x25s."`, `"\045s."`);
- a precision with no digits (`"%.s."`);
- a join in a helper file that neither registers nor attaches a probe.

The runtime half covers these: `test_tlm_prefix_refusal.py` drives every
Python face with overlong prefixes and catches a truncated name however it
was built.

Usage:  python3 scripts/check_tlm_name_join.py [--root DIR]
Exit 0 when every probe-naming file joins through dp_tlm_name_join().
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from _c_source import string_literals, strip_comments

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIR = "native"
SANCTIONED = "native/src/dp_tlm/dp_tlm_core.c"

# Registering a probe puts a file in scope. `\b...\s*\(` so
# `dp_tlm_probe_id (` and prose without a call do not count.
REGISTERS = re.compile(r"\bdp_tlm_probe\s*\(")
# A *_set_telemetry declaration and its parameter list; it is a probe ATTACH
# when the parameters name the telemetry context.
SET_TLM_DECL = re.compile(r"\b(\w+_set_telemetry)\s*\(([^;{)]*)\)")
TLM_CONTEXT = re.compile(r"\bdp_tlm(?:_state)?_t\b")
# A literal that opens with a %s conversion (position, flags, width,
# precision, `l` allowed) and a dot.
JOIN = re.compile(
    r"^%(?:\d+\$)?[-+ #0]*(?:\d+|\*(?:\d+\$)?)?"
    r"(?:\.(?:\d+|\*(?:\d+\$)?))?l?s\."
)


def _sources(root: Path) -> dict[str, str]:
    """Every C file under native/, comment-stripped, by path."""
    return {
        p.relative_to(root).as_posix(): strip_comments(
            p.read_text(encoding="utf-8", errors="replace")
        )
        for p in sorted((root / SCAN_DIR).rglob("*"))
        if p.suffix in (".c", ".h")
    }


def probe_attaches(sources: dict[str, str]) -> set[str]:
    """The *_set_telemetry functions declared to take a dp_tlm_t."""
    return {
        name
        for code in sources.values()
        for name, params in SET_TLM_DECL.findall(code)
        if TLM_CONTEXT.search(params)
    }


def offenders(root: Path = ROOT) -> list[tuple[str, int, str]]:
    sources = _sources(root)
    attaches = probe_attaches(sources)
    attach = (
        re.compile(r"\b(?:" + "|".join(sorted(attaches)) + r")\s*\(")
        if attaches
        else None
    )
    found: list[tuple[str, int, str]] = []
    for rel, code in sources.items():
        if rel == SANCTIONED:
            continue
        if not (REGISTERS.search(code) or (attach and attach.search(code))):
            continue
        for line, text in string_literals(code):
            if JOIN.match(text):  # "%%" cannot match: % is not a flag
                found.append((rel, line, f'"{text}"'))
    return found


def main() -> int:
    # --root exists for this gate's OWN test: it seeds a tree shaped like
    # native/ and points the scan there, so the gate can be sabotaged
    # without breaking a real set_telemetry.
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    found = offenders(ap.parse_args().root)
    if not found:
        print("tlm-name-join: every probe name joins through dp_tlm_name_join")
        return 0
    print("tlm-name-join: a probe name is joined by hand here --")
    for rel, n, text in found:
        print(f"  {rel}:{n}: {text}")
    print(
        "  Build it with dp_tlm_name_join (name, prefix, suffix) from\n"
        "  native/inc/doppler/dp_tlm/dp_tlm_core.h, and check its return:\n"
        "  it refuses a name that would not fit, where a bare snprintf\n"
        "  truncates it and aliases two probes onto one entry (#676). Build\n"
        "  every name before the first dp_tlm_probe(), so a refusal leaves\n"
        "  the registry untouched."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
