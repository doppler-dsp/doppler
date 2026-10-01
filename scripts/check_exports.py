#!/usr/bin/env python3
"""Gate: each shared library exports exactly what the public headers promise.

``libdoppler.so`` exported every symbol with external linkage until
doppler#1164: 1658, among them all of vendored cJSON and PFFFT under their
own names. A program that linked its own cJSON then had the loader bind one
copy for both libraries, and doppler called the program's parser -- no error,
just the wrong code. ``libdoppler_stream.so`` exported nats.c the same way.

The build now writes each library's export list from the headers
(``cmake/export_list.cmake``). This checks the built result, in both
directions, per library:

- **nothing leaks** -- every symbol the library exports is one a header under
  ``native/inc`` publishes (``public_symbols()``, the parser
  ``check_installed_headers.py`` uses). A vendored symbol is named as such.
- **nothing is missing** -- every symbol a header publishes that the
  library's own archive defines (the same objects) is exported. Read from
  the headers, not from ``cmake/public-symbols.txt``: a list that went stale
  and dropped a function is exactly what this must catch.

Names beginning ``_`` are the toolchain's (``_init``, ``_fini``) and are not
judged, as in ``check_symbol_prefix.py``. ``nm`` failing, or reading nothing,
fails: absent output is not a pass.

Usage:  python3 scripts/check_exports.py [--public LIST]
        (``DOPPLER_BUILD_DIR`` points it at another build tree; ``--public``
        replaces the headers with a list, which is how its test seeds it.)
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from check_installed_headers import ROOT, public_symbols

#: shared library -> the archive built from the same objects.
LIBS = {
    "libdoppler.so": "libdoppler.a",
    "libdoppler_stream.so": "libdoppler_stream.a",
}
#: Vendored code's names, so a leak says what it is.
VENDORED = (
    "cJSON",
    "pffft",
    "cfft",
    "rfft",
    "make_",
    "destroy_",
    "nats",
    "js",
    "stan",
)
KINDS = frozenset("TDBRCWVGSiu")


def _nm(args: list[str], lib: pathlib.Path) -> set[str]:
    proc = subprocess.run(
        ["nm", *args, "--defined-only", str(lib)],
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise SystemExit(f"check-exports: nm {lib}: {proc.stderr}")
    out = {
        p[-1]
        for p in (ln.split() for ln in proc.stdout.splitlines())
        if len(p) >= 3 and p[-2] in KINDS
    }
    if not out:
        raise SystemExit(f"check-exports: no symbols read from {lib}")
    return out


def _public(path: pathlib.Path | None) -> set[str]:
    if path is None:
        return set(public_symbols())
    return {
        ln.split()[0]
        for ln in path.read_text(encoding="utf-8").splitlines()
        if ln and not ln.startswith("#")
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--public", type=pathlib.Path)
    args = ap.parse_args(argv)
    build = pathlib.Path(os.environ.get("DOPPLER_BUILD_DIR", ROOT / "build"))
    public = _public(args.public)
    found = [(so, ar) for so, ar in LIBS.items() if (build / so).is_file()]
    if not found:
        print(f"check-exports: no shared library under {build} -- run")
        print("  `make build`. This gate has not run, so it has not passed.")
        return 1
    bad = 0
    for so, ar in found:
        exported = {s for s in _nm(["-D"], build / so) if s[:1] != "_"}
        leaked = sorted(exported - public)
        missing = sorted((public & _nm(["-g"], build / ar)) - exported)
        for s in leaked:
            what = "vendored" if s.startswith(VENDORED) else "internal"
            print(
                f"  {so}: exports {s} ({what}) -- no public header declares it"
            )
        for s in missing:
            print(
                f"  {so}: does not export {s}, which a header publishes "
                "and the library defines"
            )
        if leaked or missing:
            bad += 1
        print(
            f"check-exports: {so}: {len(exported)} exported, "
            f"{len(leaked)} leaked, {len(missing)} missing"
        )
    if bad:
        print(
            "check-exports: FAIL -- the export list is written from "
            "cmake/public-symbols.txt; `make public-symbols`, then rebuild"
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
