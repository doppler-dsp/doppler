# Soft Decisions for M-PSK

Why, what shape, and what is not known yet.

`doppler.mpsk`'s module docstring has said *"M-ary PSK mapping: hard and
**soft**"* since the module shipped. There is no soft anything in it: five
functions, all hard-decision. This page is the design for making that sentence
true.

It is phase 1 of [Adding an Algorithm](../dev/adding-algorithms.md) — the one
phase with no gate, which is exactly why it is written before the code.

______________________________________________________________________

## 1. The use case, and it is a specific one

**A Viterbi decoder fed hard bits throws away most of the coding gain it
exists to deliver.** That is the whole reason this is being built now.

`fec/` (the CCSDS TM channel-coding slice) encodes today and decodes nothing.
The decoder is next, and a rate-1/2 K=7 Viterbi is a soft-input machine: the
gap between hard-decision and soft-decision decoding of this exact code is
about **2 dB** of Eb/N0, which is larger than the difference between having
the convolutional code and not having it at low rates. Building the decoder
against a hard-bit input would be building the wrong thing correctly.

So the caller is concrete:

| caller                         | wants                                 | why                                                |
| ------------------------------ | ------------------------------------- | -------------------------------------------------- |
| the CCSDS inner decoder (next) | one soft value per channel bit        | Viterbi branch metrics                             |
| a future soft-input outer code | the same, scaled                      | R-S erasure flagging, LDPC/turbo if they ever land |
| `BerMeter` / a validator       | LLR magnitude as a confidence readout | separates "wrong" from "wrong and sure of it"      |

**The receiver already emits everything needed and nobody can use it.**
`MpskReceiver.steps()` hands out terminal symbols — `float complex`, the
symbol *before* slicing, which is the soft information. `bits()` then throws
it away through `mpsk_demap`. Nothing in the tree turns a symbol into per-bit
soft values, so every consumer would have to write that conversion itself,
and the second one to do it would write it differently.

That is the argument for putting it in `mpsk` rather than in `fec`: it is a
property of the **constellation**, not of the code that consumes it. `mpsk` is
where the constellation lives and where the library's one decision rule
(`mpsk_slice`) already is.

______________________________________________________________________

## 2. Design goals

1. **It must not become a second decision rule.** The repository has exactly
    one — `mpsk_slice` — and `mpsk_demap`, `mpsk_diff_demap`,
    `mpsk_receiver_core.c` and `mpsk_rx_loops.h` all decide through it. A soft
    demapper that disagrees with it anywhere is a fork, and forks drift.
    Concretely: **the sign of the LLR must reproduce `mpsk_demap`'s bit, at
    every M and every SNR, including where the decision is nearly a coin
    toss.**

1. **One convention, stated.** The LLR sign convention is a coin flip that
    every consumer has to agree with, so it is fixed here and pinned by a
    test rather than described in a comment.

1. **Match the sibling's shape — where the sibling's reason applies.** `mpsk`
    splits its surface two ways: inline per-symbol helpers for a receiver to
    compose (`mpsk_slice`, `mpsk_constellation`), and array free functions for
    the Python face (`mpsk_map`, `mpsk_demap`). The soft path takes the array
    half. §6 is where the other half is declined, and it is declined on the
    sibling's own reasoning rather than in spite of it.

1. **No allocation, no state.** Memoryless and element-wise, like `mpsk_demap`
    — so no state triplet, no telemetry, and phase 6 of the lifecycle is
    genuinely empty rather than skipped.

______________________________________________________________________

## 3. The convention

For each bit `i` of a symbol's Gray label:

```text
L_i = log( P(b_i = 0 | y) / P(b_i = 1 | y) )
```

**Positive means bit 0**, so the hard decision is `bit = (L < 0)` and goal 1
is a testable identity rather than a hope. Bits are LSB-first, matching how
the label byte already packs them.

Under AWGN with `y = a + n`, `E[|n|^2] = N0`, and unit-amplitude points, the
max-log form is

```text
L_i = ( min_{a: b_i(a)=1} |y - a|^2  -  min_{a: b_i(a)=0} |y - a|^2 ) / N0
```

