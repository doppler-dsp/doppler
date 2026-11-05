"""The one place an example script says it needs an optional package.

The example scripts draw with matplotlib and some design filters with scipy,
neither of which is a dependency of the library. Each script calls
:func:`require` before its first import of either, so a reader who installed
the wheel and ran ``python -m doppler.examples.<name>`` is told exactly what to
install instead of meeting a bare ``ModuleNotFoundError`` (doppler#2857).

The leading underscore keeps this file out of every example gate, which skips
``_*.py``: it is a helper, not a script anyone runs.
"""

from __future__ import annotations

import importlib

#: The command a missing package is answered with.
INSTALL = "pip install 'doppler-dsp[examples]'"


def require(package: str) -> None:
    """Exit with a one-line instruction if ``package`` cannot be imported.

    Parameters
    ----------
    package : str
        The importable name, ``"matplotlib"`` or ``"scipy"``.

    Raises
    ------
    SystemExit
        With the message ``needs <package>: pip install
        'doppler-dsp[examples]'``, exit status 1.

    Examples
    --------
    >>> from doppler.examples._extra import require
    >>> require("math")  # importable: returns quietly
    >>> require("no_such_package_xyz")
    Traceback (most recent call last):
        ...
    SystemExit: needs no_such_package_xyz: pip install 'doppler-dsp[examples]'
    """
    try:
        importlib.import_module(package)
    except ImportError as e:
        raise SystemExit(f"needs {package}: {INSTALL}") from e
