"""The display center follows the source until someone chooses one.

A live RF source says where it is tuned (a stream header's center_freq). The
display center defaulted to 0.0 regardless, so an RTL-SDR at 100 MHz was
drawn as if at 0 Hz until the user repeated `--center 100e6`. Now an unset
center adopts the source's, and a center that was set -- yml, CLI, or a
retune -- is kept.
"""

from pathlib import Path

import numpy as np

from doppler.specan.config import SpecanConfig, load_config
from doppler.specan.engine import SpecanEngine

FS, FC = 2.4e6, 100e6
NO_YML = Path("/nonexistent/doppler-specan.yml")


def _frame(engine: SpecanEngine, center_freq: float):
    rng = np.random.default_rng(0)
    iq = (rng.standard_normal(engine.block_size * 4) * 1e-3).astype(
        np.complex64
    )
    frame = None
    for k in range(0, len(iq), 8192):
        frame = engine.process(iq[k : k + 8192], FS, center_freq) or frame
    return frame


def test_an_unset_center_follows_the_source():
    cfg = load_config(yml_path=NO_YML, source="socket")
    assert not cfg.center_pinned
    frame = _frame(SpecanEngine(cfg), FC)
    assert frame is not None and frame.center_freq == FC


def test_a_center_given_on_the_command_line_is_kept():
    cfg = load_config(yml_path=NO_YML, source="socket", center=99.9e6)
    assert cfg.center_pinned
    frame = _frame(SpecanEngine(cfg), FC)
    assert frame is not None and frame.center_freq == 99.9e6


def test_a_retune_pins_the_center():
    cfg = SpecanConfig(source="socket")
    engine = SpecanEngine(cfg)
    _frame(engine, FC)  # adopts 100 MHz
    engine.retune(100.1e6)
    # The source then moves. Unpinned, the rebuild would adopt 100.2 MHz;
    # the retune was a choice, so the display stays where it was put.
    frame = _frame(engine, 100.2e6)
    assert cfg.center_pinned and frame.center_freq == 100.1e6


def test_follow_source_undoes_a_retune():
    # What the terminal's reset key calls when the start-up center was not
    # chosen: back to following the source, not pinned to a stale value.
    cfg = SpecanConfig(source="socket")
    engine = SpecanEngine(cfg)
    _frame(engine, FC)
    engine.retune(100.1e6)
    engine.follow_source()
    assert not cfg.center_pinned and cfg.center == FC
    frame = _frame(engine, 100.2e6)  # and it keeps following
    assert frame.center_freq == 100.2e6