`n0` is a parameter. It has to be, for the value to be an LLR rather than a
monotone score — but see §4 for what actually depends on it.

______________________________________________________________________

## 4. What the prototype settled, and what it did not

A throwaway prototype (scratch, **not committed**, per the lifecycle's phase 1)
was run against the shipped `mpsk_map`/`mpsk_demap` rather than a numpy
re-derivation of them. Four questions; three came back decisive.

**Q1 — does the LLR's sign reproduce `mpsk_demap`?** Yes: **zero mismatches**
over 20 000 symbols at each of M ∈ {2, 4, 8} × Es/N0 ∈ {−3, 0, +6, +20} dB,
for both the exact and the max-log form. Including −3 dB, where the decision is
nearly a coin toss and a sign disagreement would be most likely. Goal 1 is
achievable and becomes the headline C assertion.

**Q2 — BPSK and QPSK have exact closed forms, and need no search at all.**
With `phi0 = pi/4`, the QPSK grid is axis-separable and the two bits fall out
as independent BPSK decisions:

| M   | bit | closed form         | agreement with the general path |
| --- | --- | ------------------- | ------------------------------- |
| 2   | 0   | `4·Re{y}/N0`        | max abs err 1.8e-14             |
| 4   | 0   | `4·(1/√2)·Re{y}/N0` | max abs err 1.2e-14             |
| 4   | 1   | `4·(1/√2)·Im{y}/N0` | max abs err 1.4e-14             |

(float64 round-off). So two of the three constellations are **one multiply per
bit** — no distances, no minimum, no exponential. This is a real result and it
shapes the implementation: the general path exists for 8PSK.

**Q3 — max-log costs nothing at M = 2 and M = 4, and something at M = 8.**
Exactly nothing: the two forms agree to 0.0000 at M ∈ {2, 4}, which follows
from Q2 — with one constellation point in each bit subset per axis, the
max-log maximum *is* the sum. At M = 8 they differ:

| Es/N0 | mean abs ΔL | median relative | max abs ΔL |
| ----- | ----------- | --------------- | ---------- |
| 0 dB  | 0.211       | 14.4 %          | 0.701      |
| 3 dB  | 0.223       | 9.4 %           | 0.702      |
| 6 dB  | 0.170       | 3.1 %           | 0.702      |

**Q4 — the LLR is exactly linear in `1/N0`** (max abs err 0.0e+00 across a
4× change). So `n0` is a pure scale: a caller who does not know it can pass
1.0 and rescale later, and **a Viterbi is invariant to it entirely**, since a
maximum-likelihood path does not move under a positive scaling of every branch
metric.

______________________________________________________________________

## 5. The unknowns — named now, measured in phase 7

The lifecycle asks for these to be written down before the sweep exists, so
the sweep is not designed to confirm a decision already taken.

1. **What does max-log cost at 8PSK, in dB?** §4 measures it in nats of LLR
    error, which is not a unit anyone sizes a link in. The honest number is an
    Eb/N0 offset on a decoded BER curve, and it cannot be measured until the
    decoder exists. Literature says 0.1–0.2 dB for Gray-mapped 8PSK; this
    repository does not ship literature numbers. **Until it is measured, the
    header will not claim a figure.**

1. **Is max-log the right shipped default, or should exact be selectable?**
    Deferred deliberately and follows from unknown 1. Adding a switch before
    knowing whether the difference is 0.05 dB or 0.5 dB is designing for a
    hypothetical.

1. **How should `n0` be estimated by a real caller?** The `snr` module
    estimates SNR and `MpskReceiver` carries an AGC that normalises amplitude,
    so `Es` is not free-floating — but nothing currently hands a decoder an
    `N0`. Q4 says a Viterbi does not care, which is why this is an unknown and
    not a blocker. It becomes real for any soft-input outer code.

1. **Differential.** `mpsk_diff_demap` decides from a phase *difference*, and
    a soft differential demapper is a genuinely different derivation (the two
    symbols' noise is correlated through the shared reference). **Out of
    scope**, and it will be an issue rather than a paragraph if a caller
    appears.

______________________________________________________________________

## 6. Implementation sketch

One algorithm, in C, once. It composes `mpsk_constellation` rather than
re-deriving the grid — the exact defect
[the campaign found in `test_carrier_mpsk_core.c`](mpsk.md), where a private
nearest-point search meant a test scored against its own slicer.

```text
void mpsk_soft_demap (const float complex *x, size_t x_len, float *llr,
                      size_t llr_len, int m, float n0);
```

**One function, not two, and the general path only.** Goal 3 said "match the
sibling's shape", which for `mpsk_slice`/`mpsk_demap` means an inline
per-symbol helper *and* an array face. Writing it out killed both halves of
that:

- **No inline form, because no caller composes one.** `mpsk_slice` is inline
    because a carrier loop calls it per symbol inside a hot loop. The caller
    here is a decoder, which consumes a block. Worse, an inline per-symbol
    form would have to rebuild the constellation on every call —
    `mpsk_constellation` is a `cos`/`sin` pair per point — where the array
    form builds the M points **once per call** and then walks them. Adding it
    now would be filling a hole nobody is standing in, with the slow shape.
- **No closed-form fast paths, because that is two implementations of one
    primitive.** The repository's hardest rule is that two peer
    implementations of the same thing must not exist side by side; they drift,
    and a fix applied to one silently leaves the other wrong. Q2's closed
    forms are worth more as **test assertions than as code**: as code they are
    a second expression to keep in step, and as a test they are external truth
    that proves the general path right and pins `phi0 = pi/4` as the reason
    QPSK separates at all.

So the shipped path walks the M points for every M, and Q2 becomes evidence
rather than a branch. M ≤ 8, so the walk is at most eight squared distances
per symbol against a table built once.

- `llr` receives `x_len * mpsk_bps(m)` floats, symbol-major and LSB-first
    within each symbol — the layout `wfm_frame_bits` and the `fec` kernels
    already pass bits around in. It is a caller-provided **out-param** with
    its own length, following `kaiser_window`, because jm's `out_type` binding
    sizes a function's output array 1:1 with its input and this one expands by
    `log2(M)`. `llr_len` is a capacity: too small and nothing is written.
- No state, no allocation, no error path: `m` outside {2, 4, 8} gives
    `mpsk_bps(m) == 0`, so the caller writes nothing, which is the existing
    convention in this header rather than a new one.

**Phase 2 (Declare)** is `[[module.mpsk.functions]]` in `just-makeit.toml` —
`mpsk` is a function-only module with no `objects/mpsk.toml`, so the array
face is declared there and `jm apply` generates the binding and the `.pyi`.

**Phase 4 (Pin)** — the C assertions, each proven by sabotage, in
`test_mpsk_core.c` beside §5b (which already proves the slicer's
nearest-in-phase and nearest-Euclidean agree):

1. **Sign reproduces `mpsk_demap`** at every M, swept across Es/N0 through the
    coin-toss region. This is goal 1 and the only one whose failure is
    structural rather than numeric.
1. **The closed forms** of Q2, as equalities, at M = 2 and M = 4 — which also
    pins `phi0 = pi/4` as the thing that makes QPSK separable. Change the
    grid and this test is what says so.
1. **Linearity in `1/N0`** (Q4), so the scaling convention cannot drift.
1. **Magnitude is monotone in confidence**: a symbol on a decision boundary
    reads ≈ 0 and a symbol at a constellation point reads maximal. Without
    this a demapper returning `±1` everywhere passes 1 and 3.

______________________________________________________________________

## 7. What this is not

- Not a decoder. It produces the input one wants.
- Not a demodulator. `MpskReceiver` owns carrier, timing and AGC; this takes
    the symbol it already emits.
- Not differential (§5, unknown 4).
- Not a quantiser. Fixed-point/8-bit branch metrics are a decoder-side
    concern, and a decoder that wants them can quantise a float LLR.

______________________________________________________________________

## See also

- [MPSK Receiver](mpsk.md) — the object that emits the symbols this consumes
- [Adding an Algorithm](../dev/adding-algorithms.md) — the lifecycle this follows
- [Object Validation](../dev/validation.md) — phases 7 and 8, where the
    unknowns of §5 get answered
