#!/usr/bin/env python3
"""wfmgen_frame_demo.py -- a source framed by a description you built.

The Python twin of ``native/examples/wfmgen_frame_demo.c``. A frame no flag
spells -- 16 bits of the caller's own header, the payload, then a CRC-16 a
stage derives over a span it NAMES -- is a ``FrameDesc``, and a source's
``frame=`` takes it (a ``FrameDesc`` or a ``Frame``; text is refused).
``wfmgen --frame FILE`` and a scene's ``"frame"`` key reach the same C
member, so all three faces compose the same waveform.

What it shows, in order:

1. ``frame=`` reaches the SAMPLES: a framed source and an otherwise
   identical unframed one do not compose to the same waveform.
2. ``frame=`` is an input, read back from the composer's JSON: the record
   carries the description as a scene's ``"frame"`` key, and that scene
   composes byte-identically -- after the ``FrameDesc`` is gone, because
   the source keeps its own copy.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/wfmgen_frame_demo.py
"""

# --8<-- [start:frame]
import numpy as np

from doppler.wfm import STAGE_CRC16, Composer, FrameDesc, Segment, field_bits

d = FrameDesc()  # fields in wire order, by name
d.add_field("hdr", field_bits("0101110001011100"))  # a header of your own
d.add_field("payload", field_bits("101010101010101010101010"))
d.add_derived("crc", 16)  # 16 bits a stage will fill
d.add_stage_over(STAGE_CRC16, "payload", "crc")  # the span it covers

common = {
    "type": "bits",
    "fs": 1e6,
    "sps": 4,
    "modulation": "bpsk",
    "snr": 100.0,
    "snr_mode": "fs",
}
framed = Segment(**common, frame=d)
x = np.asarray(Composer([framed]).compose())
# --8<-- [end:frame]

import json  # noqa: E402
import sys  # noqa: E402


def main() -> int:
    # 1. The frame reaches the samples.
    # The same payload bits as a data source, sent as given: no frame.
    unframed = Segment(**common, data=field_bits("1010" * 6))
    y0 = np.asarray(Composer([unframed]).compose())
    ok1 = not np.array_equal(x[: y0.size], y0)
    print("framed != unframed:", ok1)

    # 2. Read back from the record; the scene rebuilds the same samples,
    #    after the FrameDesc is gone (the source owns its own copy).
    text = Composer([framed]).to_json()
    frame = json.loads(text)["segments"][0]["frame"]
    print("recorded fields:", [f.get("name") for f in frame["fields"]])
    y = np.asarray(Composer.from_json(text).compose())
    ok2 = np.array_equal(x, y) and frame["stages"][0]["kind"] == "crc16"
    print("scene JSON == keyword:", ok2)

    return 0 if (ok1 and ok2) else 1


if __name__ == "__main__":
    del d  # every check below reads the source's own copy
    sys.exit(main())
