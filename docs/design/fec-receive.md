# The FEC Receive Half

The decoder, the synchronization it needs first, and the lock it owes its
caller.

`fec/` encodes today and decodes nothing, so nothing can measure a coded link
and coding gain is unquotable. This page is phase 1 of
[Adding an Algorithm](../dev/adding-algorithms.md) for the other half.

It exists because the receive chain is **not the encoder run backwards**. Two
things about it were guessed wrong in a first sketch and corrected by
measurement before any C was written; §4 is both.

______________________________________________________________________

## 1. The use case

A CADU arrives as channel symbols and a caller wants frames. Between those two
points sit four stages, and only one of them is arithmetic anybody argues
about:

| stage          | what it needs                                          | exists?                     |
| -------------- | ------------------------------------------------------ | --------------------------- |
| soft demapping | per-bit LLRs from the constellation                    | **yes** — `mpsk_soft_demap` |
| node sync      | which symbol starts a `(C1, C2)` pair                  | no                          |
| inner decode   | soft-decision Viterbi, K = 7, r = 1/2                  | no                          |
| ASM search     | the marker in the **decoded** bits, and its complement | no                          |
| derandomise    | XOR the published sequence                             | **yes** — involutive        |
| outer decode   | R-S (255,223) E = 16, de-interleaved                   | no                          |

The order is not the encoder's reversed, and the reason is the ASM: it is
inserted *third* on transmit and covered by the inner code, so a receiver must
**Viterbi-decode first and look for the marker in the decoded bits.** That one
fact is what `fec_frame_layout_t`'s span-per-stage was built to express, read
right to left.

______________________________________________________________________

## 2. Why soft, and why it came first

A hard-decision Viterbi throws away most of the gain the code exists to
deliver — at 3 dB Eb/N0, two orders of magnitude, and below about 3 dB it is
worse than no coding at all. The measurements are in
[The Viterbi Decoder](viterbi.md) §5, which owns them; they are the reason
[Soft Decisions for M-PSK](mpsk-soft.md) landed before this page rather than
after it.

______________________________________________________________________

## 3. The shape

```text
symbols --> [node sync] --> [Viterbi K=7 r=1/2] --> bits --> [ASM search]
                 ^                   |                            |
                 |                   v                            v
            re-encode  <------  decisions                    polarity
            disagreement                                     resolved
                 |
                 v
             [lockdet]  --> locked / not
```

**Node sync, the re-encode metric, and lock detection are one mechanism**, not
three. Decode, re-encode the decisions, and compare against what arrived. That
comparison needs no truth — it scores the decoder's own output against its own
input, so it works on a real capture — and it answers all three questions at
once: which grouping is right, whether the decoder is locked, and what the
channel symbol error rate is.

