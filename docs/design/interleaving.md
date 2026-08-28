# Interleaving — spreading a burst across codewords

An interleaver adds no redundancy, detects nothing, and returns exactly as many
bits as it was given. What it does is change **where** errors land, and that is
worth a design page because the reasoning around it is easy to get wrong in
three specific ways — each of which was got wrong at least once while building
this, and each of which produces something that still round-trips against
itself.

## The transform

Write the input by rows into a `rows × cols` matrix; read it back by columns.

```text
write ->   c0  c1  c2  c3          read |
row0 [  0   1   2   3 ]                 v
row1 [  4   5   6   7 ]      0 4 8 1 5 9 2 6 10 3 7 11
row2 [  8   9  10  11 ]
```

That is the whole of it. `dp_interleave.h` is the permutation as an index map
and two typed appliers; `coding.Interleaver` holds a geometry and calls it.

De-interleaving is the same function with the two arguments exchanged — reading
a `rows × cols` matrix by columns *is* writing a `cols × rows` one by rows — so
the inverse is not a second implementation. A square block is therefore its own
inverse, which is a fact worth knowing before you rely on it accidentally.

## What it is for

Write one **codeword per row**. Reading by columns then transmits one symbol
from each codeword in turn, so a burst of up to `rows` consecutive symbols on
the wire costs each codeword **at most one**. An outer code correcting `t`
symbols per codeword survives a burst of `t × rows`.

The two numbers are a link budget rather than a tuning pair: `rows` is the
longest burst fully spread, `cols` is the codeword length.

## Three ways to get this wrong

### 1. Interleaving a single codeword buys nothing

Reed-Solomon corrects any `E` symbol errors **wherever they fall**. Permuting
them inside one codeword changes nothing a decoder can see. The gain exists
only when there are codewords to spread a burst *across*.

This is not a subtlety — it is the whole mechanism, and a validation built on a
single codeword measures a flat zero and reads like a broken interleaver. The
first draft of `validate_interleave_burst_gain` did exactly that.

### 2. It is not the outer code's own depth

`ccsds_tm_rs_encode_block(depth)` interleaves `depth` codewords and gets this
property for free — its header says so. Depth-I interleaving is *intrinsic to
the Reed-Solomon codeblock layout* (131.0-B-6 §4.4.1), fused into encode and
decode, and it is **not** a permutation applied afterwards.

So the two share a name and no implementation, and neither can be written in
terms of the other. A block interleaver is the general form: it works over
whatever span it is given, including many codeblocks and codes with no
interleaving of their own.

### 3. The unit must match the code's symbol

An RS code over `GF(256)` is protected by permuting **octets**. Permuting bits
inside such a code spreads a burst within symbols that are already wrong.

The measured difference is smaller than the argument suggests, and the reason is
worth stating precisely: each row still receives its share of a burst as a
*contiguous* run of bits, so the error **count** is about the same either way.
What differs is **alignment** — at `unit_bits = 8` each codeword gets exactly
`ceil(B/rows)` whole symbols, and at `unit_bits = 1` the run is not
octet-aligned so it can touch one more symbol at each end. That is invisible
until the burst is near the bound, and decisive there.

`unit_bits` is also an 8× throughput parameter, because an octet unit is one
`memcpy` per eight bits rather than one per bit: 515 Mbit/s against 4133
(`bench_interleaver_core`).

## The measured gain

`native/validation/interleave_burst_gain.c`, 5 × RS(255,223), E = 16, one
codeword per interleaver row:

| burst (octets) | FER, no interleaver | FER, `unit_bits=8` | FER, `unit_bits=1` |
| -------------- | ------------------- | ------------------ | ------------------ |
| 16             | 0.000               | 0.000              | 0.000              |
| 17             | 0.917               | **0.000**          | 0.000              |
| 80             | 1.000               | **0.000**          | 0.792              |
| 81             | 1.000               | 1.000              | 1.000              |

The corrigible burst goes from `E` = 16 octets to `E × rows` = 80 — and **stops
there**. An interleaver multiplies the corrigible burst by the depth; it does
not remove the bound, and 81 fails for every configuration.

0.917 rather than 1.000 at burst 17 is physical: a burst of `E+1` straddling a
codeword boundary splits into two runs of at most `E`, and both correct.

## Where it sits in a frame

`WFM_STAGE_INTERLEAVE` covers the **data group** — never the sync word or a
marker, which a receiver *finds* by correlation and which must therefore look
the same in every frame. It is applied **after** the outer code and the
randomiser and **before** the inner code, because an interleaver exists so a
burst on the channel arrives spread across the outer code's codewords: anything
between it and the wire would undo the point.

The **column** count is derived from the span the stage covers, since a stage's
cover is what says how much there is to permute. A span that is not a whole
number of `depth × unit` units is refused rather than padded — padding changes
the length, and a receiver de-interleaving the padded block recovers different
bits.

## The soft path

`DsssBurstReceiver.llrs` spans the whole frame, and an outer decoder wants those
LLRs de-interleaved **before** it runs. Slicing to hard decisions first and
de-interleaving those throws away the confidence the soft output exists to
carry, which is most of what an outer code is for.

This is why `dp_interleave.h` permutes **indices** rather than bytes: one kernel
serves `uint8` and `float32` alike. There is no `interleave_soft`, because a
transmitter has bits, not LLRs.

## Not a convolutional interleaver

A Forney interleaver is a different structure with different latency and memory
behaviour. It is deliberately absent rather than pending — see
[#1031](https://github.com/doppler-dsp/doppler/issues/1031).

## Where the code is

| what              | where                                                                                      |
| ----------------- | ------------------------------------------------------------------------------------------ |
| the permutation   | `native/inc/dp_interleave.h` (header-only)                                                 |
| the object        | `native/inc/interleaver/interleaver_core.h`, `coding.Interleaver` / `coding.Deinterleaver` |
| the frame stage   | `WFM_STAGE_INTERLEAVE` in `native/src/wfm/wfm_frame.c`                                     |
| the CLI           | `wfmgen --interleave R [--interleave-unit N]`                                              |
| the measured gain | `native/validation/interleave_burst_gain.c`                                                |
