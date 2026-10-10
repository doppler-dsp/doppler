# Streaming Spectrograms

A spectrogram turns a stream into **rows**: one spectrum every `hop` samples,
each over the `nfft` samples that start there. `dp_spectrogram` does that for
a stream that arrives in pieces of any size — a socket's datagrams, a file
reader's blocks, a ring's frames — and gives the same rows however the
stream was cut. Feed it a sample at a time or a second at a time; the rows
are identical, bit for bit.

Use it when you want **every** row: a waterfall display, a capture plotted
end to end, a detector scanning time and frequency. If you want one averaged
spectrum instead, that is [`PSD`](spectral-psd.md); a spectrogram row *is*
the PSD of its frame, unaveraged.

!!! note "C only, for now"

    The Python face follows the just-makeit release that can return a
    `(rows, nfft)` matrix from a method (just-makeit#2152). Until then the
    spectrogram is a C component: `#include "doppler/spectrogram/spectrogram_core.h"` and link `libdoppler`.

## A waterfall from a stream that arrives in pieces

The program below makes a tone that hops between four frequencies with the
library's LO, delivers it in chunks of 1, 37, 700 and 5 samples, and builds
the waterfall two ways, which must agree bit for bit. Every row that lies
inside one frequency segment is checked: its peak is that segment's tone, at
0 dBFS. It is the tested example (`make test-examples-c`), included whole.

```c
--8<-- "native/examples/spectrogram_demo.c"
```

## Rows

Row *k* covers stream samples `[k·hop, k·hop + nfft)`. With `hop == nfft`
the rows tile the stream; with `hop < nfft` they overlap, and `nfft / hop`
rows are computed per `nfft` samples. Each costs one FFT, so a quarter hop
costs about four times as much per input sample.

A row is `nfft` floats in **dBFS**, PSD's own convention: a full-scale tone
on a bin reads 0 dB whatever the window. Rows are DC-centred exactly as PSD
emits them, bin *k* at index `nfft/2 + k` with negative frequencies first;
bin order has one home, PSD's kernel, so there is no option to reorder it
here ([#1988](https://github.com/doppler-dsp/doppler/issues/1988)). The
window is PSD's index: 0 Hann, 1 Kaiser (with `beta`), 2 Blackman-Harris,
3 rectangular.

`nfft` must be the PSD's transform length for that frame, which is to say a
power of two of at least 2, and `1 <= hop <= nfft`. The window must have gain
at that length: `nfft = 2` with Hann is refused, because the symmetric
two-point Hann is `[0, 0]`. A frame length that is
not a power of two would be zero-padded by the PSD to more bins than samples
([#1966](https://github.com/doppler-dsp/doppler/issues/1966)), and linear
power rows are reserved until PSD's normalised per-frame power lands
([#1968](https://github.com/doppler-dsp/doppler/issues/1968)).

## Sizing the output

`dp_spectrogram_push` writes whole rows only, and returns the number of
**floats** it wrote. There are two honest ways to own the buffer, and the
example does both:

- **Size each push.** `dp_spectrogram_push_max_out(s, n)` is exactly the
    room that lets a push take all `n` samples: `rows_for(s, n) * nfft`. It is
    often 0, because a short chunk completes no row, and the push still takes
    the chunk into its carry.
- **Keep one buffer.** Give `push` whatever room you have. It stops when the
    next row would not fit.

`dp_spectrogram_rows_for(s, n)` is exact, not an estimate: it counts the
carry already held plus `n` samples.

## A short buffer never loses input

The rule is the ring framer's own: **a sample is taken unless it would
complete a row `out` has no room for.** So a push into a full buffer stops
at a whole row and `dp_spectrogram_consumed(s)` says where to resume. Offer
the rest again after you have drained the rows. Input that completes no row
is always taken, even with no room at all.

What is held between calls is the carry: fewer than `nfft` samples once a
push returns. `dp_spectrogram_pending(s)` counts the samples pushed that no
row has covered yet.

## Ending a stream

The last few samples of a stream usually complete no row.
`dp_spectrogram_flush(s, row)` writes the one row they are owed,
zero-padded. It is **on the hop grid**: it starts at the next row start
`k·hop`, not at the first uncovered sample, so it is exactly the row a
one-shot run over the zero-padded stream would have made. It writes a row if
and only if a sample is uncovered, returns `nfft` or 0 floats, and restarts
the stream, so a second flush writes nothing.

`flush` is explicit by design: closing a source does not imply it.

## Checkpoint and resume

The state is the stream position: the carry, the stream counts and the hop
that frames them. `dp_spectrogram_get_state` and `dp_spectrogram_set_state`
move it as a standard state blob of a size that depends on `nfft` alone.
Restore it into a fresh spectrogram created with the same arguments, and the
next push continues the stream bit for bit, even mid-frame.

A different `nfft` or `hop` is refused on restore. A different window or
`beta` is **not**: the blob does not carry them, so the rows that follow are
the restoring object's window, not the original's. Keeping them the same is
the caller's job. See [Checkpoint & Resume](state-serialization.md).

## What it is not

- **Not an averager.** Fold rows with `AccTrace` if you want a video
    average or a max-hold.
- **Not a detector.** It stops at the row.
- **Not a transport.** It never learns where its samples came from.

The design, its goals and what is still unmeasured are on
[the design page](../design/spectrogram.md); the carry is the
[ring buffer's framed face](../design/ring-buffer.md); every function's
contract, with an example, is the
[C API](../c-api/spectrogram__core_8h.md).
