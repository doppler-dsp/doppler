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

1. **And then it receives.** The consumer does not know where the bursts are:
    it demodulates the whole record, gaps included, and searches for the
    frame's sync marker. Two things keep that honest rather than circular —
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

## It uses doppler's measurements, not its own

Where the library already has the calculation, this reaches for it. That is
not tidiness: a hand-rolled mean-power ratio — which is what section 3 used to
be — is one more place for a local convention to drift from the one the
library ships.

| what                                  | how                                                                   |
| ------------------------------------- | --------------------------------------------------------------------- |
| SNR of a burst, knowing what was sent | `snr_data_aided_db`                                                   |
| SNR of a burst, knowing nothing       | `snr_m2m4_db`                                                         |
| is that gap silence, or noise?        | `snr_m2m4_db` returns NaN on a block with zero power                  |
| find a marker in a bit stream         | `syncword_find` — offset, polarity and Hamming distance as one answer |
| how many bit errors to tolerate       | `syncword_max_errors_for` / `syncword_pfa`                            |
| is this frame intact?                 | `wfm_frame_desc_crc_ok`                                               |

### The search is over bits, and says so

`syncword_find` answers in the symbol domain: which **bit** the marker starts
on, in which polarity, at what Hamming distance. A fractional-sample offset is
a timing loop's problem — `symsync`, `ratesync` — not a bit-domain searcher's,
so the scene's lead-in is a whole number of symbols and this section claims
only what it can prove: all sixty bursts found at the exact predicted bit.

Getting that boundary wrong is easy and instructive. An earlier draft gave the
scene a three-sample lead-in and searched every sample phase, ranking phases by
the marker's Hamming distance. It found all sixty and decoded all sixty — and
placed **40 of them one sample either side of the truth**, because a
rectangular pulse is flat-topped: a one-sample shift still integrates almost
the whole symbol, so the marker still matches with *zero* bit errors. Hamming
distance cannot resolve timing finer than a symbol. That is a real property,
and the fix is not a better sync word.

### The tolerance is not a property of the marker

The first draft used a 31-bit marker and `syncword_max_errors_for` **refused
it**: over this scene's 1361-bit search window, even demanding an exact match
leaves a false-frame probability above the 1e-6 asked for, because the search
returns the *first* acceptable offset and every offset ahead of the real one is
its own chance to hit first. A 63-bit marker takes the exact-match probability
from 2⁻³¹ to 2⁻⁶³ and buys 7 bits of tolerance at Pfa 1.4e-10.

A refusal is `-1`, and casting it to the unsigned tolerance the search takes
would turn *impossible* into *accept anything* — which is how a receiver locks
onto noise and reports sixty happy frames. The example handles it as a refusal.

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