This is the **re-encoding method**, and it has a literature: Mengali,
Pellizzoni and Spalvieri,
*[Soft-Decision-Based Node Synchronization for Viterbi Decoders](https://ieeexplore.ieee.org/document/412728/)*,
IEEE Trans. Commun. **43**(9), pp. 2532–2539, September 1995. They derive the
optimum in-sync/out-of-sync discriminator for a soft received sequence, find
it too complex for practice, and give a suboptimum algorithm that is a
modified re-encoding method implementable outside the decoder. **§6 unknown 1
is that this design does not yet implement their statistic** — and a naive
soft variant measured *worse* than the plain hard count, which is precisely
why the form is an open item rather than a guess.

`lockdet` is the library's shipped hysteretic detector and this feeds it
rather than growing a private one, for the reason
[the campaign records](lock-detect.md): a second lock rule is a second thing
to size, calibrate and get wrong.

______________________________________________________________________

## 4. What the prototype settled about the CHAIN

Throwaway, in scratch, **not committed**. It decoded symbols dumped from the
**shipped** `fec_conv_encode` rather than from a re-derivation of it, so every
number below is measured against the encoder that will be on the other end.

The decoder's own results — that the trellis reproduces the shipped encoder,
the traceback depth, the coding gain — belong to
[The Viterbi Decoder](viterbi.md) and are not repeated here. What follows is
what the prototype said about the chain, and both of them **refuted** a first
sketch.

### The two things it refuted

**Polarity cannot be resolved by the decoder, so node sync must not try.**
The first sketch had node sync searching four hypotheses — symbol phase ×
polarity — and picking the best. It cannot work, because
[the code is transparent](viterbi.md): an inverted symbol stream is an exact
codeword of the inverted bit stream, so both polarity hypotheses score
identically by construction.

So the re-encode metric reads the *same* under both polarities, and the
prototype shows it doing exactly that:

| hypothesis          | 2 dB       | 5 dB       |
| ------------------- | ---------- | ---------- |
| phase 0, polarity + | **0.1193** | **0.0548** |
| phase 0, polarity − | 0.1197     | 0.0532     |
| phase 1, polarity + | 0.2112     | 0.1991     |
| phase 1, polarity − | 0.2142     | 0.2051     |

**Phase separates by roughly 2× and 4×; polarity does not separate at all.**
A four-way search would have been choosing polarity from a 0.0004 difference,
i.e. from noise, and would have looked like it worked about half the time.

**Therefore: node sync resolves PHASE only (two hypotheses), and polarity is
resolved downstream by the ASM correlation, which must test the marker AND its
complement.** That is why real CCSDS receivers correlate for both, and it is
another consequence of the ASM sitting inside the inner code rather than in
front of it.

______________________________________________________________________

## 5. Sizing the detector — and BOTH ways it can be wrong

A hysteretic detector has two error probabilities and they cost different
things:

|                    | what it is                        | what it costs                                                      |
| ------------------ | --------------------------------- | ------------------------------------------------------------------ |
| **P_false_lock**   | declare in-sync while out of sync | frames never arrive; the receiver looks alive and produces nothing |
| **P_false_unlock** | drop sync while genuinely in sync | a re-acquisition and a frame outage, **for no reason**             |

The second is the one a working link actually suffers and the one nobody
sizes. Both are computable once the statistic's distribution over a decision
window is measured, so it is measured.

**The statistic over a window, at 2 dB Eb/N0, 400 trials:**

| window   | in-sync         | out-of-sync     | gap / σ |
| -------- | --------------- | --------------- | ------- |
| 64 bits  | 0.2766 ± 0.0228 | 0.3608 ± 0.0486 | 3.70    |
| 128 bits | 0.2206 ± 0.0205 | 0.3271 ± 0.0451 | 5.20    |
| 256 bits | 0.1750 ± 0.0132 | 0.2959 ± 0.0369 | 9.13    |

The in-sync mean falls with the window because a traceback of 60 leaves most
decisions inside the edge transient at 64 bits — the statistic is measuring
the harness there, not the link. **256 bits is where the separation stops
being marginal.**

**Both probabilities against the drop threshold, 128-bit window:**

| threshold | P_false_unlock (gaussian / empirical) | P_false_lock (gaussian / empirical) |
| --------- | ------------------------------------- | ----------------------------------- |
| 0.20      | 8.42e-1 / 8.33e-1                     | 2.44e-3 / < 2.5e-3                  |
| 0.22      | 5.11e-1 / 4.65e-1                     | 8.84e-3 / < 2.5e-3                  |
| 0.25      | 7.51e-2 / 6.50e-2                     | 4.39e-2 / 2.25e-2                   |
| 0.30      | **5.17e-5** / **2.50e-3**             | 2.74e-1 / 3.12e-1                   |

**Do not size the drop threshold from a Gaussian.** At 0.30 the Gaussian
underestimates P_false_unlock by **48×**, and it errs *optimistically* — it
promises a link that drops lock once in 20 000 windows where the measurement
says once in 400. That is the tail behaviour
[the campaign has hit before](lock-detect.md): a detector lives in its H0
tail, and the tail is exactly where a Gaussian assumption stops being
conservative. The empirical floor here is 1/400 = 2.5e-3, so the true number
at 0.30 is *at most* known to be ≥ that — which is itself the point, and
resolving it needs the trials a phase-7 sweep can afford.

______________________________________________________________________

## 6. The unknowns — named now, measured in phase 7

1. **The discriminator's exact form.** §3 cites Mengali et al. for the
    soft-decision statistic; this design has **not** implemented it. A naive
    soft variant — `mean(s·L) / mean(|L|)`, the fraction of available soft
    evidence the survivor collected — was measured against the plain hard
    count and **separates about 2× wider yet decides worse at short
    observations**: over 200 trials at 2 dB it got 18 decisions wrong at 64
    bits against the hard count's 9, and 9 against 4 at 128. So the naive soft
    form is not the paper's, and adopting "soft is better" on the strength of
    a title would have made the synchronizer worse. Either implement their
    statistic or keep the hard count and say why.
1. **P_false_unlock below 2.5e-3**, which needs more trials than a prototype
    should spend (§5).
1. **How long node sync takes to declare** — §5's windows say 256 bits
    separates cleanly; what a caller needs is the acquisition time including
    hysteresis.
1. **Whether the ASM search wants the soft symbols or the decoded bits.**
    Decoded bits are simpler and are what the layering implies. A soft
    correlation is stronger and would couple the search to the decoder's
    internals. Unmeasured, so undecided.
1. **The R-S decoder's failure behaviour beyond E = 16.** A (255,223) E = 16
    decoder can *miscorrect* rather than refuse. What it does at 17+ errors is
    a property a caller has to know, and it is measurable against the code
    itself rather than against an implementation.

______________________________________________________________________

## 7. Sequence

1. **The Viterbi**, with node sync over the two phase hypotheses and the
    re-encode metric. External truth is **not** a round trip — that is what
    this whole slice refuses — but `d_free = 10` and the BER curves
    **CCSDS 130.1-G prints**.
1. **R-S decode** — syndromes (already in `fec_rs_codeword_ok`), then
    Berlekamp-Massey, Chien and Forney, over the same field, roots and dual
    basis the encoder uses. External truth: corrects exactly 16 symbol errors
    and fails at 17.
1. **`fec_frame_decode`** — the chain, mirroring `fec_frame_encode`'s spans,
    with the ASM search resolving polarity.
1. **Coding gain**, through the receiver battery: coded against uncoded,
    against 130.1-G. That is the measurement the whole slice is for, and it is
    also `fec`'s certification evidence.

______________________________________________________________________

## See also

- [Soft Decisions for M-PSK](mpsk-soft.md) — the LLRs this consumes
- [Lock Detection](lock-detect.md) — the detector this feeds
- [Adding an Algorithm](../dev/adding-algorithms.md) — the lifecycle
