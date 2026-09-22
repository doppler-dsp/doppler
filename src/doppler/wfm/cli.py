"""``wfmgen`` console-script — a thin shim over the bundled C binary.

The waveform tool is implemented exactly once, in C: ``wfmgen`` (the composer
superset; ``native/src/app/wfmgen.c``). There is **no** second CLI in Python.
This entry point locates the ``wfmgen`` binary the wheel ships as package data
(``doppler/wfm/_bin/wfmgen``) and hands off to it with :func:`os.execv`, so the
process *becomes* the C tool — argv, stdio, exit status all pass straight
through. See ``docs/dev/wfmgen/api.md`` (D2/D5).

Run::

    wfmgen - -help
"""

import os
import shutil
import stat
import sys

# The binary CMake copies next to this package (see native/src/wfmcompose/
# CMakeLists.txt). Resolved relative to __file__ so it works from any install.
_BIN = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "_bin", "wfmgen"
)

#: Whether this platform builds the ``wfmgen`` binary at all -- the one
#: Python statement of where it exists. CMake builds it only inside
#: ``if(NOT WIN32)`` (native/src/wfmcompose/CMakeLists.txt:248), because it
#: links the POSIX-only stream client (doppler#1364), so on Windows the CLI
#: is absent by decision rather than missing by accident. Where this is True
#: a missing binary is a broken install and :func:`main` says so; the test
#: suite keys its skip on this and nothing else, so a Linux or macOS build
#: that lost the binary still FAILS rather than skipping.
AVAILABLE = sys.platform != "win32"


class UnavailableError(FileNotFoundError):
    """``wfmgen`` does not exist on this platform, by decision.

    Raised by :func:`_runnable` only where :data:`AVAILABLE` is False, so it
    can never stand in for a broken install. A ``FileNotFoundError`` so every
    caller that already handles a missing binary still does; the test suite's
    root conftest turns exactly this type into a skip.
    """


def _runnable() -> str:
    """Return a path to an executable ``wfmgen``.

    Wheels are zip archives and the zip entries carry no permission bits, so
    the extracted binary lands without ``+x``. Restore it in place when the
    install directory is writable (venv / uv / pipx / ``--user`` — the common
    case); otherwise fall back to a cached, executable copy under the user
    cache dir, which is always writable. Returns the path to exec.
    """
    if not AVAILABLE:
        raise UnavailableError(
            "the wfmgen CLI is not built on Windows (doppler#1364)"
        )
    st = os.stat(_BIN)  # raises FileNotFoundError with a clear path if missing
    if st.st_mode & stat.S_IXUSR:
        return _BIN
    try:
        os.chmod(_BIN, st.st_mode | 0o755)
        return _BIN
    except OSError:
        pass  # read-only install (e.g. a system site-packages) → cache a copy

    cache = os.environ.get("XDG_CACHE_HOME") or os.path.join(
        os.path.expanduser("~"), ".cache"
    )
    dst_dir = os.path.join(cache, "doppler")
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, "wfmgen")
    # Refresh the cached copy if absent or a different size (cheap staleness
    # check — a new wheel version ships a new binary).
    if not os.path.exists(dst) or os.path.getsize(dst) != st.st_size:
        shutil.copyfile(_BIN, dst)
        os.chmod(dst, 0o755)
    return dst


def main() -> int:
    """Exec the bundled ``wfmgen`` C binary, passing argv straight through."""
    if not AVAILABLE:
        sys.exit(
            "doppler: the 'wfmgen' CLI is not available on this platform. "
            "It links the POSIX-only stream client, which the Windows build "
            "does not include (doppler#1364). The waveform API it drives is: "
            "doppler.wfm.Composer, Segment and Writer."
        )
    try:
        binary = _runnable()
    except FileNotFoundError:
        sys.exit(
            "doppler: the bundled 'wfmgen' binary is missing from this "
            f"install (expected at {_BIN}). The wheel ships it as package "
            "data; a source/editable install must build it first "
            "(cmake -B build -DBUILD_PYTHON=ON && cmake --build build)."
        )
    # os.execv replaces this process; it does not return on success.
    os.execv(binary, [binary, *sys.argv[1:]])
