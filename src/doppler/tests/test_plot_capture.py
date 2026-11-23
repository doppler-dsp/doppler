"""``scripts/plot_capture.py``: the shading and the symbol rule it draws by.

The one capture plotter computes nothing, so what it can get wrong is where
it puts things. Two rules decide that, and each is held here:

- ``--shade PROBE`` greys the spans where a step-held probe reads 0 -- the
  lock validator's lifecycle stories shade their fades and the departure
  from their own ``stim.on`` record this way. A span that started a record
  late or stopped one early would put a cut where the signal was on.
- A probe is drawn as dots only when it carries recovered symbols (its name
  ends ``.i``/``.q``). The rule used to match any ``.sym.`` name, which
  turned the receiver's own ``rx.sym.lock`` metric and ``rx.sym.locked``
  flag into dot clouds the moment those probes existed.
"""

from __future__ import annotations

import json
import sys
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

sys.path.insert(0, str(repo_root(__file__) / "scripts"))

import plot_capture


def test_off_spans_hold_each_record_to_the_next() -> None:
    """A zero holds from its record to the next one; the last to the end."""
    xs = [0.0, 0.5, 1.0, 1.5, 2.0]
    assert plot_capture._off_spans(xs, [0, 1, 0, 0, 1], 3.0) == [
        (0.0, 0.5),
        (1.0, 2.0),
    ]
    assert plot_capture._off_spans(xs, [1, 1, 1, 1, 0], 3.0) == [(2.0, 3.0)]
    assert plot_capture._off_spans(xs, [1, 1, 1, 1, 1], 3.0) == []


def test_a_partial_block_is_not_off() -> None:
    """``stim.on`` is the fraction of a block that carried the signal: only
    a block with none of it is shaded."""
    assert plot_capture._off_spans([0.0, 1.0, 2.0], [0.0, 0.25, 1.0], 3.0) == [
        (0.0, 1.0)
    ]


@pytest.mark.parametrize(
    ("name", "dots"),
    [
        ("rx.demod.sym.i", True),
        ("rx.demod.sym.q", True),
        ("rx.sym.lock", False),
        ("rx.sym.locked", False),
        ("rx.car.nco", False),
    ],
)
def test_only_symbol_components_are_dots(name: str, dots: bool) -> None:
    assert plot_capture._is_symbol(name) is dots


def _capture(tmp_path: Path) -> Path:
    """A four-probe capture in the dp_tlm_capture format: records, then the
    sidecar naming each probe and the sample rate. Two share the suffix
    ``car.nco``, as the receiver's and its demodulator's do."""
    dt = np.dtype(
        {
            "names": ["n", "value", "probe", "flags"],
            "formats": ["<u8", "<f4", "<u2", "<u2"],
            "offsets": [0, 8, 12, 14],
            "itemsize": 16,
        }
    )
    rows = []
    for k in range(10):
        n = k * 100
        rows.append((n, 1.0 if 3 <= k < 6 else 0.0, 0, 0))  # rx.sym.locked
        rows.append((n, float(k), 1, 0))  # rx.car.nco
        rows.append((n, 0.0 if k in (4, 5) else 1.0, 2, 0))  # stim.on
        rows.append((n, -float(k), 3, 0))  # rx.demod.car.nco
    path = tmp_path / "story.tlm"
    np.array(rows, dtype=dt).tofile(path)
    meta = {
        "fs": 1000.0,
        "probes": {
            "rx.sym.locked": 0,
            "rx.car.nco": 1,
            "stim.on": 2,
            "rx.demod.car.nco": 3,
        },
    }
    (tmp_path / "story.tlm-meta").write_text(json.dumps(meta))
    return path


def test_shade_draws_from_the_capture(tmp_path: Path) -> None:
    """End to end: the named probe's off spans become every panel's grey."""
    cap = _capture(tmp_path)
    out = tmp_path / "story.png"
    assert (
        plot_capture.main(
            [str(cap), "--time", "--shade", "stim.on", "-o", str(out)]
        )
        == 0
    )
    assert out.stat().st_size > 0


def test_shade_needs_the_time_axis(tmp_path: Path) -> None:
    """Without ``--time`` each probe's x is its own record index, which no
    other probe shares, so a span would land anywhere: refused."""
    with pytest.raises(SystemExit, match="add --time"):
        plot_capture.main([str(_capture(tmp_path)), "--shade", "stim.on"])


def test_shade_names_one_probe(tmp_path: Path) -> None:
    """An ambiguous suffix is refused, not resolved to the first match."""
    cap = _capture(tmp_path)
    with pytest.raises(SystemExit, match="names one probe"):
        plot_capture.main([str(cap), "--time", "--shade", "car.nco"])
    with pytest.raises(SystemExit, match="no probe matching"):
        plot_capture.main([str(cap), "--time", "--shade", "stim.off"])


def test_scale_multiplies_and_names_the_unit() -> None:
    """``--scale`` is a factor and a unit, both the caller's. Failing case:
    a spec read without its factor (or its unit) gives 1 (or no unit)."""
    assert plot_capture._scale_spec("car.freq=11e6:Hz") == (
        "car.freq",
        11e6,
        "Hz",
    )
    assert plot_capture._scale_spec("x=2")[1:] == (2.0, "")


def test_running_mean_is_trailing_over_n_records() -> None:
    """The trailing mean over the last N records, the first N - 1 over what
    there is. Failing case: a centred window would give 1.0, 2.0, 4.0 ...
    and a mean over everything so far 0.0, 1.0, 2.0, 3.0."""
    assert plot_capture._running_mean([0.0, 2.0, 4.0, 6.0], 2) == [
        0.0,
        1.0,
        3.0,
        5.0,
    ]
    assert plot_capture._running_mean([3.0], 5) == [3.0]


def test_the_new_options_draw_from_the_capture(tmp_path: Path) -> None:
    """End to end: a scaled, averaged panel with a vertical line, a reference
    line and a caption read from a file writes a figure."""
    cap = _capture(tmp_path)
    cap_txt = tmp_path / "caption.txt"
    cap_txt.write_text("2.8 GHz 4 dB, guideline arm, seed 1")
    out = tmp_path / "pass.png"
    assert (
        plot_capture.main(
            [
                str(cap),
                "rx.car.nco",
                "--time",
                "--scale",
                "rx.car.nco=1000:Hz",
                "--mean",
                "rx.car.nco=3",
                "--vline",
                "0.5:t_pull",
                "--line",
                "rx.car.nco=4000:up 4000",
                "--caption-file",
                str(cap_txt),
                "-o",
                str(out),
            ]
        )
        == 0
    )
    assert out.stat().st_size > 0


def test_one_caption_source_and_a_count_of_records(tmp_path: Path) -> None:
    """Two caption sources are refused, not one silently preferred; a
    missing caption file and a mean over zero records are refused."""
    cap = _capture(tmp_path)
    with pytest.raises(SystemExit, match="give one"):
        plot_capture.main(
            [str(cap), "--caption", "a", "--caption-file", "b.txt"]
        )
    with pytest.raises(SystemExit, match="no such caption file"):
        plot_capture.main([str(cap), "--caption-file", str(tmp_path / "x")])
    with pytest.raises(SystemExit, match="count of records"):
        plot_capture.main([str(cap), "--mean", "rx.car.nco=0"])
