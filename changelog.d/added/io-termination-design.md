- **One wait contract for network, memory and disk**
    ([`docs/design/io-termination.md`](https://github.com/doppler-dsp/doppler/blob/main/docs/design/io-termination.md)).
    The three transports have the same defect three times: "no data right
    now" is indistinguishable from "no data ever", and no producer can
    answer whether the bytes it handed over actually landed. The page is
    the design phase for fixing it once instead of three times.

    Ranked by severity rather than by build order: the **ring buffer** is
    the worst and has no escape at all — `dp_<t>_wait()` is an unbounded
    busy-spin that checks no flag, so a stopped producer leaves the
    consumer spinning forever at 100% CPU and no signal handler can
    rescue it. **Disk** is the subtlest, because it does not hang, it
    lies: a short read is indistinguishable from end-of-file, so a
    tail-following reader reports a clean finish on a truncated capture.
    **Network** is the one already fixed, and it is the model.

    The interrupt primitive is not new — `dp_stream_interrupt()` is a
    `volatile sig_atomic_t` and four accessors in a file including only
    libc, with **no NATS dependency**. It is already general; only its
    name and location say otherwise. It moves down a layer, gains the
    ring and disk waits as callers, and sheds the `stream_` infix.

    No code yet: this is the ungated design phase, deliberately written
    before an implementation so the vocabulary cannot diverge across
    three transports. Streaming's own unmeasured numbers are recorded in
    the same pass, as `streaming.md` §11.
