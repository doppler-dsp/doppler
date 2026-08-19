# The Viterbi Decoder

A maximum-likelihood sequence decoder for rate-1/n convolutional codes: the
trellis, the branch metric, and the one number the implementation cannot be
written without.

Phase 1 of [Adding an Algorithm](../dev/contributing/adding-algorithms.md). The chain it
was built for — node synchronization, the ASM search, the outer code — is
[The FEC Receive Half](fec-receive.md); this page owns the decoder.

**It is a general decoder, and CCSDS is a configuration of it — and the same
is true of the encoder.** The code that
motivated it is 131.0-B-3's K = 7, rate 1/2 inner code, and everything below is
measured on that — but the constraint length, the number of outputs, the
generator polynomials and which outputs are inverted are all *parameters*. A
decoder that hard-codes one code is a decoder that cannot be pointed at the
deep-space rate-1/6 code, at a K = 9 experiment, or at anything a caller brings
of their own, and the algorithm is identical in every one of those cases. The
only thing CCSDS-specific in the tree should be the *configuration*, which is
`ccsds_tm`'s to hold.

______________________________________________________________________

## 1. What it decodes

Any rate-1/n convolutional code, given its constraint length, its generator
polynomials, and a mask saying which outputs are inverted:

| parameter |                            | CCSDS 131.0-B-3 §3.3 |
| --------- | -------------------------- | -------------------- |
| `k`       | constraint length          | 7                    |
| `n`       | outputs per input bit      | 2                    |
| `poly[]`  | generator polynomials      | `0171`, `0133`       |
| `invert`  | which outputs are inverted | bit 1 — G2 only      |
| `depth`   | traceback depth            | 60 (§4)              |

`conv_encode` ships that encoder and is pinned against the impulse
response; this decodes it, and the same object decodes anything else with a
trellis of the same family.

**The inversion has to be a parameter, not a constant.** CCSDS inverts G2;
most codes invert nothing. Built without it, the trellis differs from the
shipped encoder in **400 of 800 symbols** and decodes **39.2 % of bits wrong**
— measured, not reasoned. It is the same trap the encoder's own test was
written around, one layer out: a decoder that inverts consistently with an
encoder that inverts consistently interoperates with nothing, and only a
comparison against the *other* implementation catches it. Making it a mask
means the mistake is a wrong argument rather than a wrong decoder.

______________________________________________________________________

## 2. The trellis, in the encoder's own terms

The convention must match the encoder being decoded, and doppler's is
`conv_encode`: `reg = ((reg >> 1) | (b << (k-1))) & mask`, so the k-bit
register holds *(the newest bit, the k-1 before it)*. A **state** is those
k-1 previous inputs:

```text
state st (k-1 bits) + new bit b -> reg  = (b << (k-1)) | st
                                   c[j] = parity(reg & poly[j]) ^ invert_j
                                   next state = reg >> 1
```

2^(k-1) states, two branches each — 64 for CCSDS. Deriving the state convention the other way
round — a plausible reading, and the one written first — builds a trellis that
is perfectly self-consistent and decodes nothing the shipped encoder produced.
**The check that catches it is encoding through the trellis and comparing
against `conv_encode` symbol for symbol**, which is a phase-4 assertion
and was a prototype question before that.

______________________________________________________________________

## 3. The branch metric is inherited, not restated

