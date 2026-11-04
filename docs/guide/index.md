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

- [Delay-Doppler Search](delay-doppler.md)
- [Power Spectra & Measurements](spectral-psd.md)
- [Streaming Spectrograms](spectrogram.md)

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [Acquiring a Repeated Preamble](acquisition.md)

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [Lock Detection Across doppler.track](lock-detection.md)

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [DSSS Burst Acquisition](dsss-acquisition.md)

## Streaming

Publish/subscribe, push/pull and request/reply of IQ over NATS.

- [Real-Time Pacing & Timestamping](timing.md)

## Fixed-point arithmetic

Q8 and Q15 accumulators with exact integer arithmetic.

- [Getting Started with Fixed-Point Arithmetic](fixed-point.md)

## Utilities

Numeric helpers shared across the modules.

- [Checkpoint & Resume](state-serialization.md)

<!-- domain-index:end -->
