# The Viterbi Decoder

The CCSDS inner code decoded: the trellis, the branch metric, and the one
number the implementation cannot be written without.

Phase 1 of [Adding an Algorithm](../dev/adding-algorithms.md). The chain this
sits in — node synchronization, the ASM search, the outer code — is
[The FEC Receive Half](fec-receive.md); this page owns the decoder itself.

______________________________________________________________________

## 1. What it decodes

CCSDS 131.0-B-3 section 3.3: rate 1/2, constraint length K = 7,
non-systematic, `G1 = 1111001` (171 octal), `G2 = 1011011` (133 octal), with
**symbol inversion on the G2 output path**. `fec_conv_encode` ships that
encoder and is pinned against the impulse response; this is its inverse.

The inversion is not a detail. Built without it, the trellis differs from the
shipped encoder in **400 of 800 symbols**, and a decoder that omits it decodes
**39.2 % of bits wrong** — measured, not reasoned. It is the same trap the
encoder's own test was written around, one layer out: a decoder that inverts
consistently with an encoder that inverts consistently interoperates with
nothing, and only a comparison against the *other* implementation catches it.

______________________________________________________________________

## 2. The trellis, in the encoder's own terms

`fec_conv_encode` is `reg = ((reg >> 1) | (b << 6)) & 0x7F`, so the 7-bit
register holds *(the newest bit, the six before it)*. A **state** is therefore
those six previous inputs:

```text
state st (6 bits) + new bit b   ->   reg = (b << 6) | st
                                     C1  = parity(reg & G1)
                                     C2  = parity(reg & G2) ^ 1
                                     next state = reg >> 1
```

64 states, two branches each. Deriving the state convention the other way
round — a plausible reading, and the one written first — builds a trellis that
is perfectly self-consistent and decodes nothing the shipped encoder produced.
**The check that catches it is encoding through the trellis and comparing
against `fec_conv_encode` symbol for symbol**, which is a phase-4 assertion
and was a prototype question before that.

______________________________________________________________________

## 3. The branch metric is inherited, not restated

Input is one LLR per channel symbol in the convention
[`mpsk_soft_demap`](mpsk-soft.md) ships: `L = log(P(0)/P(1))`, positive means
symbol 0. For an expected symbol `e` the metric is `+L` when `e == 0` and `-L`
otherwise, and the survivor **maximises** the sum.

Two consequences worth stating because they remove decisions:

- **Scale does not matter.** A maximum-likelihood path cannot move when every
    branch metric is multiplied by a positive constant, so the decoder does
    not need an accurate `n0` — the caller may pass 1.0 (mpsk-soft §4, Q4).
- **The sign convention is the library's**, so a decoder that agrees with
    `mpsk_demap` on hard decisions agrees with the demapper by construction
    rather than by a second convention that has to be kept in step.

______________________________________________________________________

## 4. Traceback depth — measured, because the textbook number is wrong here

The rule of thumb is `5·K = 35`. It is not enough for this code at low SNR.

The first sweep was run at 3 dB over 4 000 bits and **every depth read
`0.00000`**, which answers nothing — a depth sweep has to be run where the
answer is not zero. At **1 dB Eb/N0 over 30 000 bits**:

| traceback depth | in K    | BER         |
| --------------- | ------- | ----------- |
| 12              | 1.7     | 0.09588     |
| 20              | 2.9     | 0.06555     |
| 25              | 3.6     | 0.05413     |
| 30              | 4.3     | 0.04630     |
| **35**          | **5.0** | **0.04178** |
| 45              | 6.4     | 0.03542     |
| 60              | 8.6     | 0.03237     |
| 90              | 12.9    | 0.03150     |
| 120             | 17.1    | 0.03137     |

The floor is ≈ 0.0314, and **`5·K` sits 33 % above it**. Depth 60 (8.6·K) is
within 3 %, depth 90 within 0.5 %.

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
[soft demapping](mpsk-soft.md) having been built first rather than bolted on.

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
    has the same defect the assembler had before it carried `fec_conv_t`.
- **64 path metrics, a decision bit per state per time**, and a traceback
    window of `depth` (§4). Storage is `depth × 64` bits for the decisions —
    a few hundred bytes at depth 60, so the window is a fixed allocation
    rather than a growth policy.
- **The butterfly**: each next state has exactly two predecessors,
    `(ns << 1) & 63` and `| 1`, both with the same input bit `ns >> 5`. That
    structure is what makes the update branch-free and is worth writing down
    because it is easy to rediscover incorrectly.
- **No exceptions to the error convention**: a decoder fed a short buffer or
    an unsupported depth writes nothing, as `fec_frame_encode` does.

______________________________________________________________________

## 8. The unknowns — measured in phase 7

1. **Where this sits against CCSDS 130.1-G's published curves.** §5's numbers
    are ours; the Green Book prints the reference performance, and agreeing
    with it is external truth that no round trip can provide. Until then this
    page does not claim a coding gain in dB.
1. **Whether `d_free = 10` is exhibited.** The code's free distance is a
    property of the code, so it is checkable against the implementation rather
    than against another implementation — the strongest kind of assertion
    available here.
1. **The cost of depth in a real budget.** §4 gives BER against depth; what a
    caller trades is latency and memory, and neither is measured yet.

______________________________________________________________________

## See also

- [The FEC Receive Half](fec-receive.md) — the chain, node sync, lock detection
- [Soft Decisions for M-PSK](mpsk-soft.md) — the LLRs this consumes
