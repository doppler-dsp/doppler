- **`DsssBurstReceiver.push()` no longer discards its input, and returns
    **every** burst it completed**
    ([#1008](https://github.com/doppler-dsp/doppler/issues/1008)). Three
    separate discard sites meant a block carrying several bursts lost all but
    the first: measured at **6/6 decoded with 333-sample blocks against 1/6
    with one large one**. It now decodes 5/5 at every block size, including a
    1.48 M-sample capture in a single call, with `dropped == 0`.
