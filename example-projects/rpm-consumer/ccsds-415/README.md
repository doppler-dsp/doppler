# ccsds-415 — the 415.1-B-1 receiver from C++, against the RPM

<!-- --8<-- [start:body] -->

The async DSSS receiver at the CCSDS 415.1-B-1 specification point, driven
from a C++17 program that links doppler statically from the installed
`-devel` RPM. It streams the point's scene through the receiver one chunk at
a time, times every chunk, captures every telemetry probe the receiver has to
a file, and checks itself. `plot.py` draws the capture and does nothing else.

It runs at both ends of the point's band, 2.2 and 2.8 GHz, and prints each
run's processing time per sample.

## Build and run it

On Fedora or EL8 (AlmaLinux, Rocky), install the two packages from the
release and a C++ toolchain:

<!-- docs-snippet: no-exec=installs packages; package-linux-smoke builds and runs this project against the RPM on Fedora and EL8 -->

```sh
sudo dnf install ./libdoppler-dsp-*.rpm ./libdoppler-dsp-devel-*.rpm \
    gcc-c++ cmake make
make run
```

The RPM is the install, so CMake finds it with no path. Any other doppler
install works too: `make PREFIX=<prefix> run` for a `cmake --install` tree
or an extracted release tarball. CI builds it both ways: against the RPM on
Fedora and EL8 (`make package-linux-smoke`), and against a scratch install
on Ubuntu (`make rpm-consumer-check`).

Then plot what it captured, with numpy and matplotlib (no doppler wheel):

<!-- docs-snippet: no-exec=needs the captures make run writes -->

```sh
make plot                    # shows a figure per carrier
make plot PLOT_ARGS=--save   # writes build/out/<capture>.png instead
```

`make help` lists the knobs. The program's own options go through `ARGS`,
for example `make run ARGS='--carrier lo --esn0 6'`; `--help` lists them.

## What it runs

The point's scene, built from installed objects. Its rates and limits are
the ones `doppler/ccsds/ccsds_415.h` holds; the code's parameters are
415.0-G-1's, named where `run.cpp` passes them (the C face has no names for
them yet, #3348):

- a 1,023-chip Gold code spreading a PN15 payload at 3,000 symbols a second,
    each 1,000-symbol frame opening with 275 code-only symbols (the shipped
    `wfm_synth` continuous DSSS);
- the chip rate the carrier sets, resampled to the 11 MSa/s capture
    (`RateConverter`, at the ratio `dp_ccsds_415_scene()` computes);
- a Doppler ramping at R_env, 11 Hz/s, through zero at mid-run
    (`doppler_channel`);
- noise at Es/N0 10 dB, 6 dB above the point's Es/N0 4 dB (`awgn`).

The receiver is the one the library builds at the point,
`dp_async_dsss_receiver_create_ccsds415()` (Python: `Ccsds415Receiver`),
designed for Es/N0 4 dB, so it is built once for its weakest signal.

The scene's payload is continuous data, not transfer frames. The TC frame
stimulus (232.0-B-4 frames as a frame description) and reading their header
fields back are the next step; until then nothing here decodes a frame.

## What it checks

`make run` exits non-zero unless, at each carrier:

1. the symbol lock is sustained within the point's lock allocation, 20 s
    from arrival (`DP_CCSDS_415_LOCK_S`);
1. the BER over the locked stream is at most 1e-3, scored against the
    payload that was sent (`dp_ber_lock_symbol()`, `dp_ber_meter`);
1. no call filled its output capacity, so no symbol was dropped;
1. the capture is complete (`dp_tlm_capture_close()` returns `DP_OK`);
1. every probe the receiver registers emitted at least once.

## Chunks, and why at least 25 samples

A call to `dp_async_dsss_receiver_steps()` writes its symbols into a buffer
the caller sizes. This program sizes it to the chunk, which holds whenever
one call cannot produce as many symbols as it was handed samples.

That count is bounded by the chain's fixed rates, never by the timing loop
(doppler#2469). The demodulator emits at most one symbol per input. Its
inputs are a fixed-rate converter's outputs from the code loop's partial
correlations. A call runs at most `x_len + H` samples: its own, plus the
carried code period or a hand-over's history, whichever is longest. So a
call's symbols are at most

```text
ceil(ratio x segments x (floor((x_len + H) / tsamps) + 1)) + 1
```

At the point, with the receiver's own configuration:

| carrier | tsamps | H     | segments | ratio  | bound at 4,096 | smallest chunk |
| ------- | ------ | ----- | -------- | ------ | -------------- | -------------- |
| 2.2 GHz | 3,500  | 8,268 | 4        | 1.9094 | 32             | 25             |
| 2.8 GHz | 2,750  | 6,693 | 4        | 1.5003 | 26             | 21             |

The values are the receiver's own state at each carrier (`tsamps`;
`H = max(tsamps - 1, refine_hist_cap, srch_hist_cap)`; `segments`; the
converter's `sps x symbol_rate / (segments x chip_rate / code_len)`), read
from `dp_async_dsss_receiver_create_ccsds415()`'s result (measured
2026-11-24).

