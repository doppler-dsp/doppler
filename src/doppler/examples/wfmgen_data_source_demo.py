#!/usr/bin/env python3
"""wfmgen_data_source_demo.py -- a payload drawn from a data source.

The Python twin of ``native/examples/wfmgen_data_source_demo.c``. A message
is split into ``data_len``-bit frames, ONE chunk per frame, each with a
CRC-16 over its own chunk -- not one frame cycled. The last chunk is short,
so it is padded from ``fill``, and the run's length is the frames', derived
rather than given (docs/design/payload-data-source.md).

``wfmgen --data FIELD`` (or ``--data-from-file PATH``, ``-`` for stdin) and a
scene's ``"data"`` reach the same C member; Python's ``data=`` takes the
bits. A file's bytes become bits through ``cvt.bytes_to_bin``.

What it shows, in order:

1. The message comes back frame by frame: every frame carries the next
   chunk, in order, and the last is padded with the fill.
2. Each frame's CRC-16 covers ITS chunk -- a receiver checks every frame on
   its own.
3. The run is exactly the frames: ``ceil(bits / data_len)`` of them, with no
   ``num_samples`` given.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/wfmgen_data_source_demo.py
"""

# --8<-- [start:data]
import numpy as np

from doppler.cvt import bytes_to_bin
from doppler.wfm import STAGE_CRC16, Composer, FrameDesc, Segment

message = b"a payload drawn from a data source, frame by frame"
octets = np.frombuffer(message, np.uint8)
bits = np.zeros(8 * octets.size, np.uint8)
bytes_to_bin(octets, bits, 0)  # MSB first, as a file is read

SPS = 4
d = FrameDesc()
PAYLOAD = 96  # bits of the source per frame
d.add_data("payload", PAYLOAD)
d.add_derived("crc", 16)
d.add_stage_over(STAGE_CRC16, "payload", "crc")  # a CRC-16 over its chunk
d.build()  # lay it out: the receive side (check, deframe) reads the layout
seg = Segment(
    type="bpsk",
    fs=1e6,
    sps=SPS,
    snr=100.0,
    snr_mode="fs",
    data=bits,  # the source: its bits
    frame=d,  # each frame: its chunk, then a CRC-16 over it
    fill=np.array([0, 1], np.uint8),  # pads the short last frame
)
x = np.asarray(Composer([seg]).compose())
# --8<-- [end:data]

import math  # noqa: E402


def check(ok: bool, what: str) -> None:
    if not ok:
        raise SystemExit(f"FAIL {what}")
    print(f"PASS {what}")


FRAME = d.nbits  # the layout's length: data, then a CRC-16 over it
OFF = d.field_off(d.field_index("payload"))  # where the chunk starts
frames = math.ceil(bits.size / PAYLOAD)
check(
    x.size == frames * FRAME * SPS,
    f"3. the run is exactly {frames} frames, with no num_samples given",
)

# BPSK at the symbol centre: bit 1 is -1.
rx = (x[SPS // 2 :: SPS].real < 0).astype(np.uint8)
pad = frames * PAYLOAD - bits.size  # the spare bits of the last frame
fill = np.tile(np.array([0, 1], np.uint8), pad)[:pad]
sent = np.concatenate([bits, fill])
for f in range(frames):
    frame = rx[f * FRAME : (f + 1) * FRAME]
    chunk = sent[f * PAYLOAD : (f + 1) * PAYLOAD]
    # The description that built the frame also reads it back: `deframe`
    # hands the corrected frame, `check` the CRC verdict -- no field offsets
    # written twice.
    back = np.asarray(d.deframe(frame))
    check(
        np.array_equal(back[OFF : OFF + PAYLOAD], chunk),
        f"1. frame {f} carries chunk {f}",
    )
    check(d.check(frame).passed == 1, f"2. frame {f}'s CRC covers its chunk")
last = (frames - 1) * FRAME + OFF + PAYLOAD
check(
    np.array_equal(rx[last - pad : last], fill),
    f"1. the last frame's {pad} spare bits are the fill",
)
print("all checks passed")
