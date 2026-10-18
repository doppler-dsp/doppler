# Parallel Asynchronous-Data DSSS Receivers

*`AsyncDsssPool` keeps its name and its Python face; inside, it becomes N
independent `AsyncDsssReceiver`s, each pinned to its own CPU, under a
controller that only watches, tunes and reports. This page is that design.
It replaces the pool [`async-dsss-receiver.md`](async-dsss-receiver.md)
describes in its §5–§9 and §11.1, the central searcher, the assigned table and
the cell receivers behind it. It keeps §11.2's `lost` state but not the
holder's release (here the receiver releases itself, under a controller that
only watches), keeps §11.3, and changes §10's release to symbol lock alone;
what else survives is doppler#2409's. Its measured numbers are [the measurement
record](async-dsss-parallel-measurements.md)'s, rendered from the runs of
shipped tools behind it under the same section numbers; the rest are derived
from §2's parameters. The points still to decide are doppler#2409.*

______________________________________________________________________

## 1. The signals

M out of N ($\leq 10$) possible signals are simultaneously on air with at
minimum differing code phase and carrier phase trajectory. Each signal may go
off air and be replaced by a new signal at any moment. All signals share a
common, periodic, simultaneous multi-epoch data-free time block lasting many
code epochs.

The carrier frequency, while bounded in range by $\approx T^{-1}_E$ (one
inverse code epoch), may take on any value within said range.

$$\left|f_m\right| \leq \Delta f_c \approx R_s \approx 1/T_E$$

In numbers, at the parameters of §2:

| quantity                                                             | value                                       |
| -------------------------------------------------------------------- | ------------------------------------------- |
| one code epoch $T_E$ = 1,023 chips at 3.069 Mcps                     | 333.3 µs, 2,046 samples at 2 samples a chip |
| $1/T_E$: the carrier bound $\Delta f_c$, and one search tile's width | 3,000 Hz                                    |
| $E_s/N_0 \geq 5$ dB at 2,700 sym/s                                   | C/N0 ≥ 39.3 dB-Hz, 4.5 dB per epoch         |
| the data-free block, by default (configurable)                       | 500 all-zero symbols: 185 ms, 555.6 epochs  |

**A signal changes only after a data-free block.** One goes off air, or a new
one comes on, at a frame boundary, never inside a frame. The 500-symbol block
is this design's; SPEC's code-only window is 450 symbols (167 ms), which the
pool as built is sized at, and either holds the 94-epoch depth the 500 Hz/s
Doppler rate allows at 3.069 Mcps.

## 2. Receiver operation

- The initial application is for continuously emitted CCSDS command link-like
    signals
    - $R_c=3.069$ Mcps
    - $N_E = R_c \times T_E = 1023$ chips per epoch
    - $E_s/N_0 \geq 5$ dB
- A slot is defined by a carrier offset $f_m$ and code phase with SNR the
    natural tie-breaker should signals be otherwise indistinguishable.
- The "bookkeeper" tracks status of all receivers which are all always online
    either searching or tracking. Search resumes on signal loss or upon command
    tuned to a particular offset specified by bookkeeper, no frequency
    acquisition search is performed by the receivers except on command on the
    slow-time axis in the data-free region
- Each receiver makes its current code phase and carrier estimate available
    for query
- Receivers are fed from a common DDC that removes any common carrier offset
    and resamples to twice the chip rate.

```mermaid
flowchart LR
    DDC["common DDC<br/>removes the common carrier offset<br/>2 samples a chip"] --> B["one block a push<br/>the same samples to every receiver"]
    subgraph POOL["AsyncDsssPool: N receivers, one pinned CPU each"]
        B --> R1["receiver 1<br/>searching or tracking"]
        B --> R2["receiver 2"]
        B --> RN["receiver N"]
        R1 -->|"status: state, code phase, carrier"| C["controller<br/>(the bookkeeper)"]
        R2 --> C
        RN --> C
        C -->|"offset, command, exclusions, rank"| R1
        C --> R2
        C --> RN
    end
    C --> OUT["reports"]
```

## 3. The receiver

One `AsyncDsssReceiver` per CPU, run by its own state machine. **Without
symbol lock it searches; with it, it tracks.** Every receiver is always
online, so one that is not tracking is already searching: there is no
separate bench of spares.

- **Searching** is code phase only, at the offset the controller tuned it
    to: one tile, $\pm 1/(2 T_E)$ about that offset.
- **On command, inside the data-free block,** the search adds the slow-time
    axis: a coherent block of D epochs resolves the carrier within the tile to
    $1/(D T_E)$. It is the only frequency search a receiver performs.
