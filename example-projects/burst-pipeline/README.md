# burst-pipeline — a burst waveform end to end

Describe a frame, generate the train, sweep it, write a BLUE file, read it
back. Copy this directory as the starting point for a downstream that
generates realistic burst traffic and then does something with it.

Every number it prints is measured on the machine that runs it, with units.
Every claim it makes is checked, and a failed check exits non-zero — an
example that validates nothing still exits 0, so this one does not rely on
you reading the output.

## What it demonstrates

1. **A frame the caller builds.** A `wfm_frame_desc_t` assembled from
    `wfm_frame_add_field` / `add_derived` / `add_stage` — fields in wire order,
    a CRC-16 stage covering a span it names — attached to a source. No
    `wfmgen` flag spells this layout.

1. **`repeats` is not shorthand for listing segments.** Sixty bursts listed as
    sixty segments carry **identical** noise: a listed segment restarts the
    source from its declared seed. One segment with `repeats = 60` fixes the
    signal and draws fresh AWGN per instance. Both are checked, in both
    directions, because the difference is invisible in a plot and fatal to a
    Monte-Carlo run.

1. **Where the declared SNR actually shows up.** SNR is a property of a
    *source*, and the floor it implies runs through the whole segment — the
    gap is the channel, not digital silence. Measured with the library's own
    estimators: ≈11.5 dB data-aided and ≈11.6 dB blind, against 12 dB
    declared.

1. **What a `Plan` does and does not cache.** It caches the clean signal and
    redraws the noise every point, so the saving is the signal's share of the
    work — modest for a low-duty burst train, larger as the on-time fraction
    or the per-sample signal cost grows. Both timings are printed and
    **neither is asserted**: a ratio is a property of the machine. What is
    asserted is that a cached render is byte-identical to composing the scene.
    That check is not decoration: a cache that is fast because it quietly
    renders less is not something a stopwatch can catch.

1. **BLUE round-trips.** cf32 out and back is byte-exact, both halves timed
    with the consumer's own work inside the read loop.

1. **And then it receives.** The consumer does not know where the bursts are:
    it demaps the whole record, gaps included, and searches for the frame's
    sync marker. Two things keep that honest rather than circular —
    the marker is **rebuilt from the frame's own declaration**
    (`wfm_seq_bits` over the same generated field the transmitter used, not a
    second copy that can drift), and the tolerance is **derived** by
    `syncword_max_errors_for` from how much stream is searched rather than
    guessed. Each frame the search lands on is checked with
    `wfm_frame_desc_crc_ok`, which needs no payload truth, so the result is a
    frame error rate a receiver could compute on a capture it did not
    generate.

## Build and run

You need doppler installed somewhere CMake can find it:

```sh
cmake --install /path/to/doppler/build --prefix ~/.local
make PREFIX=~/.local run
```

Drop `PREFIX=` for a system install. `make help` lists the knobs; `make clean`
removes `build/`. `make run` builds and runs **both** link modes — the static
one is where a missing transitive dependency shows up.

## Which call to reach for

Use the library's measurement rather than writing your own — one fewer local
convention to drift from the one doppler ships.

| you want                                   | call                                                                  |
| ------------------------------------------ | --------------------------------------------------------------------- |
| SNR of a burst, knowing what was sent      | `snr_data_aided_db`                                                   |
| SNR of a burst, knowing nothing about it   | `snr_m2m4_db`                                                         |
| to tell a silent gap from a noisy one      | `snr_m2m4_db` — NaN on a block with zero power                        |
| to find a marker in a bit stream           | `syncword_find` — offset, polarity and Hamming distance in one answer |
| to choose how many bit errors to tolerate  | `syncword_max_errors_for`, `syncword_pfa`                             |
| to know whether a received frame is intact | `wfm_frame_desc_crc_ok` — no payload truth needed                     |

### Pick the marker length for the window you search, not by eye

`syncword_max_errors_for` answers "how many bit errors may I accept?" and it
needs to know **how much stream you are searching**, because the search
returns the *first* acceptable offset — every offset ahead of the real one is
its own chance to hit first. Over this scene's 1361-bit window a 31-bit marker
is refused outright: even demanding an exact match leaves a false-frame
probability above 1e-6. A 63-bit marker takes that from 2⁻³¹ to 2⁻⁶³ and
leaves room for 7 bit errors at Pfa 1.4e-10.

A refusal comes back as `-1`. Check for it: casting `-1` into the unsigned
tolerance the search takes turns *impossible* into *accept anything*, and a
receiver that does it will lock onto noise and report frames that were never
sent.

### The search works in bits, so give it bits

`syncword_find` answers in the symbol domain: which **bit** the marker starts
on, in which polarity, at what Hamming distance. It cannot resolve timing finer
than a symbol and is not meant to — recovering a fractional-sample offset is a
timing loop's job (`symsync`, `ratesync`).

That boundary is why this example runs at **one sample per symbol**. The frame,
the search and the CRC are all questions about symbols; oversampling would add
a pulse shape to match and a sample phase to recover, neither of which changes
any answer here. Hand the searcher a demapped bit stream, take the bit offset
it returns, and let a timing loop own the samples.

### Take the polarity with the offset

A hit carries `inverted`. A BPSK stream can arrive complemented, and a receiver
that takes the offset without the polarity hands its frame decoder inverted
bits that fail the CRC for a reason that has nothing to do with the channel.

## Two things that will bite you

Both are comments in `main.c` too, because they cost a build rather than a
read:

- **A downstream that does its own maths links `-lm` itself.** This program
    calls `log10()` and `fabs()`; without `m` on the link line the static target
    fails at link time. doppler's own libm use comes with the imported target —
    yours does not.
- **`wfm_writer_destroy` *is* `wfm_writer_close`.** The header says C callers
    may use either name. Calling both, as a create/destroy pair invites, closes
    the `FILE` twice and segfaults inside `ferror()`. Call one, and check its
    status.
