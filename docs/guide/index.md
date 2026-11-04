# Guides

How to use each feature: what it is for, the call that does the job, and the
decisions you make along the way, with tested code you can copy. Every public
class is named in a guide; to see one working, follow the guide's link to
its gallery page.

!!! note "Looking for the Waveform Generator?"

    [Waveform Generator (wfmgen)](wfmgen/index.md) and
    [Capture I/O](wfm-io/index.md) are under **Tools** in the nav, with
    their own sections, so they are not listed below.

<!-- domain-index:start -->

## Spectral analysis

FFTs, correlators, power spectra and spectrograms.

- [Delay-Doppler Search](delay-doppler.md) — where a code is and how fast it is rotating: one map, batches of frames, and the hypothesis bank for an unknown carrier, with `DelayDoppler`
- [Power Spectra & Measurements](spectral-psd.md) — PSD estimation, tone and NPR measurement
- [Streaming Spectrograms](spectrogram.md) — any-size chunks in, every row of a waterfall out, the same rows however the stream is cut (C and Python)

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [Acquiring a Repeated Preamble](acquisition.md) — finding bursts of any repeated preamble with `BurstCapture` and `BurstAcquisition`

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [Lock Detection Across doppler.track](lock-detection.md) — which `configure_lock` to call, and why, for every tracking loop

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [DSSS Burst Acquisition](dsss-acquisition.md) — acquiring a spread-spectrum preamble's code phase and Doppler with `BurstAcquisition`, and continuous signals with `Acquisition`

## Streaming

Publish/subscribe, push/pull and request/reply of IQ over NATS.

- [Real-Time Pacing & Timestamping](timing.md) — sample-accurate playback timing

## Fixed-point arithmetic

Q8 and Q15 accumulators with exact integer arithmetic.

- [Getting Started with Fixed-Point Arithmetic](fixed-point.md) — Q15 / UQ15 types and quantization

## Utilities

Numeric helpers shared across the modules.

- [Checkpoint & Resume](state-serialization.md) — bit-exact `get_state`/`set_state`, composed receivers, and elastic pod hand-off

<!-- domain-index:end -->
