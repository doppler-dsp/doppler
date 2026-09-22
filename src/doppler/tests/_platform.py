"""Where the test suite meets a platform that lacks something.

Every Windows skip in the Python suite comes from here, so the set is one
file to read and one grep to count. There are exactly two kinds, and they
cite different issues on purpose:

- **Absent by decision** (doppler#1364). The ``wfmgen`` CLI and the
  ``doppler.stream`` layer are POSIX-only builds, and the owner decided the
  Windows package ships without them rather than waiting on a port. Their
  markers key on the one declaration that says so, so a Linux or macOS
  build that LOST either still fails instead of skipping.
- **A test of something the platform does not have** (doppler#1465): a
  POSIX signal, a tty, ``resource`` limits, a bash gate script. The code
  under test is fine; the TEST's mechanism has no Windows counterpart. Each
  says which, and doppler#1465 lists them so the set can only shrink.

Examples
--------
>>> from doppler.tests import _platform
>>> _platform.requires_wfmgen.mark.name
'skipif'
"""

from __future__ import annotations

import signal
import sys

import pytest

from doppler.wfm import cli as _wfmgen_cli

__all__ = [
    "HARMLESS_SIGNAL",
    "WINDOWS",
    "posix_only",
    "requires_stream",
    "requires_wfmgen",
    "skip_module_without_stream",
    "skip_module_without_wfmgen",
    "skip_without_posix_shell",
]

WINDOWS = sys.platform == "win32"

#: The wfmgen CLI, keyed on ``doppler.wfm.cli.AVAILABLE`` -- the single
#: Python statement of where CMake builds it.
_WFMGEN_ABSENT = "the wfmgen CLI is not built on Windows (doppler#1364)"
requires_wfmgen = pytest.mark.skipif(
    not _wfmgen_cli.AVAILABLE, reason=_WFMGEN_ABSENT
)


def skip_module_without_wfmgen() -> None:
    """Skip the calling test MODULE where wfmgen is not built.

    For a module whose every test drives the CLI AND that imports something
    only such a platform has (``pty``, for a terminal guard) -- the import
    would fail at collection before any marker applied.
    """
    if not _wfmgen_cli.AVAILABLE:
        pytest.skip(_WFMGEN_ABSENT, allow_module_level=True)


#: ``doppler.stream`` (and ``wfm.StreamSink``): its extension is created
#: only where ``stream_core_obj`` exists, which is ``if(NOT WIN32)`` in
#: native/src/stream/CMakeLists.txt; ``[module.wfm_sink] platforms`` says
#: the same for the sink.
_STREAM_ABSENT = "doppler.stream (NATS) is not built on Windows (doppler#1364)"
requires_stream = pytest.mark.skipif(WINDOWS, reason=_STREAM_ABSENT)


def skip_module_without_stream() -> None:
    """Skip the calling test MODULE where ``doppler.stream`` is not built.

    For a module that imports ``doppler.stream`` at the top, where a marker
    is too late: the import itself fails at collection. Call it before that
    import. Keyed on the platform, not on the import failing, so a Linux or
    macOS build missing the extension still errors at collection.
    """
    if WINDOWS:
        pytest.skip(_STREAM_ABSENT, allow_module_level=True)


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