Input is one LLR per channel symbol in the convention
[`mpsk_soft_demap`](mpsk.md#97-soft-decisions) ships: `L = log(P(0)/P(1))`, positive means
symbol 0. For an expected symbol `e` the metric is `+L` when `e == 0` and `-L`
otherwise, and the survivor **maximises** the sum.

Two consequences worth stating because they remove decisions:

- **Scale does not matter.** A maximum-likelihood path cannot move when every
    branch metric is multiplied by a positive constant, so the decoder does
    not need an accurate `n0` — the caller may pass 1.0 (`mpsk.md` §9.7).
- **The sign convention is the library's**, so a decoder that agrees with
    `mpsk_demap` on hard decisions agrees with the demapper by construction
    rather than by a second convention that has to be kept in step.

______________________________________________________________________

## 4. Traceback depth — measured, because the textbook number is wrong here

The rule of thumb is `5·K = 35`. It is not enough for this code at low SNR.

The first sweep was run at 3 dB over 4 000 bits and **every depth read
`0.00000`**, which answers nothing — a depth sweep has to be run where the
answer is not zero. At **Eb/N0 = 1 dB over 120 000 bits**, measured by
`native/validation/conv_certify.c` and rendered into
`src/doppler/tests/validation/conv/results.md`:

| traceback depth | in K    | BER         | above the floor |
| --------------- | ------- | ----------- | --------------- |
| 15              | 2.1     | 0.08444     | +106 %          |
| 20              | 2.9     | 0.06812     | +66 %           |
| 30              | 4.3     | 0.05195     | +27 %           |
| **35**          | **5.0** | **0.04821** | **+17 %**       |
| 45              | 6.4     | 0.04420     | +8 %            |
| 60              | 8.6     | 0.04156     | +1.2 %          |
| 90              | 12.9    | 0.04106     | —               |

The floor is ≈ 0.0411, and **`5·K` sits 17 % above it** while depth 60 is
within 1.2 %.

**An earlier prototype reported this differently and its numbers are
superseded.** It measured 0.04178 at depth 35 against a floor of 0.03137 —
33 % — at a nominally identical Eb/N0 over 30 000 bits. Every level in it
sits ~30 % below the table above, which is what a fraction of a dB of
Es/N0 convention is worth on a curve this steep, and the shape differs with
it. The prototype was explicitly throwaway and uncommitted; the harness
above is built, gated and re-run on every push, so it is the number of
record. **The decision it drove is unchanged** — 35 is measurably short of
the floor and 60 is on it — which is the useful thing to know about a
disagreement of this kind.

**Design number: 60**, with 90 available where the memory cost is acceptable.
The cost is linear in depth on both storage and traceback work, which is why
this is the number the implementation cannot be written without.

______________________________________________________________________

## 5. What it buys — and why soft came first

Measured on this code, streaming decisions at depth 100:

| Eb/N0 | uncoded BPSK | hard-decision | soft-decision |
| ----- | ------------ | ------------- | ------------- |
| 1 dB  | 5.63e-2      | 2.46e-1       | 3.18e-2       |
| 2 dB  | 3.75e-2      | 1.09e-1       | 7.84e-3       |
| 3 dB  | 2.29e-2      | 3.85e-2       | **3.52e-4**   |
| 4 dB  | 1.25e-2      | 2.86e-3       | 1.51e-4       |

At 3 dB the soft decoder is **two orders of magnitude** better than the hard
one. And the hard-decision column is **worse than uncoded below about 3 dB** —
the well-known low-SNR breakdown, and the sharpest available argument for
[soft demapping](mpsk.md#97-soft-decisions) having been built first rather than bolted on.

______________________________________________________________________

## 6. The code is transparent, and that is the decoder's business to state

Both generators have **odd weight (5 each)**, so inverting the input inverts
both outputs. An inverted symbol stream is therefore an exact codeword — of
the inverted bit stream. Measured directly: `decode(-llr)` returns exactly
`~bits`, and `encode(~b) == ~encode(b)` holds for every symbol after the
twelfth (the first six differ only because both encoders start from an
all-zero register rather than from inverted contents).

**So a polarity ambiguity passes through this decoder untouched, and no
decoder-side statistic can resolve it** — both hypotheses are exact codewords
and score identically. Resolving it belongs to the ASM search downstream, and
[The FEC Receive Half](fec-receive.md) is where that consequence lives. It is
stated here because it is a property of *this code*, not of the chain.

______________________________________________________________________

## 7. Implementation sketch

- **Streaming, with carried state**, matching the encoder: 3.3.2 fixes the
    output as one uninterrupted sequence, so a decoder that restarts per block
    has the same defect the assembler had before it carried `conv_enc_t`.
- **64 path metrics, a decision bit per state per time**, and a traceback
    window of `depth` (§4). Storage is `depth × 64` bits for the decisions —
    a few hundred bytes at depth 60, so the window is a fixed allocation
    rather than a growth policy.
- **The butterfly**: each next state has exactly two predecessors,
    `(ns << 1) & (S-1)` and `| 1` for `S = 2^(k-1)`, both with the same input
    bit `ns >> (k-2)`. That structure is what makes the update branch-free,
    holds for every k, and is worth writing down because it is easy to
    rediscover incorrectly.
- **Branch metrics once per step, not per state.** For rate 1/n there are only
    `2^n` distinct output patterns, so the `n` LLRs collapse to a `2^n` table
    computed once and indexed by each branch's output word. At n = 2 that is
    four adds for 128 branches.
- **Path metrics need renormalising.** They grow without bound on a stream;
    subtracting the running maximum each step keeps them bounded and changes
    no decision, since a common offset cannot reorder survivors.
- **No exceptions to the error convention**: a decoder fed a short buffer or
    an unsupported depth writes nothing, as `ccsds_tm_frame_encode` does.

______________________________________________________________________

## 8. The unknowns — measured in phase 7

1. **Where this sits against CCSDS 130.1-G's published curves.** §5's numbers
    are ours; the Green Book prints the reference performance, and agreeing
    with it is external truth that no round trip can provide. Until then this
    page does not claim a coding gain in dB.
1. ~~**Whether `d_free = 10` is exhibited.**~~ **MEASURED**, and it is —
    `test_conv_core.c` §6d walks the trellis the DESCRIPTION defines
    (`conv_outputs` and `conv_next_state`) for the lightest path that leaves
    the all-zero state and returns to it, and reproduces the published free
    distance of three codes: **10** for CCSDS's (171,133) K = 7, 5 for the
    K = 3 (7,5), 6 for the K = 4 (15,17). The inversion is taken back out
    first — a constant XOR on every branch cancels in the difference between
    two codewords, so it cannot move a distance, and leaving it in measures
    the weight of the all-zero path instead.
1. **The cost of depth in a real budget.** §4 gives BER against depth; what a
    caller trades is latency and memory, and neither is measured yet.
1. **Whether depth 60 travels.** §4 measured it for K = 7 rate 1/2. The rule
    of thumb scales with K, and this decoder takes K as a parameter — so the
    number is pinned for the code it was measured on and is a default, not a
    law, for anything else.

**Where all three get measured is settled**: the receiver instrument, not a
bespoke sweep. `native/validation/` for the C-only surface and the battery for
anything a receiver can be driven through — see
[The Receiver Test Harness](rx-test.md) for why that inventory exists and
[The FEC Receive Half](fec-receive.md) §7 for the sequence.

______________________________________________________________________

## 9. Node synchronization — the decoder's other input question

A rate-1/n decoder needs to know **which symbol starts a branch**. Nothing in
the waveform says so: a capture opens wherever the receiver settled, and the
answer changes mid-stream whenever the receiver slips by an odd number of
symbols. It lives here, in `conv`, for the same reason the decoder does —
every rate-1/n code has the question and CCSDS is not what supplies the
answer.

### The statistic is the decoder's own disagreement with itself

`node_sync_score` decodes the window, **re-encodes the decisions**, and counts
where the result differs from the received hard decisions. It references no
truth, no marker and no training sequence, so it works on a live capture:

| alignment | what the count is                                                                                                  |
| --------- | ------------------------------------------------------------------------------------------------------------------ |
| right     | the CHANNEL's symbol errors — the decoder corrected them, and a corrected symbol still disagrees with what arrived |
| wrong     | the best a maximum-likelihood search can do against a stream that is not on its trellis                            |

**The wrong hypothesis does not score a half**, and the difference matters
because a half is what a coin-flip argument predicts. Measured on clean
streams: **24 %** of symbols for CCSDS K = 7 r = 1/2, **23 %** for the same
code uninverted, **18 %** for a K = 5 r = 1/3 — against **0 %** for the right
one. The decision rests on that separation, not on an absolute level.

### Three properties, and each is a test

- **Blind to polarity, exactly.** A transparent code decodes an inverted
    stream to the complement, which re-encodes to the inverted symbols, so the
    count is identical. §6 is why that is the correct behaviour rather than a
    limitation: polarity does not separate under ANY statistic, and the ASM
    resolves it downstream.
- **The head of a window must be discarded.** Two cold starts overlap there —
    the comparison encoder begins at a zero register while the transmitter's
    was mid-stream (`k-1` bits), and the decoder begins from its own all-zero
    prior, which is simply wrong when the window opens mid-capture. Measured,
    the second dominates: skipping only `k-1` left three disagreements in
    1598 symbols on a CLEAN stream and broke the polarity equality. The skip
    is `viterbi_depth`, the decoder's own answer to how long its survivors
    take to be data-determined.
- **Re-runnable, and scored on the window you are about to decode.** An
    alignment is valid until the next slip. Scoring the whole remaining record
    answers "which phase fits most of it" when the question is "which phase
    fits the part I am about to decode" — measured, at Es/N0 = +1 dB a slip
    early in a record made a whole-record scan prefer the phase that was right
    for the tail, and frame sync then found no marker at the head. A few
    hundred scored symbols decide it.

### What it replaced

`native/validation/rx_coding_gain.c` used to pick the phase by which parity
put an **ASM** where an ASM could be. That worked and was the harness doing
the library's job with a statistic that exists only because CCSDS supplies a
marker. Swapping it for the re-encoding metric changed no measured number in
the coding-gain sweep — same frames, same bits, same bound — which is the
evidence that the general statistic is at least as good as the special one.

The two questions stay separate, and both are still asked: node sync says
which symbol starts a branch, the marker says where a FRAME starts and in
which polarity.

______________________________________________________________________

## See also

- [The FEC Receive Half](fec-receive.md) — the chain, node sync, lock detection
- [Reed-Solomon](reed-solomon.md) — the outer code, split the same way
- [Soft Decisions for M-PSK](mpsk.md#97-soft-decisions) — the LLRs this consumes
