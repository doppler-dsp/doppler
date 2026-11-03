#!/usr/bin/env python3
"""List the ``<mod>_ext.c`` files that are hand-written, one per line.

``make lint-clang-format`` drops every ``_ext.c`` because a module's
aggregator is jm-generated and jm formats its own output. Some are not:
a ``no_generate = "true"`` module's binding is written by hand, so that
exclusion left it formatted by nobody -- until ``jm apply``, whose
``c_style`` pass walks all of ``native/src`` and rewrote it under whoever
ran it next (``buffer_ext.c``, after doppler#1432).

Derived from the jm manifest -- ``just-makeit.toml`` and the
``modules/<name>.toml`` fragments it includes -- rather than listed: the day
a module is migrated its ``no_generate`` key goes, and it leaves this set by
itself. A manifest that yields no module at all is refused (exit 1): an
empty list is a correct answer once every binding is generated, but no
modules means the read failed, and an empty list then would leave every
hand-written ``_ext.c`` unformatted while lint stayed green.

Examples
--------
    $ python scripts/list_hand_ext_c.py
    native/src/stream/stream_ext.c
"""

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> None:
    # jm's public reader: a `[module.X]` lives in `modules/<name>.toml`,
    # which a parse of the root file alone would not see.
    from just_makeit import load_manifest

    try:
        modules = load_manifest(ROOT).get("module", {})
    except (OSError, ValueError) as e:
        raise SystemExit(f"list_hand_ext_c: the manifest: {e}") from e
    if not modules:
        raise SystemExit(
            "list_hand_ext_c: the manifest yields no [module.X] table; "
            "refusing to report an empty list"
        )
    for name, mod in sorted(modules.items()):
        if mod.get("no_generate") != "true":
            continue
        path = pathlib.Path("native/src") / name / f"{name}_ext.c"
        if (ROOT / path).exists():
            # POSIX separators on every OS: the list is read by make's shell
            # and xargs, which eat a Windows backslash as an escape, so
            # `native\src\x\x_ext.c` would reach clang-format as a file
            # that does not exist (#2365's Windows leg).
            print(path.as_posix())


if __name__ == "__main__":
    main()
