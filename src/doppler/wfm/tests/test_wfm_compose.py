"""Byte-parity: the generated ``wfm_compose`` composer vs ``compose.py``.

``doppler.wfm.wfm_compose`` is the jm-generated composer
(``kind = "composer"``, gh-287): the
``Synth``/``Segment``/``Timeline``/``Composer`` OO surface emitted as CPython
types **into the ``.so``**. During adoption it dual-runs alongside the
hand-written ``doppler.wfm.compose`` reference; these tests pin the generated
module to the reference sample-for-sample (and the delegated JSON wire format
byte-for-byte) so the eventual cutover is provably lossless.

Each side is driven through its own native API — the generated ``Segment``
forwards source fields plus the segment scalars, the reference ``Segment`` is a
flat dataclass — and the emitted ``cf32`` is compared with ``array_equal``
(exact,
not ``allclose``: a composer is deterministic, so parity is bit-exact).
"""

import numpy as np
import pytest

from doppler.wfm import compose as R  # hand-written reference  # noqa: N812

G = pytest.importorskip(
    "doppler.wfm.wfm_compose",
    reason="generated composer .so not built (cmake --build build)",
)


def _eq(a, b):
    a, b = np.asarray(a), np.asarray(b)
    return a.shape == b.shape and a.dtype == b.dtype and np.array_equal(a, b)


def test_single_tone_with_gap():
    g = G.Composer(
        G.Segment(
            type="tone", freq=1e5, fs=1e6, num_samples=1000, off_samples=500
        )
    )
    r = R.Composer(
        R.Segment(
            type="tone", freq=1e5, fs=1e6, num_samples=1000, off_samples=500
        )
    )
    assert _eq(g.compose(), r.compose())


def test_multi_segment_timeline():
    g = G.Composer(
        G.Timeline(
            [
                G.Segment(
                    type="tone",
                    freq=1e5,
                    fs=1e6,
                    num_samples=1000,
                    off_samples=500,
                ),
                G.Segment(type="qpsk", sps=8, fs=1e6, num_samples=4096),
            ]
        )
    )
    r = R.Composer(
        [
            R.Segment(
                type="tone",
                freq=1e5,
                fs=1e6,
                num_samples=1000,
                off_samples=500,
            ),
            R.Segment(type="qpsk", sps=8, fs=1e6, num_samples=4096),
        ]
    )
    assert _eq(g.compose(), r.compose())


def test_multi_source_sum():
    g = G.Composer(
        G.Segment.sum(
            G.Synth(type="tone", freq=1e5, level=-3.0),
            G.Synth(type="tone", freq=-2e5, level=-6.0),
            fs=1e6,
            num_samples=2048,
        )
    )
    r = R.Composer(
        R.Segment.sum(
            R.Synth(type="tone", freq=1e5, level=-3.0, fs=1e6),
            R.Synth(type="tone", freq=-2e5, level=-6.0, fs=1e6),
            fs=1e6,
            num_samples=2048,
        )
    )
    assert _eq(g.compose(), r.compose())


def test_bits_qpsk_pattern():
    pat = [1, 0, 1, 1, 0, 0, 1, 0]
    g = G.Composer(
        G.Segment(
            type="bits",
            bits=bytes(pat),
            modulation="qpsk",
            sps=4,
            fs=1e6,
            num_samples=2000,
        )
    )
    r = R.Composer(
        R.Segment(
            type="bits",
            pattern=pat,
            modulation="qpsk",
            sps=4,
            fs=1e6,
            num_samples=2000,
        )
    )
    assert _eq(g.compose(), r.compose())


def test_chirp_sweep():
    g = G.Composer(
        G.Segment(type="chirp", freq=1e5, f_end=3e5, fs=1e6, num_samples=4096)
    )
    r = R.Composer(
        R.Segment(type="chirp", freq=1e5, f_end=3e5, fs=1e6, num_samples=4096)
    )
    assert _eq(g.compose(), r.compose())


def test_execute_matches_compose():
    """Blockwise ``execute`` drains to the same stream as one-shot
    ``compose``."""
    spec = {"type": "tone", "freq": 5e4, "fs": 1e6, "num_samples": 3000}
    g = G.Composer(G.Segment(**spec))
    blocks = []
    while len(b := g.execute(512)):
        blocks.append(b)
    assert _eq(np.concatenate(blocks), G.Composer(G.Segment(**spec)).compose())


def _timeline(mod):
    return mod.Composer(
        [
            mod.Segment(
                type="tone",
                freq=1e5,
                fs=1e6,
                num_samples=1000,
                off_samples=500,
            ),
            mod.Segment(type="qpsk", sps=8, fs=1e6, num_samples=4096),
        ]
    )


def test_to_json_wire_bytes_match_reference():
    """Delegated ``wfm_spec_to_json`` → byte-identical wire format vs
    reference."""
    assert _timeline(G).to_json() == _timeline(R).to_json()


def test_from_json_round_trip():
    js = _timeline(G).to_json()
    assert _eq(G.Composer.from_json(js).compose(), _timeline(G).compose())


def test_invalid_enum_rejected():
    """Enum fields validate against the SSOT on construction."""
    with pytest.raises((ValueError, TypeError)):
        G.Synth(type="not_a_waveform")
