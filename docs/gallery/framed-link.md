# A Framed Link, Checked by Its Description

One `FrameDesc` describes a frame — a sync word, a payload, a CRC-16 — and
that same description is used on both ends of the link. wfmgen builds the
waveform from it; `MpskReceiver` recovers the symbols; the description then
finds and judges every frame. Nothing about the frame is written twice: the
sync word is field 0, the frame length comes from the layout, and the verdict
comes from `FrameDesc.check`.

The example runs BPSK and QPSK at Es/N0 = 14 dB with a carrier offset the
receiver has to remove, 200 frames of 48 payload bits each.

## What it shows

**Phase ambiguity is resolved by the sync word.** A carrier loop settles on
one of `M` equivalent phases (two for BPSK, four for QPSK) and cannot tell
which. Each phase is tried, and the one whose bits repeat the description's
sync word at the frame period is kept — the way a real receiver resolves it.

**Every frame after the lock transient passes, with the right payload.** The
first frame or two are lost while the loops settle; the rest pass
`FrameDesc.check`, and each passed payload is one that was sent.

**The check is a real detector.** One flipped bit fails it. At 4 dB Es/N0 more
than half the frames are damaged and fail, and none passes carrying a payload
that was never sent.

The script asserts all of this and exits non-zero if any claim breaks. Its C
twin is `native/examples/framed_link_demo.c`.

## The code

```python
--8<-- "src/doppler/examples/framed_link_demo.py:link"
```

## Related

- [M-PSK Receiver](mpsk-receiver.md) — the receiver used here.
- [`FrameDesc`](../api/python-wfmgen.md#framedesc-the-same-frame-deferred) — the description both ends share.
