# DSSS characterizations

How the DSSS receivers behave across their whole operating envelope: phase 7,
"Explore", of
[adding-algorithms](../../../../../docs/dev/contributing/adding-algorithms.md),
run deliberately and never per push. What the category is, why it is not an
example, and what its fast twins do and do not cover is the package
docstring's: [`__init__.py`](__init__.py).

| subject                                                                      | what it answers                                                                                                 | run                   | fast twin                            |
| ---------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | --------------------- | ------------------------------------ |
| [`async_dsss_receiver/characterize.py`](async_dsss_receiver/characterize.py) | does `AsyncDsssReceiver` stay finite, locked and decoding far above its design Es/N0?                           | `make characterize`   | `test_async_dsss_receiver.py`        |
| [`async_dsss_receiver/telemetry.py`](async_dsss_receiver/telemetry.py)       | what one receiver and one `AsyncDsssPool` cost per push, and what their loops and searcher do, on one capture   | `make pool-telemetry` | `test_async_dsss_pool.py`            |
| [`dll_lock/characterize.py`](dll_lock/characterize.py)                       | what the `Dll`'s code-lock flag needs at the continuous async-DSSS operating point                              | `make characterize`   | `test_dll_lock_twin.py`              |
| [`dsss_burst_receiver/characterize.py`](dsss_burst_receiver/characterize.py) | what loses a burst: where it lands against acquisition's framing, and where refine stops naming the right start | `make characterize`   | `test_dsss_burst_receiver_stress.py` |
| [`dsss_receiver/characterize.py`](dsss_receiver/characterize.py)             | `DsssReceiver` across C/N0, Doppler, sample rate, amplitude and data                                            | `make characterize`   | `test_dsss_receiver_stress.py`       |

Each script writes its artifacts beside itself through `doppler.util.fromto`,
whatever directory the run starts in, or wherever it is pointed (a path
argument, or `telemetry.py --out`). They are working files and gitignored.
`make pool-telemetry` points `telemetry.py` at `build/pool-telemetry/`.

## Results

One run of each, on one machine:

|          |                                                                                               |
| -------- | --------------------------------------------------------------------------------------------- |
| date     | 2026-10-17, 02:34 to 02:39 UTC                                                                |
| machine  | `swiftgo-ai9465`: AMD Ryzen AI 9 465 (20 threads), Linux 6.12 (Debian 13), Python 3.14.8      |
| commit   | `86c3b88b3`, clean tree                                                                       |
| commands | `make characterize` (every subject in `src/doppler`, 245 s), then `make pool-telemetry` (5 s) |

Each subject runs in its own process, so
`make characterize CHARACTERIZATIONS=<its script>` repeats its section alone.
The sweeps are seeded: a rerun at the same commit prints the same numbers,
except `telemetry.py`'s timings.

### `async_dsss_receiver/characterize.py`

2026-10-17 · `swiftgo-ai9465` · `86c3b88b3` · `make characterize`

**Stable at every level.** A receiver designed at Es/N0 = 20 dB, fed 2,430
symbols of a 500 Hz/s Doppler ramp, tracked, stayed finite and decoded with
no bit errors in all 35 trials:

| Es/N0 (dB)                    | seeds  | stable | worst BER | fewest symbols out |
| ----------------------------- | ------ | ------ | --------- | ------------------ |
| 20, 40, 60, 80, 120, 160, 200 | 5 each | 35/35  | 0.0000    | 2,318 of 2,430     |

The sweep reports stability only. The constellation floor its docstring
quotes (about −24 dB EVM) is not something this run prints, so it is not
re-measured here.

### `async_dsss_receiver/telemetry.py`

2026-10-17 · `swiftgo-ai9465` · `86c3b88b3` · `make pool-telemetry`

Its defaults: the Gold-1023 code (`gold:1023:10:934:350:567:73`) at 5 Mcps,
2 samples a chip, 2,700 sym/s, code only, 47 dB-Hz, 1,500 Hz, 1 s, a
four-slot pool, synth seed 7. Both objects took the same 4,887 pushes, one
code epoch (2,046 samples) each:

|          | ns/sample (mean) | median push | p99 push | real time |
| -------- | ---------------- | ----------- | -------- | --------- |
| receiver | 12.4             | 23.6 µs     | 32.8 µs  | 8.08×     |
| pool     | 85.6             | 98.7 µs     | 1,382 µs | 1.17×     |

- **The pool's dwells are 46% of its time.** 305 of its pushes (1 in 16)
    decide a dwell, at 1,295 µs against 101 µs for the rest.
- **An ordinary pool push is about 4× a receiver push.** The pool's searcher
    correlates every epoch across its Doppler tiles (`dp_acq_push`: a
    forward FFT, then a roll and an inverse FFT per tile), where the
    receiver's own search stops at its first hit. This run times the two
    objects, not the searcher alone.
