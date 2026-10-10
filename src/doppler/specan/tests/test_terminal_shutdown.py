"""The terminal display stops its DSP thread before the source is closed.

`_run_terminal` closes the source as soon as `TerminalDisplay.run()`
returns. If the DSP thread is still inside `source.read()` then, it goes on
to `recv` or `ack` on a closed `Pull`, a probable use-after-free in the
binding (#2016). `run()` therefore joins the thread on the way out; this
pins that it has finished when `run()` returns.
"""

from __future__ import annotations

import time
from types import SimpleNamespace

import numpy as np

from doppler.specan import terminal
from doppler.specan.config import SpecanConfig


class _Live:
    def __init__(self, *args, **kwargs):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def update(self, *args, **kwargs):
        pass


class _SlowSource:
    """A read that takes a while, as a receive waiting out its timeout."""

    _timeout_ms = 300

    def __init__(self):
        self.reading = False

    def read(self, n):
        self.reading = True
        time.sleep(0.3)
        self.reading = False
        return np.empty(0, np.complex64), 0.0, 0.0

    def set_fft_size(self, n):
        pass


class _Engine:
    block_size = 1024

    def process(self, iq, fs, cf):
        return None


def test_the_dsp_thread_has_finished_when_run_returns(monkeypatch):
    monkeypatch.setattr(terminal, "Live", _Live)
    monkeypatch.setattr(terminal, "Console", lambda: SimpleNamespace(width=80))
    source = _SlowSource()
    display = terminal.TerminalDisplay(_Engine(), SpecanConfig(), source)
    monkeypatch.setattr(display, "_read_key", lambda: "q")  # quit at once

    display.run()

    assert not display._dsp_thread.is_alive()
    assert not source.reading  # nothing is mid-read when close() comes
