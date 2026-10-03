"""num_samples is 0 by default, "derive it" -- and refused beside sources
that set the segment's length (doppler#1729).

A finite data source sets its segment's on-time (its frames), and so does a
lone dsss burst (one burst). Before, the CLI and a scene refused a count given
beside one, but the Python ``Segment``/``Composer`` face could not tell a given
``num_samples`` from its default of 1024, so the composer dropped it without a
word: ``Segment(type="bits", data=..., num_samples=1000)`` composed 24 samples.

The default is now 0, so a count given is a count the caller wrote, and ONE
rule -- ``dp_wfm_scene_error``, with the one static reason
``dp_wfm_why_count_derived`` -- refuses it on every face. A plain segment
derives 1024 and keeps any count it is given.
"""

from __future__ import annotations

import json
import subprocess

import numpy as np
import pytest

from doppler.wfm import (
    STAGE_CRC16,
    Composer,
    FrameDesc,
    Segment,
    cli,
    field_bits,
)

#: The one reason, as every face words it (wfm_compose.c).
WHY = "num_samples is derived: leave it 0, and give repeats for more"

#: 24 data bits in 8-bit frames: three frames, sent once.
DATA = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 3, np.uint8)


def _data_seg(**kw) -> Segment:
    return Segment(type="bits", modulation="bpsk", data=DATA, data_len=8, **kw)


def _crc_frame() -> FrameDesc:
    """``[data:8 | crc16]``: a CRC-16 trailer on each 8-bit frame."""
    d = FrameDesc()
    d.add_data("payload", 8)
    d.add_derived("crc", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "crc")
    return d


def _burst_seg(**kw) -> Segment:
    """A lone dsss burst: a preamble alone, no data source."""
    return Segment(
        type="dsss", sps=2, acq_code=field_bits("pn:31:5"), acq_reps=2, **kw
    )


def test_the_default_is_derive():
    assert Segment().num_samples == 0


@pytest.mark.parametrize(
    ("make", "on"),
    [
        # three frames of 8 data bits, no CRC unless asked: one sample a bit
        (_data_seg, 3 * 8),
        # ...and with a CRC-16 trailer on each frame
        (lambda: _data_seg(frame=_crc_frame()), 3 * (8 + 16)),
        (_burst_seg, 31 * 2 * 2),  # the preamble x reps, at sps chips
        (lambda: Segment(type="tone"), 1024),  # a plain segment
    ],
    ids=["finite data", "finite data with crc", "lone dsss burst", "plain"],
)
def test_zero_derives_the_on_time(make, on):
    assert Composer([make()]).compose().size == on


@pytest.mark.parametrize("count", [1000, (10, 20)], ids=["count", "ranged"])
@pytest.mark.parametrize(
    "make", [_data_seg, _burst_seg], ids=["finite data", "lone dsss burst"]
)
def test_a_count_beside_sources_that_set_the_length_is_refused(make, count):
    """The issue's case: the count was dropped, and 24 samples came out."""
    with pytest.raises(ValueError, match=WHY):
        Composer([make(num_samples=count)])


def test_a_plain_segment_keeps_its_count():
    assert (
        Composer([Segment(type="tone", num_samples=100)]).compose().size == 100
    )


def test_a_scene_is_refused_with_the_same_reason():
    scene = {
        "segments": [
            {
                "type": "bits",
                "data": "0xABCD",
                "data_len": 8,
                "num_samples": 64,
            }
        ]
    }
    with pytest.raises(ValueError, match=WHY):
        Composer.from_json(json.dumps(scene))


def test_a_burst_record_omits_its_count_and_replays():
    comp = Composer([_burst_seg()])
    text = comp.to_json()
    assert "num_samples" not in json.loads(text)["segments"][0]
    assert Composer.from_json(text).compose().size == comp.compose().size


@pytest.mark.parametrize(
    "flags",
    [
        ["--type", "bits", "--data", "0xABCD", "--data-len", "8"],
        ["--type", "dsss", "--sps", "2", "--acq-code", "pn:31:5*2"],
    ],
    ids=["finite data", "lone dsss burst"],
)
def test_the_cli_refuses_a_count_naming_its_flag(flags, tmp_path):
    p = subprocess.run(
        [cli._runnable(), *flags, "--count", "64", "-o", str(tmp_path / "x")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert p.returncode == 2, p.stderr
    assert "--count 64" in p.stderr and WHY in p.stderr
