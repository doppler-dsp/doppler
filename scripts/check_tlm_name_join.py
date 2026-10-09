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

**What it refuses.** A string literal whose text begins `%s.`, the format of
a hand-written `"<prefix>.<suffix>"` join, in any C file that takes part in
probe naming: one that calls `dp_tlm_probe()`, or calls or defines a
`*_set_telemetry()`. The second half is what reaches a composite, which
builds a child's prefix (`"rx.car"`) and never registers a probe itself.
Any formatter is caught, not only `snprintf`, because a private wrapper
around one is the same copy with one more hop.

**What it does not.** Comments and prose are skipped, so documentation may
quote the old spelling. `dp_tlm_core.c` is the sanctioned home. A file that
never touches telemetry may format `"%s.sigmf-meta"` or a JSON path as it
likes. No allowlist: every site was converted when the gate landed, so the
first new one fails.

Usage:  python3 scripts/check_tlm_name_join.py [--root DIR]
Exit 0 when every probe-naming file joins through dp_tlm_name_join().
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIR = "native"
SANCTIONED = "native/src/dp_tlm/dp_tlm_core.c"

# A file takes part in probe naming if it registers a probe or attaches one.
# `\b...\s*\(` so `dp_tlm_probe_id (` and prose without a call do not count.
NAMING = re.compile(r"\bdp_tlm_probe\s*\(|\w+_set_telemetry\s*\(")
JOIN = "%s."


def _strip_comments(src: str) -> tuple[str, list[tuple[int, str]]]:
    """Blank out C comments and collect every string literal.

    Returns the source with comment characters replaced by spaces (newlines
    kept, so line numbers survive) and each string literal's line and raw
    text, escapes left as written. A small state machine rather than a regex,
    because `"//"` inside a string is not a comment and `\\"` inside one
    does not end it.
    """
    out: list[str] = []
    literals: list[tuple[int, str]] = []
    i, n, line = 0, len(src), 1
    while i < n:
        c = src[i]
        two = src[i : i + 2]
        if two == "/*":
            end = src.find("*/", i + 2)
            end = n if end < 0 else end + 2
            chunk = src[i:end]
            out.append("".join("\n" if ch == "\n" else " " for ch in chunk))
            line += chunk.count("\n")
            i = end
        elif two == "//":
            end = src.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i))
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            if c == '"':
                literals.append((line, src[i + 1 : j - 1]))
            out.append(src[i:j])
            line += src[i:j].count("\n")
            i = j
        else:
            out.append(c)
            line += c == "\n"
            i += 1
    return "".join(out), literals


def offenders(root: Path = ROOT) -> list[tuple[str, int, str]]:
    found: list[tuple[str, int, str]] = []
    for path in sorted((root / SCAN_DIR).rglob("*")):
        if path.suffix not in (".c", ".h"):
            continue
        rel = path.relative_to(root).as_posix()
        if rel == SANCTIONED:
            continue
        code, literals = _strip_comments(
            path.read_text(encoding="utf-8", errors="replace")
        )
        if not NAMING.search(code):
            continue
        for line, text in literals:
            if text.startswith(JOIN):
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
