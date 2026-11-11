"""Every package's module docstring runs its examples (#3000).

A package's docstring is its ``[module.<name>] doc`` in ``modules/*.toml``,
which jm writes into ``__init__.py``. It is the overview at the top of the
package's generated reference page, so its ``Examples`` are what a reader
copies first. No gate ran them: the ``.pyi`` stubs that ``make test-stubs``
doctests carry no module docstring. Three had already gone stale when this
was written (``acquire`` called a property; ``analyzer`` passed a keyword
``Specan`` no longer takes and claimed the wrong output length).

Discovered, not registered: every subpackage of ``doppler`` is collected,
so a new one is held the day it exists.
"""

from __future__ import annotations

import doctest
import importlib
import pkgutil

import pytest

import doppler

#: Packages that are not library API: the test tree and the example scripts.
NOT_API = {"tests", "examples"}

PACKAGES = sorted(
    m.name
    for m in pkgutil.iter_modules(doppler.__path__)
    if m.ispkg and m.name not in NOT_API
)


@pytest.mark.parametrize("name", PACKAGES)
def test_the_module_docstring_examples_run(name: str) -> None:
    mod = importlib.import_module(f"doppler.{name}")
    test = doctest.DocTestParser().get_doctest(
        mod.__doc__ or "", {}, f"doppler.{name}", None, 0
    )
    runner = doctest.DocTestRunner(
        optionflags=doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE
    )
    failures: list[str] = []
    runner.run(test, out=failures.append)
    assert runner.failures == 0, "".join(failures)


def test_the_packages_are_discovered() -> None:
    """A collection that finds nothing passes vacuously; this one cannot."""
    assert {"snr", "mpsk", "interp", "dsss"} <= set(PACKAGES)
