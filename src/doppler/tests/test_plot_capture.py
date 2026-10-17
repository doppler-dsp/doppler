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
