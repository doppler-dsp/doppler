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
for example `make run ARGS='--carrier hi --esn0 6'`; `--help` lists them.
At 2.2 GHz below Es/N0 9 dB, some arrivals give an error rate near 1/3
for seconds while the receiver reports lock (#3349): this scene fails at
4, 6, 7 and 8 dB and passes at 5, 9 and 10. Scored from all five lock
flags, which the certification reads through a test header and `status()`
does not publish (#3350), some of those runs pass
([#3349, the five-flag table](https://github.com/doppler-dsp/doppler/issues/3349#issuecomment-7146889068))
and one tested does not
([#3349, the counterexample](https://github.com/doppler-dsp/doppler/issues/3349#issuecomment-7146958718)).
So no command here runs `--carrier lo` below 10 dB. `--errors` prints
where the bit errors fall in the scored stream: the first and last, the
longest run, and a count for each tenth of it.

## What it runs

The point's scene, built from installed objects. Its rates and limits are
the ones `doppler/ccsds/ccsds_415.h` holds; the code's parameters are
415.0-G-1's, named where `run.cpp` passes them (the C face has no names for
them yet, #3348):

- a 1,023-chip Gold code spreading the point's stimulus at 3,000 symbols a
    second (the shipped `wfm_synth` continuous DSSS). The stimulus is the
    design page's §1.4 period, repeated: a 275-symbol gap of the code
    alone, the 128-bit acquisition sequence, the EB90 start sequence and
    one CCSDS TC transfer frame (232.0-B-4) of 1 to 1,017 data octets,
    the length drawn per frame. The library describes and assembles each
    period (`dp_ccsds_415_frame_desc()`, `dp_wfm_frame_assemble_data()`);
    this program draws the lengths and fills the data from doppler's PN15;
- the chip rate the carrier sets, resampled to the 11 MSa/s capture
    (`RateConverter`, at the ratio `dp_ccsds_415_scene()` computes);
- a Doppler ramping at R_env, 11 Hz/s, through zero at mid-run
    (`doppler_channel`);
- noise at Es/N0 10 dB, 6 dB above the point's Es/N0 4 dB (`awgn`).

The receiver is the one the library builds at the point,
`dp_async_dsss_receiver_create_ccsds415()` (Python: `Ccsds415Receiver`),
designed for Es/N0 4 dB, so it is built once for its weakest signal.

The frames are read back from the receiver's decided symbols with no
truth, as a ground station would read them:

- a period is found by its 128-bit acquisition sequence and start
    sequence, one 144-bit marker, in either polarity (`dp_syncword_find()`),
    at the most errors whose false-frame probability over the bits
    searched is 1e-6 (`dp_syncword_max_errors_for()`);
- the header's fields are read back by name through the same description
    (`dp_wfm_frame_desc_field()`): the length, the SCID, the VCID and the
    frame sequence number;
- the CRC is checked (`dp_wfm_frame_desc_crc_ok()`), and every frame found
    counts, whatever it says;
- after a frame whose CRC holds, the next marker is one period on, and is
    tracked there; after one whose CRC fails, whose length field may itself
    be wrong, the search resumes.

## What it checks

`make run` exits non-zero unless, at each carrier:

1. the lock is sustained within the point's lock allocation, 20 s from
    arrival (`DP_CCSDS_415_LOCK_S`);
1. the BER over the locked stream, scored against the stimulus that was
    sent (`dp_ber_meter`), is not shown to be worse
    than the BPSK bound at the Es/N0 run less 1.13 dB: it fails when the
    meter's exact 99% interval (`dp_ber_confidence()`) lies wholly above
    `dp_ber_theory_ber()` there. 1.13 dB is the worst settled loss the
    receiver's certification record measured at this point (2.8 GHz, Es/N0
    4 dB; §2.6 of
    `src/doppler/dsss/tests/validation/async_dsss_receiver/results.md`),
    and the allowed loss itself is the owner's to set (#3157). The record
    covers unslipped bursts at Es/N0 4 to 7 dB, so above 7 dB, the default
    10 dB included, the figure is carried beyond what it measured. Each run
    prints the loss its own rate implies (`dp_ber_esn0_db_for_ser()`);
1. no call filled its output capacity, so no symbol was dropped;
1. the capture is complete (`dp_tlm_capture_close()` returns `DP_OK`);
1. every probe the receiver registers emitted at least once;
1. every frame sent whole inside the locked stream was read, and each
    frame's CRC verdict is the one the truth gives: pass when it was
    received as sent, fail when not. A frame read where none was sent is
    wrong whatever its CRC says.

The locked stream opens where the symbol lock and the code lock are both
sustained: the later of the two, each sustained by `dp_ber_lock_symbol()`.
That is the rule the receiver's certification record scores by (the design
page's §4.12, "The error-rate window"), applied to the two lock flags
`dp_async_dsss_receiver_status()` publishes. The record takes the latest
of five; the other three (carrier, demodulator carrier, sync) are not
published yet (#3350), so the window here can open before the record's
would, and the rate counts what settling the record leaves out.

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
- **scene**: the carrier, the time, the symbols out, and progress;
- **frames**: whether the start sequence is searched for or tracked, the
    frames read with their CRC verdicts, the start sequences missed, and
    the last frame's header read back: its data octets, SCID, VCID and
    frame sequence number, and the polarity it was read in.

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
marker; the frames) it did not evaluate. `--snapshot` and `ccsds415` keep the full
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

<!-- --8<-- [end:body] -->
