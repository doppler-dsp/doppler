#!/usr/bin/env python3
"""Generate tests/install/build-*-deps.sh from jb.toml.

jb.toml's ``[dev.*]`` package lists are the single source of truth for
doppler's system dependencies -- ``make install-deps``, CI, and both
Dockerfiles already read them via just-bashit. The per-distro install
scripts under ``tests/install/`` (rendered into ``docs/install/source.md``
and ``docs/quickstart.md`` via ``--8<--`` snippet includes) used to be a
hand-maintained second copy of the same lists, and they drifted -- the
dnf/zypper scripts once carried a ``gcc-c++`` that ``jb.toml`` never
listed, and the brew script disagreed with ``[dev.brew]`` in both
directions. This generator projects the scripts from jb.toml, so the
docs can only ever show what ``make install-deps`` actually installs.

Two dev packages are deliberately excluded from the docs projection
(``DOCS_EXCLUDE``):

* ``patchelf`` -- only the ``pip install .`` wheel build+repair path
  (auditwheel) touches it; ``make``/``make pyext``, the flow these docs
  describe, never does.
* ``rust`` -- only ``make rust-test`` needs a Rust toolchain; the
  build-from-source walkthrough doesn't run the Rust FFI tests.

Usage
-----
    python scripts/gen_install_scripts.py --write   # regenerate scripts
    python scripts/gen_install_scripts.py --check   # exit 1 on any drift
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import tomllib

ROOT = Path(__file__).resolve().parent.parent
JB_TOML = ROOT / "jb.toml"
OUT_DIR = ROOT / "tests" / "install"

# Packages present in jb.toml's dev groups that the build-from-source
# docs deliberately don't show -- see the module docstring for why each.
DOCS_EXCLUDE = frozenset({"patchelf", "rust"})

# manager -> (script filename, audience comment, install-command prefix).
# msys2 is absent on purpose: doppler doesn't target Windows natively;
# the docs route Windows users to WSL2 + the apt path.
MANAGERS: dict[str, tuple[str, str, str]] = {
    "apt": (
        "build-apt-deps.sh",
        "# Ubuntu, Debian, and derivatives (Mint, Pop!_OS, ...).",
        "sudo apt-get install",
    ),
    "pacman": (
        "build-pacman-deps.sh",
        "# Arch Linux and derivatives (Manjaro, EndeavourOS, CachyOS, ...).",
        "sudo pacman -S --needed",
    ),
    "dnf": (
        "build-dnf-deps.sh",
        "# Fedora, RHEL, CentOS Stream, Rocky, AlmaLinux.",
        "sudo dnf install",
    ),
    "zypper": (
        "build-zypper-deps.sh",
        "# openSUSE Leap and Tumbleweed.",
        "sudo zypper install",
    ),
    "brew": (
        "build-brew-deps.sh",
        "# macOS (Homebrew).",
        "brew install",
    ),
}

WIDTH = 79


def _wrap_command(prefix: str, packages: list[str]) -> str:
    """Lay out ``prefix pkg pkg ...`` greedily wrapped at WIDTH columns,
    with a trailing backslash on every line but the last and a two-space
    continuation indent -- the shape the hand-written scripts always used.
    """
    lines: list[str] = []
    current = prefix
    for pkg in packages:
        candidate = f"{current} {pkg}"
        # ' \' (2 chars) must still fit whenever another line follows;
        # being conservative and always reserving it keeps this simple.
        if len(candidate) + 2 > WIDTH and current not in (prefix, "  "):
            lines.append(current + " \\")
            current = f"  {pkg}"
        else:
            current = candidate
    lines.append(current)
    return "\n".join(lines)


def render(manager: str, packages: list[str]) -> str:
    _filename, comment, prefix = MANAGERS[manager]
    shown = [p for p in packages if p not in DOCS_EXCLUDE]
    command = _wrap_command(prefix, shown)
    return (
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "\n"
        "# Generated from jb.toml by scripts/gen_install_scripts.py -- do\n"
        "# not edit; change jb.toml's [dev.*] packages and run\n"
        "# `make docs-relink`.\n"
        f"{comment}\n"
        "# --8<-- [start:install]\n"
        f"{command}\n"
        "# --8<-- [end:install]\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--write", action="store_true", help="regenerate the scripts"
    )
    group.add_argument(
        "--check",
        action="store_true",
        help="exit 1 on any drift, write nothing",
    )
    args = parser.parse_args()

    with open(JB_TOML, "rb") as f:
        jb = tomllib.load(f)

    stale: list[str] = []
    for manager, (filename, _, _) in MANAGERS.items():
        try:
            packages = jb["dev"][manager]["packages"]
        except KeyError:
            raise SystemExit(
                f"gen_install_scripts: jb.toml has no [dev.{manager}] "
                f"packages -- MANAGERS and jb.toml are out of step."
            ) from None
        rendered = render(manager, packages)
        path = OUT_DIR / filename
        on_disk = path.read_text() if path.exists() else ""
        if rendered != on_disk:
            stale.append(filename)
            if args.write:
                path.write_text(rendered)

    if stale:
        if args.check:
            print(
                "install scripts out of sync with jb.toml: "
                + ", ".join(stale)
                + " -- run: make docs-relink",
                file=sys.stderr,
            )
            return 1
        print(f"regenerated from jb.toml: {', '.join(stale)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
