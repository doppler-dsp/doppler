"""`--detached` is a FILE FORMAT, and every combination that cannot honour
it is refused rather than dropped.

`--detached` selects BLUE's detached header: the HCB in ``<out>.hdr`` and the
samples in ``<out>.det``, instead of one file. Exactly one destination
honours it -- the dispatch tests ``file_type == 2 && detached`` -- and until
gh-725 every other combination silently discarded a flag the user typed:

* ``--realtime`` never reached the detached drain, so a paced detached run
  wrote flat out;
* ``--detached`` itself was ignored for any other ``--file-type`` or for a
  ``nats://`` destination, producing one ordinary undetached file.

gh-725 asked whether to pace it or refuse the pair. Refusing, because the
destination's own shape decides it: the ``.hdr`` carries the final sample
count and cannot be written until the drain ends, and ``--detached`` already
refuses an endless run -- so nothing can read the pair while it is being
paced, and pacing would only make a finite file take longer with no consumer
waiting.

The last test here is the one that would have prevented the confusion. The
tool's own ``--help`` described the flag as "Run as a detached background
process" and filed it under REAL-TIME, where ``--realtime`` looks like it
must apply. The guide and the CLI test had it right; the help and the
flag-matrix exclusion did not.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.wfm import cli

if TYPE_CHECKING:
    from pathlib import Path


#: Largest file a run here may write. The failure this file guards includes
#: a run that never ends (--detached --repeat, doppler#1591), which writes
#: at ~500 MB/s: its first red run filled a 7.6 GB /tmp before a timeout
#: fired. So the red state must bound its own damage -- a size cap kills the
#: child long before the disk notices, and the timeout is the fallback where
#: there is no rlimit (Windows).
_MAX_FILE = 64 << 20


def _cap_file_size() -> None:
    import resource

    resource.setrlimit(resource.RLIMIT_FSIZE, (_MAX_FILE, _MAX_FILE))


def _run(*args: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [cli._runnable(), *args],
        capture_output=True,
        timeout=20,
        preexec_fn=None if sys.platform == "win32" else _cap_file_size,
    )


#: Every invocation that names --detached where it cannot be honoured, with
#: the phrase whose absence would mean the message stopped being specific.
REFUSED = [
    pytest.param(
        ["--file-type", "blue", "--detached", "--realtime"],
        b"does not pace",
        id="realtime",
    ),
    pytest.param(
        ["--file-type", "blue", "--detached", "--realtime-resync"],
        b"does not pace",
        id="realtime-resync",
    ),
    pytest.param(
        ["--detached"],
        b"needs --file-type blue",
        id="not-blue",
    ),
    # A detached pair is a header plus data, and the header is written when
    # the run ends -- so a run that never ends has no header. --continuous
    # was refused; --repeat, just as unbounded, ran until the disk filled
    # (3.1 GB in 5 s) and left no .hdr (doppler#1591).
    pytest.param(
        ["--file-type", "blue", "--detached", "--continuous"],
        b"finite",
        id="continuous",
    ),
    pytest.param(
        ["--file-type", "blue", "--detached", "--repeat"],
        b"finite",
        id="repeat",
    ),
]


@pytest.mark.parametrize(("extra", "phrase"), REFUSED)
def test_a_combination_that_cannot_honour_detached_is_refused(
    tmp_path: Path, extra: list[str], phrase: bytes
) -> None:
    p = _run(
        "--type",
        "tone",
        "--count",
        "8",
        "-o",
        str(tmp_path / "x"),
        *extra,
    )
    assert p.returncode == 2, p.stderr
    assert phrase in p.stderr, p.stderr


def test_detached_to_a_broker_is_refused_without_needing_one() -> None:
    """No broker is contacted: the flag pair is nonsense before any
    connection, which is also why this test needs no nats-server."""
    p = _run(
        "--type",
        "tone",
        "--count",
        "8",
        "--file-type",
        "blue",
        "--detached",
        "-o",
        "nats://127.0.0.1:4222/x",
    )
    assert p.returncode == 2, p.stderr
    assert b"no meaning for a nats:// destination" in p.stderr, p.stderr


def test_the_valid_detached_run_is_unchanged(tmp_path: Path) -> None:
    """The behaviour being protected, not just the refusals."""
    base = tmp_path / "ok"
    p = _run(
        "--type",
        "tone",
        "--count",
        "8",
        "--sample-type",
        "cf32",
        "--file-type",
        "blue",
        "--detached",
        "-o",
        str(base),
    )
    assert p.returncode == 0, p.stderr
    hdr = base.with_suffix(".hdr")
    det = base.with_suffix(".det")
    assert hdr.is_file() and det.is_file()
    assert hdr.stat().st_size == 512  # a BLUE HCB
    assert det.stat().st_size == 8 * 8  # 8 cf32 samples, 8 bytes each


def test_realtime_without_detached_still_paces(tmp_path: Path) -> None:
    """The refusal is about the PAIR. --realtime on its own is untouched."""
    p = _run(
        "--type",
        "tone",
        "--count",
        "8",
        "--file-type",
        "blue",
        "--realtime",
        "-o",
        str(tmp_path / "rt"),
    )
    assert p.returncode == 0, p.stderr


def test_help_calls_detached_a_file_format_not_a_process() -> None:
    """The doc bug that seeded gh-725, pinned so it cannot come back.

    `--help` is the surface a user reads before filing an issue about a flag
    behaving oddly, and this one described a process model the flag has
    never had.
    """
    out = _run("--help").stdout
    assert b"--detached" in out
    assert b"background process" not in out
    assert b"<out>.hdr" in out and b"<out>.det" in out

    # And it is filed under OUTPUT, not REAL-TIME: the grouping is half of
    # what made --realtime look applicable.
    text = out.decode()
    detached_at = text.index("--detached")
    output_at = text.index("OUTPUT\n")
    realtime_at = text.index("REAL-TIME\n")
    assert output_at < detached_at < realtime_at
