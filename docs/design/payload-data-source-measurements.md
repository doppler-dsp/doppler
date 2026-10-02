# The Payload as a Data Source — the measurement record

The dated record behind [the design page](payload-data-source.md), under
the same section numbers. The design page states what is; this page
records what was measured to get there, in order.

______________________________________________________________________

## 4. The rules the shape leaves open

### 4.0 The chunker, prototyped before any C (2026-09-30)

A throwaway Python chunker (about 180 lines, kept in a scratch directory
and not committed, per
[the lifecycle](../dev/contributing/adding-algorithms.md#start-with-why))
implemented §F.5's four rules over a finite source and a non-blocking
file descriptor. It asserted seven cases:

| case                                           | result                                                                                                     |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| 96 bits, `LEN` 32                              | 3 data frames, bits identical                                                                              |
| 90 bits, `LEN` 32, no fill                     | refused: "26 bits into a 32-bit frame; 6 bits short"                                                       |
| 90 bits, `LEN` 32, fill `01`                   | 3 frames; the last carries 6 fill bits, `010101`                                                           |
| 0 bits                                         | 0 frames (which led to §4.3: refuse it)                                                                    |
| a pipe of 24 bits, `LEN` 10                    | data, data, then the end padded with 6 bits; octets straddle frames and the bits come back in order (§4.2) |
| a paced pipe that pauses for 2.4 frame periods | `data, idle, idle, data` (§4.1)                                                                            |
| a paced pipe that underruns, no fill           | refused, but only at the underrun (which led to §4.4)                                                      |

What it changed:

- **§4.1:** the prototype held a partial residue through the idle frames
    and gave it to the next data frame. Writing that down made it a rule,
    rather than an accident of the loop.
- **§4.4:** the refusal for a stream without a fill arrived mid-run in
    the prototype, after frames had been emitted. That is why the design
    requires the fill up front.
- **§4.5:** the paced case's output depends on scheduling. The prototype
    had to sleep to make it happen, so the C tests must inject the
    underrun instead.

______________________________________________________________________

## 6. Unknowns — the two throughput ones, measured (2026-10-02)

**Headline:** assembly does not need to run ahead of the pacer. A CADU
takes 20–25 µs to assemble, about 11 % of the 190–204 µs that wfmgen
spends on the whole frame at one sample per bit. The paced stdin has no
frame-period threshold of its own. A pipe read costs about 1 µs, so what
limits a paced run is the per-sample work, and a producer that paces
itself to the frame rate has to stay up to one 4096-sample block ahead.
The third unknown, when the SigMF metadata is written, was not measured.

**Machine:** AMD Ryzen AI 7 445 w/ Radeon 840M (12 logical CPUs), under
**WSL2** (kernel 6.18.35.2-microsoft-standard-WSL2), gcc 15.2.0. The
build is `make build` (Release), and `compile_commands.json` gives
`-O3 -DNDEBUG -march=x86-64-v2 -ffast-math -fno-finite-math-only`.
Base commit `1c918681`.

### 6.1 The cost of assembling every frame

The rows are in `native/benchmarks/bench_frame_core.c`. `frame` is a jm
component, so `make bench` runs them. A round is 32 frames, there are
200 rounds, and each row reports the minimum. The payload is
`LEN = 1784` bits, which is a CADU's 223 octets, and it goes into two
frames:

- **plain:** `asm | data:1784 | crc16`, 1832 bits;
- **cadu:** `asm | data:1784 | rs_parity` with RS(255,223) at I=1, the
    10.4.1 randomiser, and conv r1/2 over the whole frame, 4144 bits.
    This is the guide's `cadu.json` with its payload made `data:1784`.

<!-- docs-snippet: no-exec=a benchmark run is a deliberate measurement, not a per-push check -->

```sh
make bench BENCH_CMD="uv run just-makeit bench --c-only frame"
```

Five runs (four of the binary run directly, one through `make bench`).
Each cell is the range of the per-run minimums:

| row              | what one frame is                                           | µs/frame    | frames/s      |
| ---------------- | ----------------------------------------------------------- | ----------- | ------------- |
| `data pn:0`      | `dp_wfm_data_frame`, an endless `pn:0:15` stream            | 2.40 – 2.76 | 362 k – 417 k |
| `data pipe`      | the same from a pipe already holding it: `poll(0)` + `read` | 0.91 – 1.09 | 914 k – 1.1 M |
| `assemble plain` | `dp_wfm_frame_assemble_data`, plain                         | 1.87 – 2.28 | 438 k – 534 k |
| `assemble cadu`  | the same, cadu: RS + randomise + conv                       | 19.4 – 24.2 | 41 k – 51 k   |
| `pipe+cadu`      | pull from the pipe, then assemble the cadu                  | 20.1 – 25.0 | 40 k – 50 k   |

That is the frame path alone. The **whole** wfmgen pipeline takes a CADU
from a pipe that is never empty, then pulls, assembles, modulates and
writes cf32 to a file. Five runs of 20000 frames, unpaced, give:

```text
cat in.bin | wfmgen --type bits --modulation bpsk --frame cadu-data.json \
               --data-from-file - --fill 0 --sps 1 -o out.cf32
```

| µs/frame      | frames/s      | samples/s         |
| ------------- | ------------- | ----------------- |
| 189.7 – 204.2 | 4 897 – 5 271 | 20.3 – 21.8 MSa/s |

So the frame accounts for 20–25 µs of the ~197 µs, and the remaining
~42 ns per sample is the synth and the writer. At more samples per bit
the frame's share only gets smaller.

### 6.2 The shortest frame period a paced stdin keeps up with

**Derivation.** The paced loop is `drain_to_writer` in `wfmgen.c`. It
computes a block of `BLK = 4096` samples, writes it, and sleeps until
that block is due (`dp_sample_clock_pace`). A frame is pulled when the
synth reaches the frame's first bit. So every frame that starts in a
block is pulled at the moment the block is computed, which is up to
4096 samples **before** the frame is due. Each pull is one `poll(0)` and
one `read`, about 1 µs (§6.1). That leaves two conditions, and the frame
period appears in only one of them:

1. **Throughput:** `c_frame + N × c_sample ≤ T_frame`. Here `c_frame` is
    the pull plus the assembly, 20–25 µs for a CADU, and `c_sample` is
    about 42 ns. A CADU at one sample per bit therefore needs a frame
    period of at least about 200 µs, which is about 21 MSa/s. Of that,
    assembly is about 11 %.
1. **Lead:** a frame's data must be in the pipe when its block is
    computed, up to `4096 / fs` seconds early. A producer that is ahead
    of the run, as `cat` always is, never trips this. A producer that
    paces itself to the frame rate gets spurious idle frames until it is
    one block ahead, and this holds at **any** frame period. It limits
    itself: each idle frame moves the data one frame later, so the lead
    grows by one frame period until it covers the block.

**Throughput, empirically.** The same CADU ran under `--realtime` for
3 s at each `--fs`, from a pipe that is never empty. A run that keeps up
takes 3.00 s of wall time:

| `--fs` | frame period | wall (s)    | idle frames |
| ------ | ------------ | ----------- | ----------- |
| 1 MHz  | 4144 µs      | 3.01        | 0           |
| 10 MHz | 414 µs       | 3.01 – 3.07 | 0           |
| 20 MHz | 207 µs       | 3.01 – 3.03 | 0           |
| 22 MHz | 188 µs       | 3.09        | 0           |
| 25 MHz | 166 µs       | 3.18 – 3.57 | 0           |

The run keeps up to 20 MHz (a 207 µs CADU) and falls behind from 22 MHz,
as condition 1 predicts. The rows combine runs on the base-commit build
with runs on the probe build described below. With a pipe that is never
empty, the two builds give the same result. Every rate, 1 MHz included,
reports clock underruns (`--realtime` without `--realtime-resync` counts
every block until the run has caught up). The same runs reading a
**regular file** instead of the pipe report as many: 24–59 at 1 MHz,
worst 37–78 ms. So the underruns come from the paced loop on this WSL2
box and not from the pipe pull. Their cause is not diagnosed here.

**Lead, empirically.** This needed a fix that is not on main.
`wfmgen --realtime` never sends an idle frame today, because the pacing
is set after the composer has already built its synths
([#1782](https://github.com/doppler-dsp/doppler/issues/1782)). On main,
a 0.5 s pause in stdin gives 0 idle frames and
`75 underrun(s) — worst 518.967 ms behind real time`. With the setter
also applied to the live synths (a three-line scratch probe, not
committed), the same pause gives 176 idle frames (0.535 s / 3.04 ms) and
no underrun. The rows below come from the probe build. A Python producer
writes `LEAD` chunks at once, then one chunk per frame period on an
absolute schedule. The frame is
`--sync 0x1ACFFC1D --crc crc16 --data-len 256 --fill 0 --sps 1`, so
N = 304 samples. The idle frames are counted from the capture: an idle
payload is all fill, so all of its samples have one sign. Only idle
frames *after* the first data frame are counted.

| `--fs`  | frame period | block    | spurious idle frames by producer lead (frames)  |
| ------- | ------------ | -------- | ----------------------------------------------- |
| 100 kHz | 3.04 ms      | 40.96 ms | 0: 3, 7, 24 · 4: 7 · 8: 0 · 16: 0, 0 · 32: 0, 0 |
| 1 MHz   | 304 µs       | 4.10 ms  | 0: 29 · 8: 12 · 16: 7 · 32: 49 · 64: 0          |

At 100 kHz, a lead of 8 frames (24 ms) or more gave none. With no lead,
the idle frames arrive in the first block, and each one adds to the
lead. One no-lead run also had a burst of 17 at frame 131, which was a
producer stall. At 1 MHz the idle frames come in bursts at random
positions, and those are stalls of 1–15 ms in the Python producer on
WSL2, which outlast a smaller lead. So the frame period sets nothing
here. What a flowing input needs is to stay ahead of the run by one
block plus its own jitter.

### 6.3 The consequence §6 asks for

- **Assembly does not run ahead of the pacer.** At one sample per bit it
    is about 11 % of a CADU's cost, the per-sample path is the bound, and
    the 4096-sample block already separates a frame's compute from its
    deadline. Moving assembly to another thread would raise the ceiling
    by at most that 11 %. No issue is filed for it.
- **The lead belongs in the guide** (§7 step 9): a producer that paces
    itself to the frame rate under `--realtime` must stay up to one
    4096-sample block ahead, or it gets idle frames at the start.
- **Filed:** [#1782](https://github.com/doppler-dsp/doppler/issues/1782),
    `--realtime` never sends idle frames. §4.5's behaviour is not reached
    from wfmgen until it is fixed.
