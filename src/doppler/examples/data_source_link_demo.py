#!/usr/bin/env python3
"""data_source_link_demo.py -- a message over DSSS, received and deframed.

A message is split into 48-bit chunks, one chunk per frame, and each frame
is spread into a DSSS burst; ``DsssBurstReceiver`` finds every burst; the
SAME ``FrameDesc`` the transmitter was given then checks each frame and
returns its chunk. The message comes back from the capture alone -- no
hand-slicing of bit positions, no second copy of the frame layout.

What it shows, in order:

1. Transmit: the message's bits are the segment's ``data``; the description
   ``[sync | data:48 | crc16]`` says how each frame is made; the short last
   chunk is padded from ``fill``. The run is exactly the frames' length.
2. Receive: the receiver is built from the description, finds every burst
   at its exact sample, and every frame's CRC-16 checks.
3. Deframe: concatenating the chunks reproduces the message byte for byte,
   and the spare bits of the last frame are the fill.
4. The check is a real detector: one flipped payload bit fails it.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/data_source_link_demo.py
"""

# --8<-- [start:link]
import math

import numpy as np

from doppler.cvt import bytes_to_bin
from doppler.dsss import DsssBurstReceiver
from doppler.wfm import (
    STAGE_CRC16,
    Composer,
    FrameDesc,
    Segment,
    field_bits,
)

CHUNK, REPS, SPC, CHIP_RATE = 48, 5, 2, 1.0e6
acq_code, data_code = field_bits("pn:255:8:1"), field_bits("pn:31:5:3")

message = b"a message, one chunk per DSSS burst"
octets = np.frombuffer(message, np.uint8)
bits = np.zeros(8 * octets.size, np.uint8)
bytes_to_bin(octets, bits, 0)  # MSB first, as a file is read
fill = np.array([0, 1], np.uint8)  # pads the short last frame
frames = math.ceil(bits.size / CHUNK)  # one burst per chunk

# The frame, once: a sync word, one chunk of the source, a CRC-16 over it.
d = FrameDesc()
d.add_field("sync", field_bits("0000011001010"))  # Barker-13, inverted
d.add_data("payload", CHUNK)
d.add_derived("crc", 16)
d.add_stage_over(STAGE_CRC16, "payload", "crc")
d.build()
NB = d.nbits

receiver = DsssBurstReceiver(
    acq_code=acq_code,
    data_code=data_code,
    frame=d,  # the sync word and the frame length are the description's
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
seg = Segment(
    type="dsss",
    fs=CHIP_RATE * SPC,
    snr=12.0,
    snr_mode="esno",
    seed=1,
    sps=SPC,
    acq_code=acq_code,
    acq_reps=REPS,
    data_code=data_code,
    frame=d,
    data=bits,  # the message: its bits, one chunk per burst
    fill=fill,
    off_samples=receiver.min_gap,  # dead air after the train
)
x = np.asarray(Composer([seg]).compose())

out = np.asarray(receiver.push(x))
events = receiver.events()
# --8<-- [end:link]

BURST = (REPS * acq_code.size + NB * data_code.size) * SPC


def check(ok: bool, what: str) -> None:
    if not ok:
        raise SystemExit(f"FAIL {what}")
    print(f"PASS {what}")


check(
    x.size == frames * BURST + receiver.min_gap,
    f"1. the run is exactly {frames} bursts and the dead air, "
    "with no num_samples given",
)
valid = [e for e in events if e["frame_valid"]]
check(len(valid) == frames, f"2. {len(valid)}/{frames} bursts decoded")
check(
    [int(e["preamble_start"]) for e in valid]
    == [k * BURST for k in range(frames)],
    "2. every burst is found at its exact sample",
)

# --8<-- [start:deframe]
off = d.field_off(d.field_index("payload"))
chunks = []
for k in range(frames):
    frame = out[k * NB : (k + 1) * NB]
    verdict = d.check(frame)
    assert verdict.passed == verdict.checked == 1, f"frame {k}: CRC failed"
    chunks.append(np.asarray(d.deframe(frame))[off : off + CHUNK])
got = np.concatenate(chunks)
# --8<-- [end:deframe]

n = bits.size
check(np.array_equal(got[:n], bits), "3. the chunks are the message's bits")
check(
    np.packbits(got[:n]).tobytes() == message,
    "3. the message comes back byte for byte",
)
pad = got.size - n
check(
    np.array_equal(got[n:], np.tile(fill, pad)[:pad]),
    f"3. the last frame's {pad} spare bits are the fill",
)
bad = out[:NB].copy()
bad[off] ^= 1
check(d.check(bad).passed == 0, "4. a flipped payload bit fails the check")
print("all checks passed")
