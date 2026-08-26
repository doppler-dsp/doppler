# DsssBurstReceiver — the Composed Burst Chain

![Four bursts decoded at their exact samples, and the same answer at every block size](../assets/dsss_burst_receiver_demo.png)

The composed form of `BurstAcquisition -> refine -> BurstDemod`, the burst
DSSS receive chain, in one C object:
[`DsssBurstReceiver`](../api/python-dsss.md). Its continuous counterpart is
[`DsssReceiver`](dsss-receiver.md); the hand-composed version of *this*
chain — each stage demonstrated on its own — is the
[5-Burst DSSS Link](dsss-burst-pipeline.md) page, and that page is still the
one to read first if you want to see what each stage does.

This page is what composing them buys. It is not fewer lines.

## What the hand-off actually costs

Between acquisition and demodulation sits arithmetic that every caller was
redoing, and getting subtly wrong:

- acquisition reports an **end** anchor and a code phase **modulo one code
    period**. Neither is a burst start, and recovering one from the other is
    the `refine` stage — a search over whole code periods either side.
- one preamble raises **several** detections. Coalescing them is a rule
    about a span, not a loop the caller writes.
- the bin→frequency fold was restated at four call sites in three mutually
    inconsistent ways before it became
    [`dp_fftfreq()`](../c-api/index.md).

## Three properties, each asserted

The example is a gate, not an illustration: it exits non-zero if any of
these stops being true.

### 1. Block size does not change the answer

The same capture is pushed whole, then in 64 KiB, 1000 and 333-sample
blocks — the smallest **32x shorter than a single burst**. All five decode
the same four bursts at the same samples, with `dropped == 0`.

```python
--8<-- "src/doppler/examples/dsss_burst_receiver_demo.py:decode"
```

That is not free. It is what the history ring and the internally sliced
push are for, and it was broken in three separate places at once
([#1008](https://github.com/doppler-dsp/doppler/issues/1008)): an early
return that never looked at `x`, a `break` that left the tail unwritten,
and a discard that did not count. A block carrying several bursts lost all
but the first.

### 2. A burst split across two calls is held, not lost

Feed half a burst and `push()` returns nothing — deliberately. A burst is
returned when it is **complete**, not when it is guessed at. Feed the rest
and it comes out whole, bit-exact, wherever the split fell.

<!-- docs-snippet: skip=an excerpt whose names (capture, truth, payload) live in the example's namespace; the script itself is executed on every push by `make test-examples-python` -->

```python
--8<-- "src/doppler/examples/dsss_burst_receiver_demo.py:split"
```

`pending` is how you can tell the difference between "nothing here" and "I
am holding one". **Read it before you stop feeding a stream**: closing a
file or a socket while it is non-zero discards a burst that would have
decoded, and no other read-back distinguishes that case from an empty
capture — `dropped` counts samples the ring refused, `n_bursts` counts what
was demodulated, and a truncated burst is neither.

### 3. `refine_span` is the minimum burst spacing

Two detections closer together than `refine_span` are treated as the same
preamble and merged. So bursts packed tighter than it are **lost rather
than reported**, and the span is a property to read rather than a constant
to assume:

<!-- docs-snippet: skip=an excerpt whose names (BURST_LEN, receiver) live in the example's namespace; the script itself is executed on every push by `make test-examples-python` -->

```python
--8<-- "src/doppler/examples/dsss_burst_receiver_demo.py:spacing"
```

The boundary is sharp. At spacing exactly `refine_span`, one burst of four
is lost; one sample more recovers all four. Both spans were internal until
[#1011](https://github.com/doppler-dsp/doppler/issues/1011) — the only way
to learn the minimum spacing was to read the C, and the header's own
formula for it was 2.4x low.

## The same thing in C

The [C example](https://github.com/doppler-dsp/doppler/blob/main/native/examples/dsss_burst_receiver_demo.c)
demonstrates the identical four sections and prints the identical numbers,
because both build their capture from **one wfmgen scene** through the same
engine — `wfm_compose_create()` in C, `Composer`/`Segment` in Python.
Neither tiles a preamble, spreads a frame, appends a CRC or draws noise.

What the C face shows that the Python one cannot is the part the binding
does for you: the lifecycle you manage yourself, and that the output buffer
is **the caller's** — sized from `push_max_out()` on the block being
pushed, not from `payload_len`, because one call may complete several
bursts.

## Why the preamble is 255 chips, not 511

`acq` transforms `sf * spc` **verbatim**: the code-axis correlation is
circular, so zero-padding the replica or the epoch would change the
correlation rather than interpolate it. An m-sequence is `2^n - 1` chips —
31, 127, 511, 2047 — every one prime or near-prime, which is the worst case
for an FFT. At 127 chips and `spc=4` that is 508 = 2²·**127**, and
pocketfft falls back to Bluestein: **9.70 µs against 0.75 µs** for a smooth
length, and 21 MSa/s against 52 end to end.

255 chips at `spc=2` is 510 = 2·3·5·17 — smooth, **and** twice the
autocorrelation ratio of the 127-chip code. Better on both axes rather than
a trade.

Rounding to 512 is the trap: no binary code of that length has good
periodic autocorrelation (an extended m-sequence gives 8.0; the best of a
4000-code random search reached 10.7), and this object's own
[certification](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/dsss_burst_receiver/results.md)
brackets what that costs — ratio 31 found every burst offset, 1.07 lost
47% of them.

## Related pages

- [5-Burst DSSS Link](dsss-burst-pipeline.md) — the same chain, hand-composed, each stage on its own
- [DsssReceiver](dsss-receiver.md) — the continuous counterpart
- [DSSS Acquisition: Pd/Pfa](dsss-acq-characterization.md) — the search stage characterised
