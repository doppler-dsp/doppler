# A Message Over DSSS, Received and Deframed

A message is split into 48-bit chunks, one chunk per frame, and each frame
is spread into a DSSS burst. `DsssBurstReceiver` finds every burst; the
**same** `FrameDesc` the transmitter was given then checks each frame and
returns its chunk. The message comes back from the capture alone — there is
no hand-slicing of bit positions and no second copy of the frame layout.

## What it shows

**The run is exactly the frames.** The message's bits are the segment's
`data`, the description `[sync | data:48 | crc16]` says how each frame is
made, and the short last chunk is padded from `fill`. No `num_samples` is
given: the run length is the frames', derived.

**The receiver is built from the description.** The sync word and the frame
length are the description's, so the receiver finds all six bursts at their
exact samples, and `FrameDesc.check` passes every frame's CRC-16.

**The message comes back byte for byte.** `FrameDesc.deframe` hands back each
corrected frame; the payload is a slice found by name. Concatenated, the
chunks reproduce the message, and the spare bits of the last frame are the
fill.

**The check is a real detector:** one flipped payload bit fails it.

The script asserts all of this and exits non-zero if any claim breaks. Its C
twin is `native/examples/data_source_link_demo.c`.

## The code

```python
--8<-- "src/doppler/examples/data_source_link_demo.py:link"
```

```python
--8<-- "src/doppler/examples/data_source_link_demo.py:deframe"
```

## Related

- [DSSS Burst Receiver](dsss-burst-receiver.md) — the receiver used here.
- [A Framed Link, Checked by Its Description](framed-link.md) — the same idea
    for BPSK and QPSK.