- **The pool found and tracked the one emitter.** Slot 0 seeded at sample
    32,736 (1,527 Hz, 45.5 dB-Hz estimated, against 1,500 Hz and 47 dB-Hz
    set) and was tracking by sample 863,412, at 1,502 Hz. It was the only
    slot that reported. The captures hold 33 probes for the receiver and 105
    for the pool (13, plus 23 for each of four slots).

This is how the cost is distributed over pushes, not a benchmark. The
published cost is `make bench`'s (`bench_async_dsss_pool_core`,
`bench_async_dsss_receiver_core`). The run record, event log, captures and
figures are in `build/pool-telemetry/`.

### `dll_lock/characterize.py`

2026-10-17 · `swiftgo-ai9465` · `86c3b88b3` · `make characterize`

**The symbol-aided look holds code lock at both C/N0s with no drop in 2 s.**
The default 20-partial detector reads "unlocked" 97% of the time at
40 dB-Hz, while the tracked code rate stays within 2 ppm of 1 in every run:

| C/N0 (Es/N0)       | detector              | looks | miss per decision | off   | drops/s | code rate |
| ------------------ | --------------------- | ----- | ----------------- | ----- | ------- | --------- |
| 45 dB-Hz (10.7 dB) | 20 partials (default) | 20    | 5.1%              | 0.8%  | 3.33    | 0.999999  |
|                    | partials, sized       | 25    | 1.7%              | 0.2%  | 0.67    | 0.999999  |
|                    | symbol-aided, sized   | 3     | 0.0%              | 0.0%  | 0.00    | 0.999999  |
| 40 dB-Hz (5.7 dB)  | 20 partials (default) | 20    | 85.8%             | 97.1% | 10.67   | 0.999999  |
|                    | partials, sized       | 161   | 2.2%              | 0.0%  | 0.00    | 0.999999  |
|                    | symbol-aided, sized   | 10    | 1.0%              | 0.0%  | 0.00    | 0.999998  |

Against this script's earlier run in
[async-dsss-receiver-measurements](../../../../../docs/design/async-dsss-receiver-measurements.md)
§12.4 (measured 2026-09-02), the miss rates agree to the precision §12.4
quotes, but one: the symbol-aided look at 40 dB-Hz now misses 1.0% of
decisions, against 0.4% then. That is the 1% that
`det_verify_count(0.01, 1e-6)` takes as its miss rate when it sets the
receiver's three-miss drop. The off fractions and drop rates are at or below
§12.4's throughout.

### `dsss_burst_receiver/characterize.py`

2026-10-17 · `swiftgo-ai9465` · `86c3b88b3` · `make characterize`

Geometry: a 124-sample code period, a 496-sample acquisition frame (all four
of the preamble's periods), a 2,448-sample burst.

**Where the burst lands, at 59 dB-Hz** (near the knee, one burst per offset
across a frame):

| acquisition code | peak/sidelobe | offsets detected | lost offsets                     |
| ---------------- | ------------- | ---------------- | -------------------------------- |
| random           | 2.07          | 37/62 (60%)      | 25, from 0.18 to 0.77 of a frame |
| structured       | 1.07          | 2/62 (3%)        | 60, from 0.02 to 0.98 of a frame |

The code dominates; the random code's residual band is acquisition's
non-overlapping framing, doppler#1006. The script's docstring quotes 65% for
the random code (measured 2026-09-24); this run finds 60%.

**Refine against C/N0** (24 trials a point, the start redrawn across a code
period each trial):

| C/N0 (dB-Hz)             | exact start | frame returned |
| ------------------------ | ----------- | -------------- |
| 100.0 to 63.1 (6 points) | 100%        | 100%           |
| 60.9                     | 96%         | 100%           |
| 59.2                     | 100%        | 100%           |
| 57.7                     | 88%         | 92%            |
| 56.5                     | 62%         | 75%            |

The script prints these as "correct period" and "CRC ok". The first is
`preamble_start == at`, the exact sample the burst began at. The second is
`bool(bits.size)`: a frame came out, which `push()` does for every burst it
demodulates, CRC passed or not. So this run does not say how many CRCs
passed, and the docstring's reading of the gap between the two needs a
column it does not have: doppler#2407.

### `dsss_receiver/characterize.py`

2026-10-17 · `swiftgo-ai9465` · `86c3b88b3` · `make characterize`

**Doppler pull-in ends at 794 Hz** at 65 dB-Hz, inside a native search span
of ±1,466 Hz. Over 300 random trials:

| \|Doppler\| (Hz) | clean        |
| ---------------- | ------------ |
| 0 to 264         | 50/50 (100%) |
| 264 to 528       | 72/72 (100%) |
| 528 to 792       | 46/51 (90%)  |
| 792 to 1,056     | 0/58 (0%)    |
| 1,056 to 1,320   | 0/69 (0%)    |

C/N0 is monotonic at near-zero Doppler (20 Hz): 8/8 clean in each of
\[40, 53), \[53, 67) and \[67, 80) dB-Hz. The script's docstring says 500 to
1,000 Hz "decodes at chance level"; today the 528 to 792 Hz bin is 90% clean,
and the loss starts at the measured boundary.
