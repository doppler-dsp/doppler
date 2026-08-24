#!/usr/bin/env python3
"""Every function an installed header declares must exist in a shipped library.

`install(DIRECTORY native/inc/ … PATTERN "*.h")` takes the whole tree, so a
header in `native/inc/` is a **published C API** whether or not anything
implements it. Two of them declared an API the library did not define:
`telemetry/tlm_recorder.h` (seven functions, superseded by `dp_tlm_capture`
and never built) and an empty `stream/stream_core.h` scaffold. Both were
installed and both had a Doxygen page, so a downstream could read them,
include them, and fail at link time (doppler#801).

That is the worst possible first experience of a library, and it is
mechanically detectable, which is why this exists rather than a note asking
people to remember.

Absolute, not a ratchet
-----------------------
There is no allowlist. The two originals are deleted, so the correct count
is zero and a ratchet would only be somewhere for a third to hide. If a
declaration legitimately has no definition in these archives — a header for
a component built only under an option, say — the honest fix is to stop
installing that header, not to list it here.

Where a symbol may live
-----------------------
`libdoppler.a` and the optional `libdoppler_stream.a`, which are exactly the
two archives the package installs and exports. A symbol that resolves only
inside a Python extension module is NOT a C API and does not count.

A second question of the same file set
--------------------------------------
No installed header may define a **feature-test macro** (`_GNU_SOURCE`,
`_POSIX_C_SOURCE`, `_DARWIN_C_SOURCE`, …). glibc's `features.h` latches the
feature set on its FIRST inclusion, so such a `#define` is a no-op for every
translation unit that reached libc before including the header -- which is
most of them. `buffer/buffer.h` carried one at its own line 51 and it was
inert in exactly that way, measured directly: after `#include <stdio.h>` then
`#include "buffer/buffer.h"`, `__USE_GNU` is not defined (doppler#986).

It read as though the header had handled the problem, so the failure was
attributed elsewhere: the phase-6 telemetry work was reverted because adding
an include ABOVE that line stopped `syscall` resolving.

The macros belong on the compile line, and the top-level CMakeLists.txt puts
them there -- on the exported target too, so a downstream inherits them.

This lives here rather than in a gate of its own because it is the same
question of the same files: what does a published header oblige a consumer
to do? Nothing it cannot see.
"""

from __future__ import annotations

import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INC = ROOT / "native" / "inc"

#: The install rule's own exclusion, restated here because it is the one
#: header in the tree that is deliberately not published.
NOT_INSTALLED = {"pyex_common.h"}

#: A declaration, once the text has been reduced to file-scope statements.
#: The specifier run is captured so `static` / `inline` can be rejected: those
#: travel in the header and every TU gets its own copy, so they are not a
#: published ABI and `nm` will never see them.
_DECL = re.compile(
    r"^\s*(?P<spec>[A-Za-z_][\w\s\*]*?)(?P<name>\w+)\s*\((?P<args>[^()]*)\)\s*$",
    re.S,
)

_KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "typedef",
    "else",
    "do",
}
_NOT_ABI = {"static", "inline", "typedef", "JM_FORCEINLINE"}


def _blank_comments(text: str) -> str:
    """Replace comments with the same number of newlines.

    Collapsing them instead -- which the first draft did -- costs the line
    numbers AND turns doxygen prose into parseable text: `@param pfa
    Whole-search false-alarm probability` reads as a declaration of `pfa`,
    and six of the first run's twenty-two findings were sentences.
    """

    def keep_lines(m: re.Match[str]) -> str:
        return "\n" * m.group(0).count("\n")

    text = re.sub(r"/\*.*?\*/", keep_lines, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _blank_preproc(text: str) -> str:
    """Drop preprocessor lines, keeping the line count."""
    return "\n".join(
        "" if ln.lstrip().startswith("#") else ln for ln in text.split("\n")
    )


def _file_scope(text: str):
    """Yield ``(statement, line)`` for every `;` statement at file scope.

    Brace depth is what keeps a function BODY out: a call inside one looks
    exactly like a declaration to a regex, which is where `conjf`,
    `PyLong_FromSize_t` and `_mm_cvtss_f32` came from. `extern "C" {` is
    treated as depth-neutral, since everything real is inside one.
    """
    text = re.sub(r'extern\s+"C"\s*\{', "        ", text)
    depth, start = 0, 0
    for i, ch in enumerate(text):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth = max(0, depth - 1)
        elif ch == ";" and depth == 0:
            yield text[start:i], text.count("\n", 0, start) + 1
            start = i + 1
        if ch in "{};" and depth == 0 and ch != ";":
            start = i + 1


def archives() -> list[pathlib.Path]:
    build = pathlib.Path(os.environ.get("DOPPLER_BUILD_DIR", ROOT / "build"))
    return [
        p
        for p in (build / "libdoppler.a", build / "libdoppler_stream.a")
        if p.is_file()
    ]


def defined_symbols(libs: list[pathlib.Path]) -> set[str]:
    out: set[str] = set()
    for lib in libs:
        proc = subprocess.run(
            ["nm", "-g", "--defined-only", str(lib)],
            capture_output=True,
            text=True,
        )
        for line in proc.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-2] in ("T", "t", "D", "B", "R", "W"):
                out.add(parts[-1].lstrip("_"))
    return out


