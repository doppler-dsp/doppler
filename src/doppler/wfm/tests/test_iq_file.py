"""Smoke tests for the IqFile reader's lifecycle + accessors.

Round-trip / bit-faithfulness against real wfmgen captures lives in
``test_readback.py``; this file covers construction, the state getters,
reset (rewind), and the headerless error contract on a tiny hand-written file.
"""

import struct

import numpy as np
import pytest

from doppler.wfm import IqFile


@pytest.fixture
def cf32_file(tmp_path):
    """A 4-sample interleaved-cf32 capture (8 floats, little-endian)."""
    p = tmp_path / "smoke.iq"
    vals = [1.0, 0.0, 0.0, 1.0, -1.0, 0.5, 0.25, -0.75]
    p.write_bytes(struct.pack("<8f", *vals))
    return str(p)


def test_create_and_nsamples(cf32_file):
    obj = IqFile(cf32_file, "cf32")
    assert obj is not None
    assert obj.nsamples == 4  # 32 bytes / 8 bytes-per-cf32-sample


def test_getters(cf32_file):
    obj = IqFile(cf32_file, "cf32")
    assert obj.get_fd() >= 0  # a real open fd
    assert obj.get_position() == 0
    assert obj.get_nsamples() == 4
    assert obj.get_sample_type() == 0  # cf32 -> 0
    assert obj.get_endian() == 0  # le -> 0
    obj.read(2)  # advance via read, not set_position
    assert obj.get_position() == 2


def test_reset_rewinds(cf32_file):
    obj = IqFile(cf32_file, "cf32")
    obj.read(2)
    assert obj.get_position() == 2
    obj.reset()
    assert obj.get_position() == 0


def test_endian_and_type_indices(cf32_file):
    obj = IqFile(cf32_file, "ci16", "be")
    assert obj.get_sample_type() == 3  # ci16 -> 3
    assert obj.get_endian() == 1  # be -> 1


def test_context_manager(cf32_file):
    with IqFile(cf32_file, "cf32") as obj:
        iq = obj.read(obj.nsamples)
    assert iq.shape == (4,)
    assert iq.dtype == np.complex64


def test_missing_file_raises(tmp_path):
    with pytest.raises(FileNotFoundError):
        IqFile(str(tmp_path / "nope.iq"), "cf32")


def test_bad_sample_type_rejected(cf32_file):
    with pytest.raises(ValueError):
        IqFile(cf32_file, "cf99")


def test_close_is_idempotent(cf32_file):
    obj = IqFile(cf32_file, "cf32")
    obj.close()
    obj.close()  # no error on second close
    tail = obj.read(4)  # reads after close are zero
    assert np.all(tail == 0)


def test_read_past_eof_zero_fills(cf32_file):
    obj = IqFile(cf32_file, "cf32")
    obj.read(obj.nsamples)  # consume the file
    tail = obj.read(3)
    assert tail.shape == (3,)
    assert np.all(tail == 0)
