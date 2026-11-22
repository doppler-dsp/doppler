#!/usr/bin/env python3
"""Fail when release.yml names a floating runner image.

``ubuntu-latest`` and ``windows-latest`` are labels GitHub re-points when a
new image is promoted. The v0.84.0 release ran its x86_64 PyPI smoke on
``ubuntu-latest``, which had rolled from 24.04 (v0.83.0's run) to 26.04 eight
days earlier. ``actions/setup-python`` publishes no 3.9 build for 26.04, so
the py3.9 leg failed before anything was installed -- after the wheels had
been published, so ``make ship`` exited non-zero for a release that had
worked (#3188). A release is the one place an image roll must not be able to
reach: it cannot be re-run on the old image, and PyPI is immutable.

So release.yml pins every runner (``ubuntu-24.04``, ``windows-2025-vs2026``,
``macos-15``), including matrix entries and ``${{ }}`` expressions that carry
a label. ``ci.yml`` stays on ``-latest`` ON PURPOSE: there a roll shows up on
a pull request, which is the early warning this file does not get.

Usage
-----
::

    python scripts/check_release_runners.py           # release.yml
    python scripts/check_release_runners.py FILE ...  # named files
"""

from __future__ import annotations

import pathlib
import re
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
RELEASE = ROOT / ".github" / "workflows" / "release.yml"
FLOATING = re.compile(r"\b(?:ubuntu|windows|macos)-latest\b")


def _scalars(node, path=""):
    """Every string in a parsed workflow, with where it sits."""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from _scalars(v, f"{path}/{k}")
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from _scalars(v, f"{path}[{i}]")
    elif isinstance(node, str):
        yield path, node


def check(file: pathlib.Path) -> list[str]:
    doc = yaml.safe_load(file.read_text(encoding="utf-8"))
    out = []
    for where, text in _scalars(doc):
        for m in FLOATING.finditer(text):
            out.append(f"{file.name}: {where}: `{m.group(0)}`")
    return out


def main(argv: list[str]) -> int:
    files = [pathlib.Path(a) for a in argv] or [RELEASE]
    bad = [line for f in files for line in check(f)]
    if not bad:
        print(
            f"check_release_runners: OK -- {len(files)} file(s), "
            "no floating runner"
        )
        return 0
    print(
        "check_release_runners: a release workflow names a floating "
        "runner -- FAIL"
    )
    for line in bad:
        print(f"  {line}")
    print(
        "\n  `-latest` is re-pointed when GitHub promotes an image, and a\n"
        "  release cannot be re-run on the old one (v0.84.0's py3.9\n"
        "  smoke, #3188). Pin the image: ubuntu-24.04,\n"
        "  windows-2025-vs2026, macos-15. ci.yml stays on -latest on\n"
        "  purpose: a roll shows up on a pull request there, which is the\n"
        "  early warning release.yml does not get."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
