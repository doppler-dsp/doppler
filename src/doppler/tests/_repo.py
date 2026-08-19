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

from pathlib import Path

__all__ = ["repo_root"]


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
