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

1. **Where the declared SNR actually shows up.** The gap is not silence — the
    source's AWGN keeps running while the signal stops — so burst power over
    gap power recovers the declaration (≈11.9 dB measured against 12 dB
    declared, `snr_mode=fs`).

1. **What a `Plan` does and does not cache.** It caches the clean signal and
    redraws the noise every point, so the saving is the signal's share of the
    work — modest for a low-duty burst train, larger as the on-time fraction
    or the per-sample signal cost grows. Both timings are printed and
    **neither is asserted**: a ratio is a property of the machine. What is
    asserted is that a cached render is byte-identical to composing the scene.
    That check is not decoration — a Plan that was fast because it silently
    dropped the noise floor is
    [#1158](https://github.com/doppler-dsp/doppler/issues/1158), which this
    example found, and no stopwatch could have seen it.

1. **BLUE round-trips.** cf32 out and back is byte-exact, both halves timed
    with the consumer's own work inside the read loop.

## Build and run

You need doppler installed somewhere CMake can find it:

```sh
cmake --install /path/to/doppler/build --prefix ~/.local
make PREFIX=~/.local run
```

Drop `PREFIX=` for a system install. `make help` lists the knobs; `make clean`
removes `build/`. `make run` builds and runs **both** link modes — the static
one is where a missing transitive dependency shows up.

## Two things building it taught

Both are comments in `main.c`, and both were found by running it rather than
by reading a header:

- **A downstream that does its own maths links `-lm` itself.** This program
    calls `log10()` and `fabs()`; without `m` on the link line the static target
    fails at link time. doppler's own libm use comes with the imported target —
    yours does not.
- **`wfm_writer_destroy` *is* `wfm_writer_close`.** The header says C callers
    may use either name. Calling both, as a create/destroy pair invites, closes
    the `FILE` twice and segfaults inside `ferror()`. Call one, and check its
    status.
