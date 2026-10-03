"""A receiver built from a ``data:LEN`` description recovers its source.

#1620 step 6. A data description (``[sync | data:LEN | crc]``) frames a
multi-frame source: the transmitter sends one chunk of the source per burst,
and ``DsssBurstReceiver`` -- told only the description -- must hand each
burst's frame back so that ``FrameDesc.deframe`` returns the chunk that was
sent. The comparison is against the SOURCE, not against a second transmit
path, so a receiver that mislaid the sync word, the frame length or the CRC
position cannot pass by agreeing with itself.

Each case also states what a receiver MUST NOT do: a corrupted burst is
reported ``frame_valid == 0`` rather than delivered as a good chunk.
"""

import numpy as np
import pytest

from doppler.dsss import DsssBurstReceiver
from doppler.wfm import STAGE_CRC16, Composer, FrameDesc, Segment, field_bits

REPS, SPC, CHIP_RATE = 5, 2, 1.0e6
ACQ, DATA = field_bits("pn:255:8:1"), field_bits("pn:31:5:3")
SYNC = field_bits("0000011001010")


def _desc(chunk):
    d = FrameDesc()
    d.add_field("sync", SYNC)
    d.add_data("payload", chunk)
    d.add_derived("crc", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "crc")
    d.build()
    return d


def _rx(d):
    return DsssBurstReceiver(
        acq_code=ACQ,
        data_code=DATA,
        frame=d,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=60.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )


def _capture(d, source, fill, rx):
    seg = Segment(
        type="dsss",
        fs=CHIP_RATE * SPC,
        snr=12.0,
        snr_mode="esno",
        seed=1,
        sps=SPC,
        acq_code=ACQ,
        acq_reps=REPS,
        data_code=DATA,
        frame=d,
        data=source,
        fill=fill,
        off_samples=rx.min_gap,
    )
    return np.asarray(Composer([seg]).compose())


@pytest.mark.parametrize(
    "chunk,nbits", [(48, 48 * 3), (48, 48 * 3 + 20), (24, 24 * 5)]
)
def test_each_burst_returns_the_chunk_that_was_sent(chunk, nbits):
    d = _desc(chunk)
    source = np.random.default_rng(chunk + nbits).integers(
        0, 2, nbits, dtype=np.uint8
    )
    fill = np.array([0, 1], np.uint8)
    rx = _rx(d)
    x = _capture(d, source, fill, rx)
    out = np.asarray(rx.push(x))
    ev = rx.events()

    frames = -(-nbits // chunk)  # ceil: the last chunk is padded
    assert len(ev) == frames
    assert all(e["frame_valid"] for e in ev)

    off = d.field_off(d.field_index("payload"))
    pad = frames * chunk - nbits
    want = np.concatenate([source, np.tile(fill, pad)[:pad]])
    for k in range(frames):
        frame = out[k * d.nbits : (k + 1) * d.nbits]
        assert d.check(frame).passed == 1
        got = np.asarray(d.deframe(frame))[off : off + chunk]
        assert np.array_equal(got, want[k * chunk : (k + 1) * chunk]), k


def test_a_corrupted_burst_is_not_delivered_as_good():
    """Damage the CAPTURE, so the receiver's own verdict is what is tested.

    Negating the samples of one payload symbol in burst 1 flips that bit on
    the wire. The receiver still finds and releases the burst (the preamble
    is intact), so it must report ``frame_valid == 0`` for it -- and still
    ``1`` for its neighbours -- rather than hand over a bad chunk as good.
    """
    d = _desc(48)
    source = np.random.default_rng(5).integers(0, 2, 48 * 3, dtype=np.uint8)
    rx = _rx(d)
    x = _capture(d, source, np.array([0], np.uint8), rx).copy()

    burst = (REPS * ACQ.size + d.nbits * DATA.size) * SPC
    off = d.field_off(d.field_index("payload"))
    sym = burst * 1 + (REPS * ACQ.size + (off + 3) * DATA.size) * SPC
    x[sym : sym + DATA.size * SPC] *= -1  # one payload symbol, burst 1

    out = np.asarray(rx.push(x))
    ev = rx.events()
    assert len(ev) == 3, "every burst is still found and released"
    assert [bool(e["frame_valid"]) for e in ev] == [True, False, True]
    assert d.check(out[d.nbits : 2 * d.nbits]).passed == 0
