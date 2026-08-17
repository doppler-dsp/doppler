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
is that this design does not implement their statistic and has not evaluated
it** — the hard count is chosen for being the cheapest form that measured no
worse, not for being better.

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

### The operating point

**Window = 500 channel symbols, threshold = 100 disagreements (20 %).** Given
from operational experience rather than derived here — so what it buys is
measured, which is what turns a working number into a recorded one. 1000
trials per cell:

| Eb/N0 | theory `p_s`·500 | in-sync count | out-of-sync count | P_false_unlock | P_false_lock |
| ----- | ---------------- | ------------- | ----------------- | -------------- | ------------ |
| 1 dB  | 65.5             | 66.6 ± 10.6   | 125.5 ± 22.8      | 0.015          | 0.127        |
| 2 dB  | 52.0             | 52.5 ± 7.1    | 126.3 ± 24.2      | < 1e-3         | 0.133        |
| 3 dB  | 39.4             | 39.7 ± 6.0    | 126.8 ± 24.4      | < 1e-3         | 0.155        |
| 4 dB  | 28.2             | 28.4 ± 5.2    | 125.6 ± 25.1      | < 1e-3         | 0.166        |
| 5 dB  | 18.8             | 18.8 ± 4.4    | 124.5 ± 26.2      | < 1e-3         | 0.188        |

Three things this says, and only one of them was expected:

1. **The in-sync statistic IS the channel symbol error rate**, to within a
    count: 66.6 against 65.5 predicted, 18.8 against 18.8. §3's claim that the
    same number serves as sync metric, lock statistic and channel quality is
    not a convenience — it is the same quantity three times.
1. **Against false UNLOCK the threshold is excellent**: 1.5 % at 1 dB and
    below the 1e-3 resolution from 2 dB up. It sits ~7σ above the in-sync mean
    at 2 dB, which is why.
1. **Against false LOCK a single window is marginal** — 12.7 % to 18.8 %,
    because the out-of-sync distribution is ~125 ± 25 and the threshold is
    about **1σ below its mean**. Note it barely moves with Es/N0: an
    out-of-sync decoder is finding a path through what is effectively noise,
    so its disagreement rate is a property of the code and not of the channel.

**That asymmetry is the argument for hysteresis, not against the threshold.**
`lockdet` requires a run of consistent looks before it moves, and k
consecutive windows drive a 15 % per-window false lock to 15 %^k — 0.3 % at
k = 3. Holding lock and acquiring it are different jobs with different error
budgets, and this operating point is sized for the first; the second is what
the up-counter is for.

### A harness artifact that looked like a result

The first run of this table put the in-sync count **45 % above** the
theoretical symbol error rate (95 against 65.5 at 1 dB, 55 against 18.8 at
5 dB) and every number was internally consistent. The cause was the
measurement window overlapping the streaming decoder's **undecided tail** — a
traceback of 60 leaves the last 60 bits undecided, and they were inside the
window. It was measuring the harness, not the link, and it would have been
recorded as "the statistic runs above `p_s`, and the gap widens with SNR",
which is a plausible-sounding finding about nothing. The assertion that the
window lies inside the decided region is now part of the measurement.

______________________________________________________________________

## 6. The unknowns — named now, measured in phase 7

1. **The discriminator's exact form, which is UNEVALUATED.** §3 cites Mengali
    et al. for the soft-decision statistic. This design does not implement it,
    and nothing here measures it — only the abstract has been read.

    What was measured is three *ad-hoc* comparators over the two phase
    hypotheses: the hard disagreement count, `mean(s·L)/mean(|L|)`, and the
    plain correlation `sum(s·L)`. Wrong decisions out of 300 trials at 2 dB:

    | usable bits | hard | `mean(s·L)/mean(|L|)` | `sum(s·L)` |
    | \--- | --- | --- | --- |
    | 32 | 17 | 18 | 18 |
    | 64 | 6 | 7 | 7 |
    | 128 | 0 | 0 | 0 |

    **No difference this test can resolve.** So the hard count is chosen
    because it is the cheapest of three that measured the same — no multiplies
    — and *not* because soft decision loses. An earlier run of this table did
    show the soft forms losing (18 against 9 at 64 bits) and **that was the
    undecided-tail artifact of §5**, found and retracted rather than shipped.
    Whether the paper's derived statistic beats all three at short
    observations is open, and is the question worth answering if node sync
    ever needs to declare faster than 64 bits.

1. **P_false_lock under hysteresis.** §5 measures a single window; what a
    caller acquires with is `lockdet`'s run-length, and the geometric estimate
    (15 %^k) assumes independent windows, which consecutive windows of a
    streaming decoder are not. Measure it rather than multiply it.

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

1. **Coding gain, through the harness that already exists.** Coded against
    uncoded, against 130.1-G. This is the measurement the whole slice is for
    and it is also `fec`'s certification evidence — and it is **not** a new
    sweep. [The Receiver Test Harness](rx-test.md) is the inventory of what a
    receiver measurement rests on, and the instrument built on it
    (`dp_rx_test.h`, `rx_battery.c`) already owns the stimulus
    (`wfm_synth` + `doppler_channel`), the statistics with their refusals and
    confidence intervals (`dp_ber_test.h`), and the frame outcomes
    (`frame_meter`). A coded link is a **new operating point and an adapter**,
    the same way `ContinuousMpskReceiver` was — not a second harness.

    rx-test.md §5.3 names the gap this closes from the other side: *"the
    framed generator and the frame-aware measurer have never met"*. `fec` is
    the layer that makes the generator produce something the measurer was
    built to score.

______________________________________________________________________

## See also

- [Soft Decisions for M-PSK](mpsk-soft.md) — the LLRs this consumes
- [Lock Detection](lock-detect.md) — the detector this feeds
- [Adding an Algorithm](../dev/adding-algorithms.md) — the lifecycle