- **Refining, then tracking,** as the receiver does today: the carrier
    estimate refined, then the Costas, `Dll`, `RateConverter`, `MpskReceiver`
    chain live.
- **Tracking ends when symbol lock is lost,** and the receiver goes back to
    searching at its offset. A signal changes only after a data-free block
    (§1), so a loss there is normally its signal leaving, and symbol lock
    lets go of a signal gone for good well inside one data-free block.
    Inside a frame it is less forgiving: under this rule a fade it does not
    ride out costs a re-search, and at the 5 dB floor even a short one
    often does.

Symbol lock through a fade, of the draws that held lock before it; the
draws that had not are excluded and counted (§3.1):

<!-- async-dsss-parallel:design-fade:start -->

| Es/N0 | draws not settled, excluded | rides out 5 ms       | rides out 10 ms      | rides out 20 ms     | lets go after a loss, the slowest |
| ----- | --------------------------- | -------------------- | -------------------- | ------------------- | --------------------------------- |
| 10 dB | 1 of 21                     | 20 of 20 (0.84–1.00) | 16 of 20 (0.58–0.92) | 7 of 20 (0.18–0.57) | 33.9 ms                           |
| 5 dB  | 8 of 28                     | 20 of 20 (0.84–1.00) | 14 of 20 (0.48–0.85) | 1 of 20 (0.01–0.24) | 27.9 ms                           |

<!-- async-dsss-parallel:design-fade:end -->

What today's receiver must gain for this:

1. **Re-acquiring on loss of symbol lock.** The searching flavor can
    already reach `lost`: `lost_confirm_s`, a create parameter, releases
    tracking once BOTH lock flags have been down that long, and `reset()`
    returns it to searching
    (`native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`:
    `dp_async_dsss_receiver_create()`'s `lost_confirm_s`, and
    `dp_async_dsss_receiver_reset()`).
    By default it is 0, so it never does. What it lacks is the step from
    `lost` back to searching on its own, and this design's trigger: symbol
    lock alone, where today's release needs code lock down too.
1. **A tuned offset**: a search centred where the controller says, not at 0.
1. **The slow-time search on command,** at a depth the data-free block and
    the Doppler rate allow.
1. **Exclusions**: code phases the search must not take, because another
    receiver already tracks them. The pool's zone is keyed on code phase alone
    (within a chip, at any Doppler); keying on carrier seeded duplicate receivers
    (`async-dsss-receiver-measurements.md` §12.14). A receiver's own search
    takes its strongest hit, and on a shared input the strongest hits are
    the signals already owned. Refusing the hit afterwards is not enough: a
    reset receiver finds the same peak again.
1. **A rank** when more than one receiver searches the same offset: rank r
    takes the r-th strongest peak that is not excluded, so k new signals
    arriving together lock on k receivers in one dwell. Ranks line up only
    if those receivers' dwells start on the same epoch of the shared sample
    clock. The engine already reports a peak list (`set_max_peaks`; the
    pool asks for 16), but the receiver's own search asks it for one hit and
    exposes no way to ask for more.

## 4. The controller

The bookkeeper of §2. It searches nothing, seeds nothing and holds no
surface. It:

- **reads each receiver's `status()`** (state, chip phase, carrier, code and
    symbol lock) and keeps the table of who tracks what, never the demodulator's
    timing flag, which outlives the signal (record §3.2);
- **tunes each searching receiver to an offset,** and commands the slow-time
    search in the data-free block;
- **hands every searching receiver the exclusions** (the signals the others
    track) and its rank among the receivers sharing its offset;
- **resolves duplicates**: two receivers holding the same (offset, code
    phase), the first to lock keeps it and the other adds it to its
    exclusions; between signals otherwise indistinguishable, SNR decides;
- **reports**.

## 5. Arrival to tracking

A new signal is found by the receivers already searching, without waiting
for anyone to be freed. With the receiver's own search over $\pm 1/T_E$, data
on, and a signal arriving at any time, code phase and carrier in range
(§5.1):

<!-- async-dsss-parallel:design-arrival:start -->

| Es/N0 | tracking (the state), median | symbol lock, median of those that lock | locked 2 s after arrival |
| ----- | ---------------------------- | -------------------------------------- | ------------------------ |
| 10 dB | 45 ms (slowest 47)           | 86 ms (slowest 698)                    | 20 of 20 (0.84–1.00)     |
| 5 dB  | 86 ms (slowest 101)          | 375 ms (slowest 1734)                  | 15 of 20 (0.53–0.89)     |

