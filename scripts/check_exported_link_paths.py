#!/usr/bin/env python3
"""Gate: an exported CMake link interface names no absolute path.

doppler installs a CMake package, and ``doppler::doppler-static`` carries its
PUBLIC link interface into the installed ``doppler-targets.cmake``. Whatever
is written there is resolved on the CONSUMER's machine. A library name (``m``,
``Threads::Threads``) resolves there; an absolute path is the build machine's
and usually does not.

v0.51.0 shipped ``INTERFACE_LINK_LIBRARIES "/usr/lib64/libm.so;..."`` -- where
the manylinux build container keeps libm, found by ``find_library`` -- so
every CMake consumer of the static library on Debian or Ubuntu failed with
``No rule to make target '/usr/lib64/libm.so'``. v0.50.0 exported ``m``.

Nothing before publishing could see it: a consumer built on the build machine
links, because the path exists there. Only the post-release smoke, on a
different distro, went red -- after the tarball was public. The defect is in
the TEXT of the export file, so the gate reads the text.

A path under ``${_IMPORT_PREFIX}`` is the package's own file, relocated at
the consumer, and is fine. Anything else starting with ``/`` or a drive letter
is refused.

Usage:  python3 scripts/check_exported_link_paths.py <dir>
        <dir> is a build tree (reads CMakeFiles/Export/) or an install prefix
        (reads lib/cmake/). Exit 1 if an absolute path is exported, or if no
        export file is found -- a gate with nothing to read has not passed.

Standard library only: it runs in C-only jobs that never set up uv.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

LINK_PROP = re.compile(r'INTERFACE_LINK_LIBRARIES\s+"([^"]*)"')
ABSOLUTE = re.compile(r"^(/|[A-Za-z]:[/\\])")


def export_files(root: Path) -> list[Path]:
    """Every exported-targets file under a build tree or install prefix."""
    found = sorted(root.glob("CMakeFiles/Export/**/*.cmake"))
    found += sorted(root.glob("lib*/cmake/**/*.cmake"))
    return found


def offenders(text: str) -> list[str]:
    """The absolute-path items in every INTERFACE_LINK_LIBRARIES value."""
    bad = []
    for value in LINK_PROP.findall(text):
        for item in value.split(";"):
            item = item.strip()
            if ABSOLUTE.match(item):
                bad.append(item)
    return bad


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__.strip().split("\n\n")[-2], file=sys.stderr)
        return 2
    root = Path(argv[1])
    files = export_files(root)
    if not files:
        print(
            f"exported-link-check: no export file under {root} -- this gate"
            " has not run, so it has not passed. Configure (or install) first."
        )
        return 1
    bad = []
    for f in files:
        for item in offenders(f.read_text(errors="replace")):
            bad.append((f, item))
    if bad:
        print("exported-link-check: FAIL — an exported link interface names")
        print("  an absolute path, which only exists on the build machine:")
        for f, item in bad:
            print(f"  {f}: {item}")
        print(
            "  Link a library by NAME (e.g. `m`), not by the path find_library"
            "\n  returned: the name resolves on the consumer's machine."
        )
        return 1
    print(
        f"exported-link-check: OK — {len(files)} export file(s), no absolute"
        " path in any link interface"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
