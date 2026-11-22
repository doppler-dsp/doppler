"""The loop filter's contract is stated once and reaches every face.

``loop_filter_core.h`` promises a closed-loop noise bandwidth of ``bn`` for a
loop whose rest has unit gain, and says what the caller delivers for that to
hold and what a gain other than 1 does (the doc block of
``dp_loop_filter_create``; ``dp_state_filter_create_why`` for the third-order
filter). The class docstring the Python face shows and the C reference are
generated from those blocks, so this holds that the sentences are in the
header, in the stub and at run time: a rewrite that drops one fails here.

The one numerical claim, the bandwidth at a loop gain K, is held by closing
the loop in Python around ``LoopFilter`` and reading its noise bandwidth from
the impulse response (``sum(h^2) / 2``), against the formula the docstring
states and against the claim the header used to make (K).
"""

from __future__ import annotations

import re

import numpy as np
import pytest

from doppler.tests._repo import repo_root
from doppler.track import LoopFilter, StateFilter

ROOT = repo_root(__file__)

#: The sentences every face carries, for both filters.
COMMON = (
    "provided the rest of the loop has unit gain",
    "slope at lock is 1 per unit of the filter's error input",
    "applies the control with unit gain, once per update",
    "is scaled to unit slope before the filter",
)
#: What only the second-order filter states: its closed form for a gain K.
SECOND_ORDER = (
    "(4*K*zeta^2 + 1) / (4*zeta^2 + 1)",
    "5/3 at K = 2 and 3 at K = 4 for zeta = 0.707",
)

FACES = {
    "LoopFilter": (
        LoopFilter,
        "native/inc/doppler/loop_filter/loop_filter_core.h",
        COMMON + SECOND_ORDER,
    ),
    "StateFilter": (
        StateFilter,
        "native/inc/doppler/state_filter/state_filter_core.h",
        COMMON,
    ),
}


def _flat(text: str) -> str:
    """One line, comment stars and markdown ticks gone, so a sentence the
    header and the generated docstring wrap differently still matches."""
    text = re.sub(r"\n\s*(\*|///?)?\s*", " ", text)
    return re.sub(r"\s+", " ", text.replace("`", "")).strip()


def _stub_class(name: str) -> str:
    """The docstring of ``class <name>`` in track.pyi, as written."""
    text = (ROOT / "src/doppler/track/track.pyi").read_text(encoding="utf-8")
    m = re.search(rf'^class {name}:\n    """(.*?)"""', text, re.M | re.S)
    assert m, name
    return m.group(1)


@pytest.mark.parametrize("name", sorted(FACES))
def test_the_header_states_the_contract(name):
    _, header, sentences = FACES[name]
    text = _flat((ROOT / header).read_text(encoding="utf-8"))
    missing = [s for s in sentences if _flat(s) not in text]
    assert not missing, (header, missing)


@pytest.mark.parametrize("name", sorted(FACES))
def test_the_stub_class_docstring_carries_it(name):
    _, _, sentences = FACES[name]
    text = _flat(_stub_class(name))
    missing = [s for s in sentences if _flat(s) not in text]
    assert not missing, (name, missing)


@pytest.mark.parametrize("name", sorted(FACES))
def test_the_runtime_docstring_carries_it(name):
    """What ``help()`` shows."""
    cls, _, sentences = FACES[name]
    text = _flat(cls.__doc__ or "")
    missing = [s for s in sentences if _flat(s) not in text]
    assert not missing, (name, missing)


@pytest.mark.parametrize("name", sorted(FACES))
def test_the_bn_line_points_at_the_contract(name):
    cls, _, _ = FACES[name]
    assert "unit-slope detector" in _flat(cls.__doc__ or "")


# ── what a loop gain other than 1 does ───────────────────────────────────


def _noise_bw(bn: float, zeta: float, k: float, n: int = 40000) -> float:
    """The closed loop's noise bandwidth, cycles per sample, by the impulse
    response of: error = input - output; control = filter(error); output
    += k * control, with no extra delay (the contract's fourth item)."""
    lf = LoopFilter(bn=bn, zeta=zeta, t=1.0)
    out = 0.0
    h = np.empty(n)
    for i in range(n):
        e = (1.0 if i == 0 else 0.0) - out
        out += k * lf.step(e)
        h[i] = out
    return float(np.sum(h * h) / 2.0)


@pytest.mark.parametrize("zeta", [0.5, 0.707, 1.0])
@pytest.mark.parametrize("k", [0.5, 2.0, 4.0])
def test_a_loop_gain_scales_the_bandwidth_as_the_docstring_says(k, zeta):
    bn = 0.002
    ratio = _noise_bw(bn, zeta, k) / _noise_bw(bn, zeta, 1.0)
    want = (4 * k * zeta**2 + 1) / (4 * zeta**2 + 1)
    assert ratio == pytest.approx(want, rel=0.03), (k, zeta, ratio, want)


@pytest.mark.parametrize("k", [2.0, 4.0])
def test_the_gain_is_not_the_bandwidth_ratio(k):
    """The control: the header used to say a slope of 4 gives a loop four
    times wider. It is 3.0 at zeta 0.707, and 5/3 at a slope of 2."""
    ratio = _noise_bw(0.002, 0.707, k) / _noise_bw(0.002, 0.707, 1.0)
    assert abs(ratio - k) > 0.3, (k, ratio)


def test_a_unit_gain_loop_delivers_the_bandwidth_asked_for():
    bn = 0.002
    assert _noise_bw(bn, 0.707, 1.0) == pytest.approx(bn, rel=0.02)
