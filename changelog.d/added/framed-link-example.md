- **A framed BPSK/QPSK link example** — `framed_link_demo.py` (and its C
    twin) sends frames built from one `FrameDesc`, receives them with
    `MpskReceiver`, resolves the carrier-phase ambiguity by the sync word and
    judges every frame with `FrameDesc.check`. It asserts that every passed
    payload was sent, a flipped bit is rejected and, at low Es/N0, damaged
    frames fail rather than pass.
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620))
