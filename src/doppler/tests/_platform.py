"""Where the test suite meets a platform that lacks something.

Every Windows skip in the Python suite comes from here, so the set is one
file to read and one grep to count. There is one kind:

**A test of something the platform does not have** (doppler#1465): a POSIX
signal, a tty, ``resource`` limits, a bash gate script. The code under test is
fine; the TEST's mechanism has no Windows counterpart. Each says which, and
doppler#1465 lists them so the set can only shrink.

There used to be a second kind, "absent by decision" (doppler#1364): the
``wfmgen`` CLI and ``doppler.stream`` were not built on Windows. Both are now
(#1575 and its follow-up), so nothing is absent by decision any more.

Examples
--------
>>> from doppler.tests import _platform
>>> _platform.posix_only("needs a tty").mark.name
'skipif'
"""

from __future__ import annotations

import signal
import sys

import pytest

__all__ = [
    "HARMLESS_SIGNAL",
    "WINDOWS",
    "posix_only",
    "skip_module_posix_only",
    "skip_without_posix_shell",
]

WINDOWS = sys.platform == "win32"

#: A signal a test may arm and raise without side effects. POSIX has
#: SIGUSR1 for exactly this; on Windows doppler maps only SIGINT, SIGBREAK
#: and SIGTERM (dp_win_sig_map, native/src/dp_interrupt.c), and SIGBREAK is
#: the one no runner or developer sends by accident.
HARMLESS_SIGNAL: int = (
    signal.SIGBREAK if WINDOWS else signal.SIGUSR1  # type: ignore[attr-defined]
)


def posix_only(why: str) -> pytest.MarkDecorator:
    """Skip on Windows because the TEST needs something POSIX-only.

    Parameters
    ----------
    why : str
        The missing mechanism, said plainly (``"raises SIGUSR1"``). It is
        the skip reason a Windows log prints, next to doppler#1465.

    Returns
    -------
    pytest.MarkDecorator
        Usable on a test, a class, or as a module's ``pytestmark``.

    Examples
    --------
    >>> from doppler.tests._platform import posix_only
    >>> "doppler#1465" in posix_only("needs a tty").kwargs["reason"]
    True
    """
    return pytest.mark.skipif(
        WINDOWS, reason=f"POSIX-only: {why} (doppler#1465)"
    )


def skip_without_posix_shell(what: str) -> None:
    """Skip the running test, on Windows, as it is about to exec a shell gate.

    Called from a test module's own run-the-script helper rather than put on
    the module, so the tests in that file that never exec the script -- "does
    `make lint` reach the gate", the Python half of a gate -- still run.

    Why these are carve-outs rather than fixes: the scripts are doppler's lint
    gates, and their execution home is ``make lint`` on Linux CI. On Windows a
    bare ``bash`` resolves to ``C:\\Windows\\System32\\bash.exe``, the WSL
    launcher, ahead of PATH, and a ``.sh`` cannot be exec'd at all.

    Parameters
    ----------
    what : str
        The gate being exec'd, for the skip reason.
    """
    if WINDOWS:
        pytest.skip(
            f"POSIX-only: execs {what}, a bash gate whose home is make lint "
            "on Linux CI (doppler#1465)"
        )


def skip_module_posix_only(why: str) -> None:
    """Skip the calling test MODULE on Windows, for the reason ``why``.

    :func:`posix_only` for a module that IMPORTS what Windows lacks (``pty``,
    ``resource``): the import fails at collection, before any marker applies.
    Call it above that import.

    Parameters
    ----------
    why : str
        The missing mechanism, as for :func:`posix_only`.

    Examples
    --------
    >>> from doppler.tests._platform import skip_module_posix_only
    >>> skip_module_posix_only("needs a pty") if not WINDOWS else None
    """
    if WINDOWS:
        pytest.skip(
            f"POSIX-only: {why} (doppler#1465)", allow_module_level=True
        )
