#!/usr/bin/env python3
"""List the ``<mod>_ext.c`` files that are hand-written, one per line.

``make lint-clang-format`` drops every ``_ext.c`` because a module's
aggregator is jm-generated and jm formats its own output. Some are not:
a ``no_generate = "true"`` module's binding is written by hand, so that
exclusion left it formatted by nobody -- until ``jm apply``, whose
``c_style`` pass walks all of ``native/src`` and rewrote it under whoever
ran it next (``buffer_ext.c``, after doppler#1432).

Derived from ``just-makeit.toml`` rather than listed: the day a module is
migrated its ``no_generate`` key goes, and it leaves this set by itself.

Examples
--------
    $ python scripts/list_hand_ext_c.py
    native/src/stream/stream_ext.c
"""

import pathlib

import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> None:
    manifest = tomllib.loads((ROOT / "just-makeit.toml").read_text())
    for name, mod in sorted(manifest.get("module", {}).items()):
        if mod.get("no_generate") != "true":
            continue
        path = pathlib.Path("native/src") / name / f"{name}_ext.c"
        if (ROOT / path).exists():
            print(path)


if __name__ == "__main__":
    main()