<!-- async-dsss-parallel:design-arrival:end -->

At 10 dB the median arrival locks inside one default data-free block
(185 ms), though not every draw does. At the 5 dB floor the search is not
what is slow: the state reads tracking on every draw, and it is symbol lock
that lags, on some draws for seconds and on some not within 2 s. What
decides it is the carrier the refine stage hands the carrier loop, by how
far it lands from the truth (§5.1):

<!-- async-dsss-parallel:design-seed:start -->

| refine seed error | locked, 10 dB                      | locked, 5 dB                       |
| ----------------- | ---------------------------------- | ---------------------------------- |
| 0–100 Hz          | 15 of 15 (0.80–1.00), median 67 ms | 6 of 6 (0.61–1.00), median 151 ms  |
| 100–200 Hz        | 3 of 3 (0.44–1.00), median 143 ms  | 5 of 5 (0.57–1.00), median 419 ms  |
| 200–300 Hz        | 2 of 2 (0.34–1.00), median 599 ms  | 4 of 7 (0.25–0.84), median 1479 ms |
| ≥ 300 Hz          | —                                  | 0 of 2 (0.00–0.66)                 |

<!-- async-dsss-parallel:design-seed:end -->

The loop's acquisition bound is `bn / m`, 60 Hz at this front end, and it
acquires every draw inside it, reliably to twice it and not at all by four
times (`costas_pullin.c`, `async_dsss_receiver_core.h`). The whole receiver
is more forgiving than the loop alone: slower to lock as the seed error
grows, it still locked draws in the 200–300 Hz row, past four times the
bound; the record's seed-edge block gives the largest locked error and the 5 dB
draws at 250 Hz or more. Why it reaches past the loop's edge is open (doppler#2409): the
error here is read when tracking starts, which need not be the residual
the loop finally pulls in. That residual is doppler#982's. `rx.car.nco` (#2412) now shows what the loop
pulls in; read as a residual, it is not yet measured (doppler#2409). Since this design
defines tracking as symbol lock, a receiver at the floor that misses it
would let go of the signal it holds and search again, so the refine's
residual is a design input (doppler#2409).

## 6. What it costs

Every state fits one CPU (§6: the shipped C benches, the mean per sample at
6.138 MSa/s):

<!-- async-dsss-parallel:design-cost:start -->

| receiver state                              | × real time, one thread |
| ------------------------------------------- | ----------------------- |
| tracking                                    | 0.06                    |
| searching one tile, one-epoch looks         | 0.11                    |
| searching ±1/T_E (3 tiles), one-epoch looks | 0.19                    |
| searching ±1/T_E, slow time over D = 154    | 0.53                    |

<!-- async-dsss-parallel:design-cost:end -->

The slow-time row is deeper than the 94 epochs this design's Doppler rate
allows, so it bounds that search. Even it leaves the CPU spare, so the
controller's offsets are for spreading the searching receivers across the
range, not for fitting one into its CPU.

## 7. Why the pool as built is replaced

Its push is one receiver's plus a central searcher's that never stops,
because a new emitter may appear at any time. At the design point it was
sized for, ±50 kHz at the Doppler-rate depth, that searcher alone runs
several times slower than real time on one thread, and threads bring it
down only in wall time, the CPUs they spend being more (§7):

<!-- async-dsss-parallel:design-pool:start -->

| per push                                   | timed at | × real time (wall) |
| ------------------------------------------ | -------- | ------------------ |
| one receiver, tracking                     | 10 MSa/s | 0.10               |
| the pool, 12 slots, one emitter tracked    | 10 MSa/s | 1.43               |
| the same, across the machine's cores       | 10 MSa/s | 1.24               |
| its searcher at ±50 kHz, 21 tiles, D = 154 | 10 MSa/s | 5.28               |
| the same, eight threads                    | 10 MSa/s | 1.46               |
| its searcher at ±50 kHz, 53 tiles, D = 61  | 4 MSa/s  | 8.05               |
| the same, four threads                     | 4 MSa/s  | 2.36               |

<!-- async-dsss-parallel:design-pool:end -->

Parallel receivers spend nothing like that: the search is each idle
receiver's own.

## Related

- [`async-dsss-receiver.md`](async-dsss-receiver.md): the receiver, and the
    pool this replaces (§5–§9 and §11.1)
- [the measurement record](async-dsss-parallel-measurements.md)
- [Tracking a Population of DSSS Emitters](../guide/async-dsss-pool.md): the
    pool as built, from the user's side
