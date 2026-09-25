"""Find the repo root by walking up, not by counting directories.

``Path(__file__).parents[N]`` is correct exactly once: from the source
checkout. The coverage harness copies this package to ``build-cov/pkg/doppler``
— two levels deeper — so every fixed-depth root landed inside ``build-cov/``,
and every test reading ``docs/``, ``scripts/`` or ``native/`` failed there.
That was **89 results** (27 failed, 62 errors), tolerated only because
``COVERAGE_CMD`` ignored pytest's exit code.

Walking up for a marker is depth-independent, so one expression is right from
both trees. ``build-cov/`` lives *inside* the repo, so the walk finds the real
root and those tests **run** under coverage and contribute to the number,
rather than being skipped — which is why this is a better answer than marking
them "needs a checkout" and excluding them.

Both markers are required together: ``pyproject.toml`` alone would match any
parent that merely happens to be a Python project, which on a developer's
machine is a real possibility.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

__all__ = ["build_dir", "exe", "header_root", "repo_root"]


def repo_root(start: Path | str | None = None) -> Path:
    """Return the doppler checkout root containing ``start``.

    Parameters
    ----------
    start : Path or str, optional
        Any path inside the tree; defaults to this file. A file or a
        directory both work — the walk begins at its parents either way.

    Returns
    -------
    Path
        The directory holding both ``pyproject.toml`` and ``native/``.

    Raises
    ------
    RuntimeError
        When no ancestor carries both markers. That means the caller is not
        running from a checkout at all, which for the tests using this is a
        real failure rather than something to paper over with a default.

    Examples
    --------
    >>> from doppler.tests._repo import repo_root
    >>> (repo_root() / "pyproject.toml").is_file()
    True
    """
    here = (
        Path(start).resolve()
        if start is not None
        else Path(__file__).resolve()
    )
    for parent in (here, *here.parents):
        if (parent / "pyproject.toml").is_file() and (
            parent / "native"
        ).is_dir():
            return parent
    raise RuntimeError(
        f"no doppler checkout above {here} — looked for a directory with "
        "both pyproject.toml and native/"
    )


def build_dir(start: Path | str | None = None) -> Path:
    """Return the CMake build tree the tests should read artifacts from.

    ``$DOPPLER_BUILD_DIR`` wins when set; otherwise ``<repo>/build``. The
    variable is not new here — ``ffi/rust/build.rs`` has always read it, and
    ``COVERAGE_CMD`` already exported it for the cargo leg. This makes the
    Python gates honour the same one instead of each spelling ``build/``
    itself.

    Why it matters: the coverage job builds ONLY ``build-cov``. Three gates
    hard-coded ``build/`` — the C doc-snippet compiler and the conv/rs
    certify harnesses — so on a runner they failed with "not built, run
    `make build` first", while on a developer's box the ordinary build tree
    sat there and they passed. That divergence was invisible for as long as
    the recipe discarded pytest's exit code.

    Parameters
    ----------
    start : Path or str, optional
        Any path inside the tree, passed through to :func:`repo_root` when
        the environment does not name a build directory.

    Returns
    -------
    Path
        The build tree. It is NOT required to exist — a gate that needs an
        artifact from it says so itself, naming the file it wanted.

    Examples
    --------
    >>> from doppler.tests._repo import build_dir, repo_root
    >>> build_dir() == repo_root() / "build"  # with DOPPLER_BUILD_DIR unset
    True
    """
    env = os.environ.get("DOPPLER_BUILD_DIR")
    if env:
        return Path(env)
    return repo_root(start) / "build"


def exe(path: Path | str) -> Path:
    """A built executable's path, with this platform's suffix.

    CMake names an executable ``<target>`` on POSIX and ``<target>.exe`` on
    Windows. A gate that spells the bare name finds nothing on Windows and
    reports "not built" about a binary sitting right there -- which is what
    every validator did on the first Windows run of the suite.

    Parameters
    ----------
    path : Path or str
        The executable as CMake names the TARGET, without any suffix.

    Returns
    -------
    Path
        ``path`` on POSIX, ``path.exe`` on Windows.

    Examples
    --------
    >>> import sys
    >>> from doppler.tests._repo import exe
    >>> exe("build/validate_rs").name == (
    ...     "validate_rs.exe" if sys.platform == "win32" else "validate_rs"
    ... )
    True
    """
    p = Path(path)
    return p.with_name(p.name + ".exe") if sys.platform == "win32" else p


def header_root(start: Path | str | None = None) -> Path:
    """Return the directory doppler's C headers live in (jm schema 8).

    The layout is answered once, by ``scripts/_layout.py``, which the gate
    scripts import; loading that file here gives the tests the same answer
    rather than a second spelling of ``native/inc/doppler``.

    Returns
    -------
    Path
        ``<root>/native/inc/<pkg>``.
    """
    import importlib.util

    root = repo_root(start)
    spec = importlib.util.spec_from_file_location(
        "_layout", root / "scripts" / "_layout.py"
    )
    assert spec is not None and spec.loader is not None
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    return root / layout.HEADER_ROOT