So capacity = chunk holds from 25 samples, and `--chunk` refuses less. The
default is 4,096. The check that no call filled its capacity stays on at run
time.

## What it measures

Each chunk's processing time per sample goes into the capture as the probe
`app.proc_ns`, beside the receiver's own. The summary states the run's
processing time per sample against the sample period, 90.9 ns at 11 MSa/s,
and their ratio both as what it is and as what it means. The slowest chunk
is the one that searches or hands over.

An example, not a claim: on 2026-11-24, on an AMD Ryzen AI 7 445 (one
thread, a Release build of this tree linked against the build's
`libdoppler.a`), it printed

```text
ccsds415-2.2GHz: locked 0.26 s after arrival (allocation 20 s); BER 0.0e+00 over 6463 bits
  processing time per sample 5.5 ns, sample period 90.9 ns: 6.0% of the sample period (16.5 times faster than real time); slowest chunk 3336.8 ns a sample; 6848 chunks of 4096
ccsds415-2.8GHz: locked 0.30 s after arrival (allocation 20 s); BER 0.0e+00 over 6347 bits
  processing time per sample 5.6 ns, sample period 90.9 ns: 6.2% of the sample period (16.2 times faster than real time); slowest chunk 2386.5 ns a sample; 6848 chunks of 4096
OK
```

## The capture

One file per carrier, `ccsds415-<carrier>.tlm`, written by doppler's
lossless capture (`doppler/dp_tlm_capture/dp_tlm_capture_core.h`), with a
`-meta` JSON sidecar. The records are 16 bytes each. The sidecar carries
their layout (`dtype`), the probe table and the sample clock, so `plot.py`
reads the format from the file rather than restating it.

## The TUI

`ccsds415-tui` is a second front end over the same run (`run.cpp`), not a
second receiver: the scene, the receiver, the capture and the checks are
the batch binary's. It draws, at most every 100 ms of wall time:

- **cost**: the last chunk's processing time per sample and the run's,
    the sample period, and their ratio as a share of the sample period and
    as times faster than real time; and, apart, the display's own time per
    refresh, so it is plain that drawing is not what is being measured;
- **receiver**: its state, both lock flags, the symbol-lock metric against
    its threshold, C/N0, the Doppler and the code rate (received over
    nominal: 1 is no Doppler), as `dp_async_dsss_receiver_status()`
    reports them;
- **scene**: the carrier, the time, the symbols out, and progress.

It runs on one thread. The timed span is the receiver call alone; the
drawing and the pacing fall outside it. By default each chunk is held to
real time, so its 30 s scene takes 30 s and lock can be watched happening,
then the Doppler ramping through zero; `--unpaced` runs it as fast as it
goes. Paced, the processing time per sample reads higher than unpaced (7.6
against 5.6 ns in one run on the box above): the receiver starts each chunk
with a cache the sleep has let go cold, so the panel labels it "(paced)".

When every scene has ended, the last frame stays on screen with a line
saying so, until `q`.

`q` quits early, through the same teardown: the display closes, then the
capture. Stopping is the user's choice, not the receiver's failure, so a
stopped run answers only what a partial run can: the capture is complete
and holds every record emitted so far, and no call filled its capacity. It
exits 0 when those hold, and says in one line when it stopped and which
full-scene checks (the lock time, if not yet reached; the BER; the payload
marker) it did not evaluate. `--snapshot` and `ccsds415` keep the full
checks.

<!-- docs-snippet: no-exec=needs the RPM and FTXUI; package-linux-smoke runs the TUI's snapshot on Fedora -->

```sh
sudo dnf install ftxui-devel   # Fedora; EL8 needs EPEL
make tui
```

It is built against FTXUI 5 or later and written to what 5.0.0 through
7.0.3 share. `--snapshot` needs no terminal: it runs the scene unpaced and
prints the last frame as text. CI runs it on Fedora, against the
distribution's FTXUI, and requires every panel row to carry a value of its
kind (`tests/install/tui-snapshot-check.sh`).

Where no package provides FTXUI (Ubuntu), fetch a pinned, hashed
release: `make tui CMAKE_ARGS=-DCCSDS415_FETCH_FTXUI=ON`. That path builds
on Ubuntu with FTXUI 7.0.3 and 5.0.0, but no CI job runs it; neither does
one run EL8 with EPEL's FTXUI.

## Compilers

The example is C++17 over doppler's C headers, and passes doppler's
complex samples as C99 `float _Complex`, which GCC and Clang accept in C++
as an extension and MSVC does not. So it builds with:

- **GCC**: 8.5 (EL8, the RPM's toolchain), 13.3 (Ubuntu) and Fedora's,
    each in CI;
- **Clang**: 21.1.8, by hand, not in CI (#3347);
- **not MSVC**, so not the Windows build doppler itself has.

## Not here yet

- **The TC frame stimulus**, and the frame fields read back from it, in
    both binaries.

<!-- --8<-- [end:body] -->
