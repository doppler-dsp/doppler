# M-PSK Receiver — Performance Characterisation

![Eight-panel Monte-Carlo characterisation of MpskReceiver and MpskReceiverR](../assets/mpsk_receiver_performance_demo.png)

This is the receiver's characterisation, not a demo of it. Everything on the page
is measured over **randomly drawn geometries** rather than a fixed grid, on both
[`track.MpskReceiver`](../api/python-track.md) (complex baseband) and
[`track.MpskReceiverR`](../api/python-track.md) (real IF) — see the
[walkthrough page](mpsk-receiver.md) for what they are and the
[design note](../design/mpsk.md) for how they work.

Each trial independently draws the constellation order, `m_out`, a **non-integer**
samples-per-symbol, the IF placement, both loop bandwidths, the frequency and
clock offsets, the absolute level, and the data. That is the point:

!!! quote "Why random draws, not a grid"

    A grid measures the geometries you thought of. Random draws over the
    documented input domain either show you a clean distribution or hand you the
    outlier — and that is exactly how a real one hid here for weeks: at
    `sps = 10` with `m_out = 4` and an IF at 0.10, the occupied band reaches DC,
    where the real front end's image rejection collapses, and EVM falls to
    −4 dB. No grid of "reasonable" cases drew it.

## The measurement rules the panels obey

These are not incidental — get any of them wrong and the numbers look like DSP
defects. They are the same rules the [design note](../design/mpsk.md) and the
receiver's test harness encode.

| Rule                                      | Why                                                                                                                                                                                                                                                                                                                |
| ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Settling = `2·(5/bn_t + 5/bn_c)`**      | 5/Bn per loop; the two **add** because they are cascaded (the carrier discriminator reads the on-time strobe, so it cannot converge until timing has); then double for joint tracking, where each loop sees the other's transient. Measuring from `5/bn` alone reads −9.0 dB where the settled answer is −23.2 dB. |
| **Offsets INSIDE the loop bandwidth**     | Seeded on truth, the loop never leaves its initial state and any lock time is meaningless. Asserted outside `Bn`, the test measures luck. Measured: carrier lock in 39 symbols at `0.25·Bn`, 1376 at `1·Bn`, **never** at `2·Bn`.                                                                                  |
| **Loop SNR ≈ 20 dB, `bn ≤ 0.01`**         | `ρ_L = Es/N0 + squaring_loss − 10log₁₀(bn)`. Below ~20 dB the loop is driven by noise: the estimate random-walks and the receiver **declares lock while producing chance-level symbols**. If you are short of it, narrow `bn` — never accept less.                                                                 |
| **Never widen `bn` for a shorter record** | `bn` is normalised to the **symbol** rate, so settling is a fixed number of symbols at any sample rate. Widening does not buy samples, it changes the receiver. A heavily oversampled case simply needs millions of samples — that is what [`wfmgen`](wfmgen.md) is for.                                           |
| **Anchor at SER = 1e-3**                  | Per-M that is 6.8 / 10.3 / 15.7 dB, which asks "does it meet its bound" at the same place on the curve for every order, instead of at one arbitrary Es/N0.                                                                                                                                                         |

## The three panels

**EVM vs the coherent bound.** Self-referenced EVM — each symbol against its own
hard decision — against `EVM_dB = −(Es/N0)_dB`. EVM is an I/Q-plane quantity, so
there is no factor of two, and an EVM *beating* the bound means the measurement
is wrong. Median margin over the whole random sweep: **+0.1 dB**.

**SER vs the theoretical M-PSK bound.** Truth-referenced, with a lag and rotation
search, red for the real IF and blue for complex baseband. Paired with EVM
deliberately: a bit error rate alone is fragile in both directions, and it is the
**disagreement** between a truth-referenced rate and a truth-free EVM that
carries the diagnosis — see the warning below for a case where that disagreement
was the whole story.

**Lock time against each trial's own budget.** Not an absolute symbol count — a
*fraction* of that trial's `2·(5/bn_t + 5/bn_c)`, which is the only way to
compare trials with different bandwidths.

!!! note "Five more panels exist, on demand"

    `--only falsealarm,level,invariance,chunking,telemetry`, or `--only all`.
    They are not in the committed figure because a plot is the wrong shape for
    what they measure: level invariance and sample-rate invariance are flat
    lines, false alarm is a count of zero, chunking is a bar chart of exact
    zeros, and telemetry is two bars. All five are **assertions** now —
    `src/doppler/track/tests/test_mpsk_receiver_performance.py` for false alarm,
    level invariance and the coherent bound, and the `bench_mpsk_receiver*.py`
    pair for the telemetry cost (+10.5% complex, +11.6% real). A pass/fail
    property belongs in a test, where it runs on every commit, rather than in a
    figure nobody re-reads.

!!! warning "A rotation-blind metric cannot check your measurement window"

    Self-referenced EVM and the hard-decision phase error both estimate the
    constellation rotation from the data, so a constellation that is still
    *rotating* reads clean on both while decoding to the wrong symbols. Measured
    on 8PSK: EVM within 0.3 dB of the bound, phase-error mean −0.0002 rad, and
    **not one symbol beyond the ±π/8 decision boundary** — beside an SER six
    times the bound. Two independent truth-free validators agreed with each
    other and both were blind to it.

    The cause was the window, and specifically the **handover**: with
    `acq_to_track` enabled it fires on carrier lock plus a warmup, which is later
    than the analytic `5/Bn` budget *and* later than every lock indicator, and
    the decision-directed loop then has its own transient. The handover landed at
    symbol 2525 against a 2000-symbol budget; measuring from 2000 read 5.95× the
    bound where the settled answer is **1.68×**. Localise a suspected window
    fault by asking **where** the errors are — an error rate per block across the
    record — not by adding another truth-free metric.

## Streaming a real capture in

The receivers are streaming objects: state carries across calls, so a capture
arrives in whatever blocks your transport hands you.

```python
--8<-- "src/doppler/examples/mpsk_receiver_performance_demo.py:chunking"
```

## Reproduce

Every panel is independently selectable, so a single question is cheap to ask:

```bash
# the three panels above (the default, and what the figure is)
python src/doppler/examples/mpsk_receiver_performance_demo.py

# every panel, including the five pass/fail ones
python src/doppler/examples/mpsk_receiver_performance_demo.py --only all

# one question at a time
python src/doppler/examples/mpsk_receiver_performance_demo.py --only falsealarm

# more draws for a tighter distribution
python src/doppler/examples/mpsk_receiver_performance_demo.py --trials 400
```

!!! note "Not run by the examples gate"

    This script is listed in `src/doppler/examples/.examples-skip`. Its Monte
    Carlo asserts the coherent bound, and 8PSK at `m_out < 8` genuinely misses it
    (measured 8.1 dB of loss at `m_out = 4`, `sps = 10.73`), so pass/fail depends
    on the draw. Everything it measures about the *shipped defaults* is covered by
    `src/doppler/track/tests/`, which does run in CI.
