#!/usr/bin/env python3
"""Refuse a wheel whose filename no CPython installer would ever select.

A wheel's filename IS its install contract: pip parses the
``<python>-<abi>-<platform>`` triple and installs the wheel only when one of
its tags is among the tags the running interpreter supports. A malformed
triple does not fail loudly anywhere -- the upload succeeds, PyPI lists the
file, and pip silently skips it and falls back to building the sdist.

That is exactly what shipped in v0.60.0 and v0.61.0 (doppler#1817)::

    doppler_dsp-0.61.0-cp313-cpwin_amd64-win_amd64.whl
    doppler_dsp-0.61.0-cp314-cpwin_amd64-win_amd64.whl

The ABI tag should be ``cp313``/``cp314``. A Windows user on 3.13 or 3.14
needed a C toolchain to install doppler, and nothing in the release noticed:
the post-release smoke installs only the 3.12 Windows wheel, and
``release-watch`` counted archives without reading a wheel name. The bad tag
comes from just-buildit's ``_abi_tag()`` (just-buildit/just-buildit#68); this
gate is doppler's side -- it cannot fix a name, only refuse to publish one.

How a wheel is judged
---------------------
Each name is parsed with :func:`packaging.utils.parse_wheel_filename` (the
same parser pip vendors), so a name pip cannot parse fails here too. Every
tag must name a CPython interpreter, ``cpXY``; from that tag the wheel's OWN
Python ``(X, Y)`` and platform are taken, and the wheel passes only when at
least one of its tags is in :func:`packaging.tags.cpython_tags` for that
version and platform -- i.e. some real CPython ``X.Y`` on that platform would
install it. The check therefore needs no list of expected names to keep in
step with the build matrix.

What it does not judge
----------------------
Whether the PLATFORM tag matches the machine that built it (a mislabelled
``manylinux`` wheel parses and passes) -- the per-platform smoke tests own
that. And a free-threaded ``cp313t`` wheel would be refused, since
``cpython_tags`` derives only the default ABI from a bare version; doppler
builds none, and the refusal is the safe direction.

Where the names come from
-------------------------
Positional arguments are wheel paths or bare filenames (``release.yml`` passes
the built ``dist/*.whl`` before ``publish-python``). ``--pypi DIST VERSION``
adds the filenames PyPI lists for that release and ``--release REPO TAG`` the
GitHub Release's assets (``make release-watch`` passes both, after the
release is verified). Zero wheels in total is a failure, never a pass: a
fetch that returned nothing is not evidence that every wheel is good.

Examples
--------
The two names from the issue, one shipped broken and one correct::

    $ python scripts/check_wheel_tags.py \\
    >     doppler_dsp-0.61.0-cp313-cpwin_amd64-win_amd64.whl
    FAIL doppler_dsp-0.61.0-cp313-cpwin_amd64-win_amd64.whl: no tag ...
    $ echo $?
    1
    $ python scripts/check_wheel_tags.py \\
    >     doppler_dsp-0.61.0-cp313-cp313-win_amd64.whl
    ok   doppler_dsp-0.61.0-cp313-cp313-win_amd64.whl  (cp313-cp313-win_amd64)
    1 wheel(s): 1 installable, 0 refused
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import urllib.request
from pathlib import PurePath

from packaging.tags import cpython_tags
from packaging.utils import InvalidWheelFilename, parse_wheel_filename

# `cp3` + minor. The interpreter tag spells the version without a dot, so
# cp313 is 3.13 and cp39 is 3.9 -- the major is always one digit for CPython 3.
_CPYTHON = re.compile(r"^cp(\d)(\d+)$")


def judge(name: str) -> tuple[bool, str]:
    """Return ``(installable, reason)`` for one wheel filename.

    Parameters
    ----------
    name : str
        A wheel's filename (a path is reduced to its basename by the caller).

    Returns
    -------
    tuple of (bool, str)
        Whether some CPython for the wheel's own version and platform would
        install it, and the accepted tag or the reason it was refused.

    Examples
    --------
    >>> judge("doppler_dsp-0.61.0-cp313-cpwin_amd64-win_amd64.whl")[0]
    False
    >>> judge("doppler_dsp-0.61.0-cp313-cp313-win_amd64.whl")
    (True, 'cp313-cp313-win_amd64')
    >>> judge("doppler_dsp-0.61.0-py3-none-any.whl")
    (False, "tag 'py3-none-any' is not a CPython (cpXY) tag")
    """
    try:
        _, _, _, tags = parse_wheel_filename(name)
    except InvalidWheelFilename as exc:
        return False, f"unparseable: {exc}"
    for tag in sorted(tags, key=str):
        m = _CPYTHON.match(tag.interpreter)
        if m is None:
            return False, f"tag '{tag}' is not a CPython (cpXY) tag"
        version = (int(m.group(1)), int(m.group(2)))
        if tag in set(
            cpython_tags(python_version=version, platforms=[tag.platform])
        ):
            return True, str(tag)
    return False, (
        "no tag is among cpython_tags() for its own Python -- pip would "
        "skip it and build the sdist: " + ", ".join(sorted(map(str, tags)))
    )


def pypi_wheels(dist: str, version: str) -> list[str]:
    """Filenames of the wheels PyPI lists for ``dist==version``."""
    url = f"https://pypi.org/pypi/{dist}/{version}/json"
    with urllib.request.urlopen(url, timeout=30) as resp:
        files = json.load(resp)["urls"]
    return [f["filename"] for f in files if f["filename"].endswith(".whl")]


def release_wheels(repo: str, tag: str) -> list[str]:
    """Filenames of the wheels attached to a GitHub Release."""
    out = subprocess.run(
        [
            "gh",
            "release",
            "view",
            tag,
            "--repo",
            repo,
            "--json",
            "assets",
            "--jq",
            ".assets[].name",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return [n for n in out.split() if n.endswith(".whl")]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("wheels", nargs="*", help="wheel paths or filenames")
    ap.add_argument(
        "--pypi",
        nargs=2,
        metavar=("DIST", "VERSION"),
        help="also check the wheels PyPI lists for a release",
    )
    ap.add_argument(
        "--release",
        nargs=2,
        metavar=("REPO", "TAG"),
        help="also check a GitHub Release's wheel assets",
    )
    args = ap.parse_args(argv)

    sources: list[tuple[str, list[str]]] = []
    if args.wheels:
        sources.append(("given", [PurePath(w).name for w in args.wheels]))
    if args.pypi:
        sources.append(
            (f"PyPI {args.pypi[0]}=={args.pypi[1]}", pypi_wheels(*args.pypi))
        )
    if args.release:
        sources.append(
            (
                f"GitHub Release {args.release[1]}",
                release_wheels(*args.release),
            )
        )

    total = refused = empty = 0
    for label, names in sources:
        if not names:
            print(f"FAIL {label}: no wheels found -- nothing was checked")
            empty += 1
            continue
        for name in names:
            ok, why = judge(name)
            total += 1
            if ok:
                print(f"ok   {name}  ({why})")
            else:
                refused += 1
                print(f"FAIL {name}: {why}")
    if not sources:
        print("FAIL no wheels given -- nothing was checked")
        return 1
    print(
        f"{total} wheel(s): {total - refused} installable, {refused} refused"
    )
    return 1 if refused or empty else 0


if __name__ == "__main__":
    sys.exit(main())
