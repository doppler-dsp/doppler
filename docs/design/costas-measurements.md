---
description: "the record behind Costas's design page: the runs, comparisons and figures of its second loop order (#3127)"
---

# Costas — the measurement record

The dated record behind [Costas's design page](costas.md), under the same section numbers. Text that stood on the design page until 2026-11-17 is moved here unchanged.

## 1 The bandwidth at the detector instant

for the phase at the detector instant: the impulse-response sum read
there equals `2·Bn` to six digits at `bn` 0.005 to 0.04.

## 1 Where the phase is read

**Where the phase is read matters to a jitter figure.** The NCO phase
read just after the update, past the `kp·e` step, is not the phase at the
detector instant. Its sum of squares is 1% under `2·Bn` at `bn` 0.005 and
7% under at 0.04, at both orders. `costas_jitter` reads `nco.phase` after
each update, so it holds the jitter to the sum taken at that same point.

## 3.3 The blob versions on main, the held train and #3127

| blob                           | on main | on the held train | with this work |
| ------------------------------ | ------- | ----------------- | -------------- |
| `COSTAS` (`CSTS`)              | v3      | v3                | v4             |
| `DESPREADER` (`DSPR`)          | v5      | v5                | v5, larger     |
| `DSSS_RECEIVER` (`DSRX`)       | v4      | v4                | v4, larger     |
| `ASYNC_DSSS_RECEIVER` (`ADRX`) | v17     | v19               | unchanged      |
| `ASYNC_DSSS_POOL` (`ADPL`)     | v7      | v8                | unchanged      |

The table is main's. The train this lands beside carries `ADRX` v19 and
`ADPL` v8, and a Costas child changes their size, not their versions.

## 3.4 The A/B the order-2 loop was held to

- **The proof:** `make bench-ab A=<main> B=<branch>` on the `costas`,
    `despreader`, `dsss_receiver` and `async_dsss_receiver` rows, all at
    order 2, must tie.
- **If they do not tie,** the fallback is two update functions,
    `costas_update()` unchanged and an order-3 sibling, with the embedder
    choosing. That costs a second steering path, so it is taken only on a
    measurement.

## 3.5 The one-time order-2 comparison

- `validate_costas_pullin --check` and `validate_costas_jitter --check`,
    whose order-2 rows are identical before and after this work (a one-time
    comparison, in the commit message), and the bench rows above;

## 3.5 Why the recurrence is not held bit for bit

- **Why not bit for bit:** it is a second implementation in another
    translation unit, and a compiler may contract a multiply-add into
    one rounding in one and not the other. clang and arm64 do, and gcc
    does too once the reference's loop body differs. Bit for bit, it
    failed on macOS, arm64, Windows clang-cl and the clang C job: the
    closed reference loop drifted 14 phase counts and 1.5e-9 in
    `integ` over 3,000 updates.

## 3.5 The recurrence's measured figures and sabotages

- **Measured** under gcc and clang: 0 in `integ` and in the phase, and
    prompts within 4.8e-7 of a 3.4e-4 bound.
- **The sabotages** fail it by orders: a wrong gain moves the phase
    3.9e8 counts against 51,000, and a frequency read before the step
    moves it 2.0e7 counts and the prompts 0.042 against 3.4e-4.

## 3.5 The validators' order-3 claims, before their rows

**The validators, at both orders, asserting in the sweep and in `--check`
alike** (order 2 had asserted only under `--check`, #1566):

- **`costas_pullin`:** order 3 acquires at and inside `bn/m` on both
    signs, and the bound scales with `bn`.
    - Past the bound (reported), both orders reach 2x and neither reaches 4x
        at the shared budget of 250 updates.
    - Given 1,000 updates, order 3 reaches 4x and order 2 still does not.
        So past the bound, order 3 pulls in further but slower
        ([#3123](https://github.com/doppler-dsp/doppler/issues/3123)).
- **`costas_jitter`:** order 3's linearization factor is 1.07 against
    order 2's 1.06, inside the same band. The jitter is proportional to
    `bn`, with a clear threshold. Its noise gain is the impulse-response sum
    at the point it reads the phase, after the update (§1), extended by the
    rate path.
- A wrong order-3 phase gain turns both red.