def declared() -> dict[str, list[tuple[pathlib.Path, int]]]:
    """function name -> [(header, line), …] over every installed header."""
    out: dict[str, list[tuple[pathlib.Path, int]]] = {}
    for header in sorted(INC.rglob("*.h")):
        if header.name in NOT_INSTALLED:
            continue
        text = header.read_text(encoding="utf-8", errors="replace")
        text = _blank_preproc(_blank_comments(text))
        for stmt, line in _file_scope(text):
            m = _DECL.match(stmt)
            if m is None:
                continue
            name = m.group("name")
            if name in _KEYWORDS:
                continue
            if set(m.group("spec").split()) & _NOT_ABI:
                continue
            out.setdefault(name, []).append((header, line))
    return out


#: Defining any of these in an installed header is a no-op for most callers
#: (see the module docstring). Matched on the `#define` line, so a header may
#: still TEST one with `#ifdef`.
_FEATURE_TEST = re.compile(
    r"^\s*#\s*define\s+(_GNU_SOURCE|_DEFAULT_SOURCE|_BSD_SOURCE"
    r"|_SVID_SOURCE|_POSIX_C_SOURCE|_POSIX_SOURCE|_XOPEN_SOURCE"
    r"|_XOPEN_SOURCE_EXTENDED|_DARWIN_C_SOURCE|_ISOC99_SOURCE"
    r"|_ISOC11_SOURCE|_FILE_OFFSET_BITS|_LARGEFILE64_SOURCE)\b",
    re.M,
)


def feature_test_defines() -> list[tuple[pathlib.Path, int, str]]:
    """(header, line, macro) for every feature-test macro defined in one.

    Discovered over the installed tree, so a header that grows one is caught
    when it is written rather than when somebody's include order changes.
    There is no allowlist, for the same reason the declaration check has
    none: the correct count is zero, and a list is only somewhere for the
    next one to hide.
    """
    out = []
    for header in sorted(INC.rglob("*.h")):
        if header.name in NOT_INSTALLED:
            continue
        # Count lines in the BLANKED text, not the original: the offsets
        # come from the blanked one, and _blank_comments preserves newline
        # counts precisely so the two agree line-for-line. Mixing them
        # reported dp_state.h:2 for a define on line 35.
        blanked = _blank_comments(
            header.read_text(encoding="utf-8", errors="replace")
        )
        for m in _FEATURE_TEST.finditer(blanked):
            line = blanked.count("\n", 0, m.start()) + 1
            out.append((header, line, m.group(1)))
    return out


def main() -> int:
    bad_macros = feature_test_defines()
    if bad_macros:
        print("check_installed_headers: installed header(s) define a")
        print("  feature-test macro. It is a no-op for any translation unit")
        print("  that reached libc first, so it does not do what it looks")
        print("  like it does (#986).\n")
        for header, line, macro in bad_macros:
            print(f"  {header.relative_to(ROOT)}:{line}: {macro}")
        print("\n  Define it on the compile line instead — the top-level")
        print("  CMakeLists.txt does, on the exported target too, so a")
        print("  downstream inherits it. There is deliberately no allowlist.")
        return 1

    libs = archives()
    if not libs:
        print("check_installed_headers: no libdoppler.a under")
        print(f"  {os.environ.get('DOPPLER_BUILD_DIR', 'build/')} — build")
        print("  first (`make build`). This gate has not run, so it has not")
        print("  passed.")
        return 1

    have = defined_symbols(libs)
    decls = declared()
    missing = {n: w for n, w in decls.items() if n not in have}

    if missing:
        print("check_installed_headers: installed header(s) declare a")
        print("  function the shipped libraries do not define. A downstream")
        print("  can read it, include it, and fail at link time (#801).\n")
        for name in sorted(missing):
            for header, line in missing[name]:
                rel = header.relative_to(ROOT)
                print(f"  {rel}:{line}: {name}")
        print(
            f"\n  {len(missing)} undefined declaration(s) across "
            f"{len(decls)} checked, against "
            f"{', '.join(p.name for p in libs)}."
        )
        print("  Either implement it, or stop installing the header —")
        print("  there is deliberately no allowlist.")
        return 1

    print(
        f"check_installed_headers: OK — {len(decls)} declaration(s) across "
        f"{len(list(INC.rglob('*.h')))} installed header(s) all resolve in "
        f"{', '.join(p.name for p in libs)}, and none defines a "
        f"feature-test macro"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
