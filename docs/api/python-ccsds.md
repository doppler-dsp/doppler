# Python CCSDS API

The `doppler.ccsds` module holds **CCSDS 131.0-B's published literals** — the
values one standard picked, kept beside the general layer rather than inside
it. Today that is `asm_bits()`, the Attached Sync Marker.

Source:
[`src/doppler/ccsds/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/ccsds/__init__.py)

______________________________________________________________________

## What is deliberately not here

The standard's **transforms** — the outer Reed-Solomon code, the randomiser,
the inner convolutional code — have no binding of their own and are not
getting one. You reach them by *describing a CADU*: three fields and three
covers on a [`FrameDesc`](python-wfmgen.md#framedesc-the-same-frame-deferred), and the
general assembler runs the standard's kernels from an ops table it is handed.
That is what keeps `wfm/wfm_frame.h` free of CCSDS while `ccsds_tm` depends on
it, and it is the design working rather than a gap in it — see
[Describing a frame](../design/frame-description.md).

So the rule for this module is narrow and worth stating: **a literal a mission
copies out of the Blue Book belongs here; a transform does not.**

______________________________________________________________________

## `asm_bits`

The Attached Sync Marker, `0x1ACFFC1D`, as 32 unpacked bits — one bit per
byte, `out[0]` first on the wire. Figure 9-1 of 131.0-B numbers the marker's
bit 0 as the most significant bit of `0x1A`, so the expansion runs down from
the top of the constant.

```python
from doppler.ccsds import asm_bits

b = asm_bits()
b.size, b[:8].tolist()                          # (32, [0, 0, 0, 1, 1, 0, 1, 0])
int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D   # True
```

**A call rather than a constant you expand.** An MSB-first expansion written
out twice is a transcription that can disagree with itself, and a receiver
that disagrees with the assembler about the marker syncs to nothing — silently
and forever, because there is no error to report. This tree's own doctests
were the second copy until [#900](https://github.com/doppler-dsp/doppler/issues/900).

**It is what a receiver acquires on.** Pair it with
[`SyncFinder`](python-detection.md#frame-synchronisation), which is general —
it takes whatever marker you hand it — to find where a CADU starts in a bit
stream:

```python
import numpy as np
from doppler.detection import SyncFinder

marker = np.asarray(asm_bits())
rng = np.random.default_rng(1220)
stream = rng.integers(0, 2, 500, dtype=np.uint8)
stream[173 : 173 + marker.size] = marker

f = SyncFinder(marker)
hit = f.find(stream, max_errors=f.max_errors_for(96, pfa=1e-3))
assert (hit.found, hit.offset, hit.inverted) == (1, 173, 0)
```

**And it is the only thing in a CADU that can report a 180-degree carrier
ambiguity.** The marker is deliberately *not* randomised — 10.4's NOTE: *"The
ASM was not randomized and is not derandomized"* — so it reads the same in
every frame and in exactly one polarity. Everything else in the frame can be
complemented and still pass: Reed-Solomon is linear and the all-ones vector is
itself a codeword, so a global flip lands on another codeword and the decoder
has nothing to object to. `hit.inverted` is what sees it.

The worked end-to-end example is
[A CCSDS CADU, as a Frame Description](../gallery/ccsds-link.md).

______________________________________________________________________

::: doppler.ccsds.asm_bits

## Related pages

<!-- related-pages:start -->

**Design** — [A Frame as a Description](../design/frame-description.md)

<!-- related-pages:end -->
