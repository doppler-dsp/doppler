# FFT-size handshake, per front end; the contract is in docs/design/specan.md.


from __future__ import annotations

import threading
import time

import pytest

from doppler.specan import record_demo, terminal
from doppler.specan.config import SpecanConfig
from doppler.specan.engine import (
    DBM_AT_UNIT_AMPLITUDE,
    SpecanEngine,
    dbm_to_amplitude,
)
from doppler.specan.source import DemoSource


class _RecordingSource(DemoSource):
    # A demo source that keeps every ``set_fft_size`` it is given.

    def __init__(self, sample_rate: float = 1e6) -> None:
        super().__init__(
            sample_rate=sample_rate, center_freq=0.0, tone_freq=1e5, seed=3
        )
        self.announced: list[int] = []

    def set_fft_size(self, n: int) -> None:
        self.announced.append(n)
        super().set_fft_size(n)


class _SpyEngine(SpecanEngine):
    # An engine that keeps the ``data_size`` of every frame it produced.

    def __init__(self, cfg: SpecanConfig) -> None:
        super().__init__(cfg)
        self.data_sizes: list[int] = []

    def process(self, iq, fs_in, center_freq):
        frame = super().process(iq, fs_in, center_freq)
        if frame is not None:
            self.data_sizes.append(frame.data_size)
        return frame


def _first_frame(step, limit: int = 1000):
    # Call ``step`` until it returns a frame; fail if it never does.
    for _ in range(limit):
        frame = step()
        if frame is not None:
            return frame
    raise AssertionError(f"no frame after {limit} steps")


def test_engine_step_announces_the_frame_data_size():
    engine = _SpyEngine(SpecanConfig())
    source = _RecordingSource()

    _first_frame(lambda: engine.step(source))
    for _ in range(3):  # later frames at the same size: no new announcement
        engine.step(source)

    assert source.announced == [engine.data_sizes[0]]


def test_terminal_dsp_loop_announces_the_frame_data_size():
    engine = _SpyEngine(SpecanConfig())
    source = _RecordingSource()
    display = terminal.TerminalDisplay(engine, SpecanConfig(), source)

    display._running = True
    thread = threading.Thread(target=display._dsp_loop, daemon=True)
    thread.start()
    deadline = time.monotonic() + 30.0
    while not source.announced and time.monotonic() < deadline:
        time.sleep(0.01)
    display._running = False
    thread.join(timeout=10.0)

    assert source.announced, "the terminal never announced an FFT size"
    assert source.announced[0] == engine.data_sizes[0]


def test_server_next_frame_announces_the_frame_data_size(monkeypatch):
    # The server needs fastapi; skip, not error, where it is not installed.
    server = pytest.importorskip("doppler.specan.server")
    engine = _SpyEngine(SpecanConfig())
    source = _RecordingSource()
    monkeypatch.setattr(server, "_engine", engine)
    monkeypatch.setattr(server, "_source", source)

    _first_frame(server._next_frame)

    assert source.announced == [engine.data_sizes[0]]


def test_record_frames_announces_the_frame_data_size():
    engine = _SpyEngine(SpecanConfig())
    source = _RecordingSource()

    frames = record_demo.collect_frames(engine, source, n_frames=2, warmup=1)

    assert len(frames) == 2
    assert source.announced == [engine.data_sizes[0]]


def test_one_dbm_definition_is_the_one_the_engine_offsets_by():
    # amplitude 1.0 is DBM_AT_UNIT_AMPLITUDE dBm; every other level is the
    # 20 log10 of its ratio to that, so the engine's offset and the source's
    # levels cannot disagree (#2159).
    assert dbm_to_amplitude(DBM_AT_UNIT_AMPLITUDE) == pytest.approx(1.0)
    for dbm in (-90.0, -20.0, 0.0, 13.0):
        expected = 10.0 ** ((dbm - DBM_AT_UNIT_AMPLITUDE) / 20.0)
        assert dbm_to_amplitude(dbm) == pytest.approx(expected)


def test_an_rbw_change_after_the_first_frame_re_announces():
    # At 2.048 MHz a 4 kHz to 8 kHz rbw change halves data_size (1024 to 512);
    # the source must be told the new size, not keep scaling by the old one.
    cfg = SpecanConfig(fs=2.048e6, rbw=4000.0)
    engine = _SpyEngine(cfg)
    source = _RecordingSource(sample_rate=2.048e6)

    first = _first_frame(lambda: engine.step(source))
    cfg.rbw = 8000.0
    engine.zoom(cfg.span)
    second = _first_frame(lambda: engine.step(source))

    assert second.data_size != first.data_size
    assert source.announced == [first.data_size, second.data_size]


def test_a_data_size_change_with_the_same_display_bins_re_announces():
    # 8 to 16 kHz rbw at 2.048 MHz: data_size 512 to 256, nfft stays 512 and
    # the display stays 401 bins. Only data_size moves, so an announce keyed
    # on the display bins never fires.
    cfg = SpecanConfig(fs=2.048e6, rbw=8000.0)
    engine = _SpyEngine(cfg)
    source = _RecordingSource(sample_rate=2.048e6)

    first = _first_frame(lambda: engine.step(source))
    cfg.rbw = 16000.0
    engine.zoom(cfg.span)
    second = _first_frame(lambda: engine.step(source))

    assert first.fft_size == second.fft_size
    assert (first.data_size, second.data_size) == (512, 256)
    assert source.announced == [512, 256]
