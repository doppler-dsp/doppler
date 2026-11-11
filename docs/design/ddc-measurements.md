---
description: "The measured cost of the candidate down-converter architectures, a dated record."
---

# DDC Architecture — the measurement record

The record behind [DDC Architecture](ddc.md#measured-cost).

## Performance of the candidate architectures (a 2026-05-17 record)

!!! note "A dated design study, not the shipped DDC's throughput"

    These numbers come from the architecture comparison made when the
    library began (2026-05-17), before `DDC` moved onto `RateConverter`. No
    script in the tree reproduces them, and architectures A and C were never
    shipped. For the shipped DDC's throughput, measured on every release,
    see "Down-converter, decimate by 2" on
    [the benchmarks page](../benchmarks.md).

Block = 65536 samples × 200 iterations, M=3 N=19, Release build, x86-64.

Architecture C is measured here but has no section above, since B beats it
at every rate. It is B's order reversed, a wide-band (0.2/0.8) resample at
rate 2r *before* the halfband:
`LO ──► polyphase resample (0.2/0.8, rate 2r) ──► HB ÷2`. The
wider transition band needs fewer taps, but the resampler emits twice the
samples and the halfband must then process all of them.

| Rate | Decimation |   Arch A |        Arch B |   Arch C |
| ---- | ---------- | -------: | ------------: | -------: |
| 0.50 | 2×         | 61 MSa/s | **335 MSa/s** | 70 MSa/s |
| 0.25 | 4×         | 70 MSa/s |  **76 MSa/s** | 62 MSa/s |
| 0.10 | 10×        | 72 MSa/s |  **97 MSa/s** | 74 MSa/s |
| 0.01 | 100×       | 85 MSa/s | **116 MSa/s** | 80 MSa/s |

Architecture B wins at every rate.
