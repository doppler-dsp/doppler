# Parallel Asynchronous-Data DSSS Receivers

*`AsyncDsssPool` keeps its name and its Python face; inside, it becomes N
independent `AsyncDsssReceiver`s, each pinned to its own CPU, under a
controller that only watches, tunes and reports. This page is that design.
It replaces the pool [`async-dsss-receiver.md`](async-dsss-receiver.md)
describes in its §5–§11, the central searcher, the assigned table and the
cell receivers behind it. Every number here is measured, and the runs behind
them are [the measurement record](async-dsss-parallel-measurements.md),
under the same section numbers. The points still to decide are doppler#2409.*

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
| the data-free block, by default (configurable)                       | 500 all-zero symbols: 185 ms, 556 epochs    |

**A signal changes only after a data-free block.** One goes off air, or a new
one comes on, at a frame boundary, never inside a frame.

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
    (§1), so a loss there is normally its signal leaving. The lock detector
    is already hysteretic (§3.1): it rides out a dropout of up to 20 ms at
    10 dB and 10 ms at 5 dB, and lets go 20–24 ms after a signal leaves for
    good, well inside one data-free block.

What today's receiver must gain for this:

1. **Re-acquiring on loss.** Today the searching flavor never leaves
    tracking: its `lost_confirm_s` defaults to 0, so it never enters `lost`
    and coasts through anything
    (`native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`).
1. **A tuned offset**: a search centred where the controller says, not at 0.
1. **The slow-time search on command,** at a depth the data-free block and
    the Doppler rate allow.
1. **Exclusions**: code phases (and carriers) the search must not take,
    because another receiver already tracks them. A receiver's own search
    takes its strongest hit, and on a shared input the strongest hits are
    the signals already owned. Refusing the hit afterwards is not enough: a
    reset receiver finds the same peak again.
1. **A rank** when more than one receiver searches the same offset: rank r
    takes the r-th strongest peak that is not excluded, so k new signals
    arriving together lock on k receivers in one dwell. Ranks line up only
    if those receivers' dwells start on the same epoch of the shared sample
    clock. The engine already reports a peak list (`set_max_peaks`; the
    pool asks for 16).

## 4. The controller

The bookkeeper of §2. It searches nothing, seeds nothing and holds no
surface. It:

- **reads every receiver's status** (state, code phase, carrier estimate,
    lock flags) and keeps the table of who tracks what;
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
for anyone to be freed. The receiver's own search over $\pm 1/T_E$, data on
(§5.1):

| Es/N0 | searching | refining   | arrival to tracking |
| ----- | --------- | ---------- | ------------------- |
| 10 dB | 9 pushes  | 127 pushes | 45 ms               |
| 5 dB  | 45 pushes | 218 pushes | 88 ms               |

Both are inside one default data-free block (185 ms).

## 6. What it costs

Every state fits one CPU, at every depth (§6.1, §6.2: the share of one CPU
is the push time over the 333.3 µs of signal it covers):

| receiver                                                           | one tile  | the whole range, $\pm 1/T_E$ |
| ------------------------------------------------------------------ | --------- | ---------------------------- |
| tracking                                                           | 0.07–0.08 | 0.07–0.08                    |
| searching, its own one-epoch looks                                 | 0.17      | 0.34                         |
| searching, slow-time over D = 16                                   | 0.24      | 0.39                         |
| searching, slow-time over D = 94 (the 500 Hz/s Doppler-rate bound) | 0.33      | 0.64                         |

Even a search of the whole range at the rate-bound depth leaves a third of
its CPU free, so the controller's offsets are for spreading the searching
receivers across the range, not for fitting one into its CPU. Tracking cost
0.07–0.08 with data on at 10 and 5 dB, and 0.07 code only at 10 dB.

## 7. Why the pool as built is replaced

Its push is one receiver's plus a central searcher's that never stops,
because a new emitter may appear at any time (§7.1):

| per push                      | share of one CPU |
| ----------------------------- | ---------------- |
| one receiver                  | 0.09             |
| the pool, one emitter tracked | 0.64             |
| the pool's searcher alone     | 0.56             |

At the design point the pool was sized for (±50 kHz in 35 tiles, the
rate-bound depth of 94 epochs) that searcher is 7.4 CPUs serially, 2.2 on
four threads and 1.5 on eight (§7.2). On eight threads the dwell that
decides each 94-epoch block alone takes 33 ms, longer than the block's
31.3 ms of signal. Parallel receivers spend nothing like that: the search is
each idle receiver's own.

## Related

- [`async-dsss-receiver.md`](async-dsss-receiver.md): the receiver, and the
    pool this replaces (§5–§11)
- [the measurement record](async-dsss-parallel-measurements.md)
- [Tracking a Population of DSSS Emitters](../guide/async-dsss-pool.md): the
    pool as built, from the user's side
