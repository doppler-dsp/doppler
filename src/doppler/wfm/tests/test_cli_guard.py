"""wfmgen must not spew raw binary IQ onto an interactive terminal.

`wfmgen` with no `--output` defaults to raw IQ on stdout. On a real terminal
that prints binary garbage (a footgun when --output is forgotten), so the CLI
refuses it; piping/redirecting and the text CSV format stay allowed.
"""

from __future__ import annotations

import os
import pty
import subprocess

from doppler.wfm import cli


def _bin() -> str:
    return cli._runnable()


def test_wfmgen_refuses_binary_to_terminal():
    # stdout is a pseudo-tty -> isatty(stdout) is true -> guard fires.
    primary, secondary = pty.openpty()
    try:
        p = subprocess.run(
            [_bin()], stdout=secondary, stderr=subprocess.PIPE, close_fds=True
        )
    finally:
        os.close(secondary)
        os.close(primary)
    assert p.returncode != 0
    assert b"refusing to write binary IQ to a terminal" in p.stderr


def test_wfmgen_pipe_still_writes_binary():
    # stdout is a pipe (not a tty) -> binary IQ flows as before.
    p = subprocess.run(
        [_bin(), "--type", "tone", "--count", "16"],
        capture_output=True,
    )
    assert p.returncode == 0
    assert len(p.stdout) > 0


def test_wfmgen_csv_to_terminal_allowed():
    # CSV is human-readable text, so writing it to a tty is fine.
    primary, secondary = pty.openpty()
    try:
        p = subprocess.run(
            [_bin(), "--file_type", "csv", "--type", "tone", "--count", "2"],
            stdout=secondary,
            stderr=subprocess.PIPE,
            close_fds=True,
        )
    finally:
        os.close(secondary)
        os.close(primary)
    assert p.returncode == 0
