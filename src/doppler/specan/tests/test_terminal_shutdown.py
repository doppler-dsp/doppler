"""The terminal display never closes a source its DSP thread is reading.

`_run_terminal` closes the source and engine once `TerminalDisplay.run()`
returns. If the DSP thread is still inside `source.read()` then, it goes on
to `recv` or `ack` on a closed `Pull`, a probable use-after-free in the
binding (#2016). `run()` joins the thread on the way out, but only for a
grace period: a `read()` loops until it has a whole block, so it can
outlive any bound. Past the grace, `_run_terminal` leaves both open and
says so. Ending the read itself is #2016.
"""

from __future__ import annotations

import threading
import time
from types import SimpleNamespace

import numpy as np

import doppler.specan.source
from doppler.specan import terminal
from doppler.specan.__main__ import _run_terminal
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


def _headless(monkeypatch):
    monkeypatch.setattr(terminal, "Live", _Live)
    monkeypatch.setattr(terminal, "Console", lambda: SimpleNamespace(width=80))
    # Quit at the first key poll.
    monkeypatch.setattr(terminal.TerminalDisplay, "_read_key", lambda _: "q")


def test_the_dsp_thread_has_finished_when_run_returns(monkeypatch):
    _headless(monkeypatch)
    source = _SlowSource()
    display = terminal.TerminalDisplay(_Engine(), SpecanConfig(), source)

    display.run()

    assert display.dsp_stopped
    assert not source.reading  # nothing is mid-read when close() comes


class _StuckSource:
    """A read() that outlives the grace: still short of a whole block."""

    def __init__(self):
        self.release = threading.Event()
        self.closed = False

    def read(self, n):
        self.release.wait(timeout=30)
        return np.empty(0, np.complex64), 0.0, 0.0

    def set_fft_size(self, n):
        pass

    def close(self):
        self.closed = True


def test_a_source_still_being_read_is_not_closed(monkeypatch, capsys):
    _headless(monkeypatch)
    source = _StuckSource()
    monkeypatch.setattr(doppler.specan.source, "make_source", lambda _: source)
    try:
        _run_terminal(SpecanConfig(timeout=0))  # a 1 s grace
        assert not source.closed
        assert "#2016" in capsys.readouterr().err
    finally:
        source.release.set()  # let the daemon DSP thread finish
