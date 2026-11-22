"""A generated Field reads back as the bits it renders (doppler#3192).

A ``wfm_seq_t`` is a literal or a generator's description (PN, Gold, dotted);
a scene spells the second as ``"pn:800:15"``. ``Composer.segments`` used to
read such a field as ``None``, so a scene that used one could not be
recomposed from what it reads back. The five sequence fields' Python read
now goes through ``dp_wfm_source_<field>_bits`` (jm's ``get_fn`` on a bytes
field), which renders the description through ``dp_wfm_seq_bits``; the
stored member, ``to_json`` and the setter keep what was given.

The oracle for the bits is ``field_bits``, the one place the library turns a
Field spelling into bits; the evidence that the read is right is that the
segments recompose to the same samples.
"""

from __future__ import annotations

import json

import numpy as np
import pytest

from doppler.wfm import Composer, Segment, field_bits

#: (field, the scene segment that carries it generated). An ofdm source with
#: mac data carries data and mac_header; a dsss burst carries the two codes.
SCENES = {
    "data": {
        "type": "ofdm",
        "fs": 20e6,
        "wlan": 36,
        "data": "pn:800:15",
        "data_len": 800,
    },
    "mac_header": {
        "type": "ofdm",
        "fs": 20e6,
        "wlan": 36,
        "mac": "data",
        "mac_header": "pn:192:15",
        "data": "pn:800:15",
        "data_len": 800,
    },
    "acq_code": {
        "type": "dsss",
        "sps": 4,
        "fs": 4e6,
        "acq_code": "pn:31:5",
    },
    "data_code": {
        "type": "dsss",
        "sps": 4,
        "fs": 4e6,
        "acq_code": "pn:31:5",
        "data_code": "pn:31:5",
        "data": "pn:16:5",
        "data_len": 16,
    },
}


def _scene(seg):
    return Composer.from_json(json.dumps({"version": 1, "segments": [seg]}))


@pytest.mark.parametrize("field", sorted(SCENES))
def test_a_generated_field_reads_as_the_bits_it_renders(field):
    seg = SCENES[field]
    got = _scene(seg).segments[0]
    bits = getattr(got, field)
    assert bits is not None, field
    np.testing.assert_array_equal(
        np.frombuffer(bits, dtype=np.uint8), field_bits(seg[field])
    )


@pytest.mark.parametrize("field", sorted(SCENES))
def test_the_segments_recompose_to_the_same_samples(field):
    """What ``segments`` reads back composes the same samples, with no edit
    of the copy: its ``num_samples`` is the length the sources derive, and a
    count equal to that is not dropped (#3225)."""
    c = _scene(SCENES[field])
    segs = c.segments
    again = Composer(segs)
    np.testing.assert_array_equal(again.compose(), c.compose())


def test_a_record_still_writes_the_description_it_was_given():
    """Only the Python read moved: the record keeps the generator's
    spelling, so a scene written from a scene is the scene."""
    c = _scene(SCENES["mac_header"])
    out = json.loads(c.to_json())["segments"][0]
    assert out["mac_header"] == "pn:192:15", out
    assert out["data"] == "pn:800:15", out


def test_an_absent_field_still_reads_none():
    """No generator, no bits: ``len`` 0 is nothing to read."""
    assert Segment(type="ofdm", fs=20e6, wlan=36).mac_header is None


def test_a_literal_still_reads_as_given():
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
    seg = Segment(type="ofdm", fs=20e6, wlan=36, data=bits, data_len=8)
    np.testing.assert_array_equal(
        np.frombuffer(seg.data, dtype=np.uint8), bits
    )
