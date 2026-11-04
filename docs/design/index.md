# Design

Why doppler is the way it is: the architecture decisions, API contracts and
algorithm rationale behind each feature. These pages are written for the
people building doppler, and power users who want to dig deeper are welcome.
They are not where a user meets a feature: to *use* one, start at its
[guide](../guide/index.md), its [gallery](../gallery/index.md) page or its
[API](../api/index.md) entry. (Repo layout and forward-looking roadmaps live
under [Contributing](../dev/index.md).)

<!-- domain-index:start -->

## Signal sources

Oscillators and noise: the NCO, the LO and AWGN.

- [The NCO](nco.md)

## Waveform generation (wfm)

Compose, generate and stream any waveform: the Synth, scenes and Plans, codes, frames, and the wfmgen tool.

- [A Frame as a Description](frame-description.md)
- [A Frame as a Description — the measurement record](frame-description-measurements.md)
- [A Receiver Built from a Frame Description](rx-frame-description.md)
- [Capture files — one reader, one writer, four containers](capture-files.md)
- [Ending a capture — spooling an endless stream to disk while reading it back](end-of-capture.md)
- [One home for the waveform enum tables](waveform-enum-ssot.md)
- [The Payload as a Data Source](payload-data-source.md)
- [The Payload as a Data Source — the measurement record](payload-data-source-measurements.md)
- [Waveform amplitude & composition](wfmgen-composition.md)
- [wfmgen — the waveform generator](wfmgen.md)

## Channel impairment

What a channel does to a signal: a Doppler channel with clock Doppler, delay and noise.

- [The Doppler channel — one parameter, both clocks and the carrier](doppler-channel.md)
- [The Doppler channel — the measurement record](doppler-channel-measurements.md)

## Rate conversion & resampling

Polyphase resampling, rate converters and halfband decimators.

- [CIC Decimator — the fixed-point input budget](cic.md)
- [Continuously Variable Resampler](RESAMPLER.md)

## Spectral analysis

FFTs, correlators, power spectra and spectrograms.

- [Corr2D batches: many maps in one call](corr2d-batch.md)
- [Corr2D hypothesis bank: operation order](corr2d-hypotheses.md)
- [Corr2D: decoupled (interpolated) inverse length](corr2d-interpolated-inverse.md)
- [Spectral & Measurement API Map](spectral-api-map.md)
- [The Spectrogram — chunks in, rows out](spectrogram.md)
- [The Spectrogram — the measurement record](spectrogram-measurements.md)

## Spectrum analyzer

Specan: a windowed, averaged power spectrum in dBFS.

- [Spectrum Analyzer](specan.md)

## Measurement suite

Tone, NPR and two-tone IMD metrics returned as named records.

- [Measurement Suite — single-tone ADC / spectral metrics](measurement-suite.md)

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [BurstBank — the coarse-Doppler bank as one C object](burst-bank.md)
- [BurstCapture: acquisition's output, turned into bursts](burst-capture.md)
- [CoarseChannel — is a channel an object, or a slice of the bank?](coarse-channel.md)
- [Design — pure-functional acquisition kernel (elastic fleet)](acq-fn.md)
- [The acquisition surface — the measurement record](acq-surface-measurements.md)
- [The acquisition surface: the DSSS search engine](acq-surface.md)

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [MPSK Receiver](mpsk.md)
- [Receiver Test Harness](rx-test.md)
- [Symbol Timing on a Rate Cascade](ratesync-timing.md)
- [SymbolSync Timing Lock Detector](timing_lock_detector.md)
- [The Loop Filter](loop-filter.md)

## Automatic gain control (AGC)

A log-domain feedback AGC.

- [Automatic Gain Control](agc.md)

## Detection statistics

Pd, Pfa and threshold models, a lock detector and a sync-word finder.

- [Detection Sizing — the four laws behind one prefix](detection.md)
- [Lock Detection — the reasoning](lock-detect.md)

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [AsyncDsssReceiver and AsyncDsssPool — continuous DSSS, from spec to object](async-dsss-receiver.md)
- [AsyncDsssReceiver and AsyncDsssPool — the measurement record](async-dsss-receiver-measurements.md)
- [DsssBurstReceiver: the burst chain, composed in C](dsss-burst-receiver.md)
- [The Polynomial-Phase Estimator — the reasoning](ppe.md)

## Channel coding

Convolutional, Reed-Solomon and interleaving codes.

- [Interleaving — spreading a burst across codewords](interleaving.md)
- [Reed-Solomon](reed-solomon.md)
- [The FEC Receive Half](fec-receive.md)
- [The Viterbi Decoder](viterbi.md)

## Streaming

Publish/subscribe, push/pull and request/reply of IQ over NATS.

- [Streaming — one envelope, six roles, two planes](streaming.md)

## Buffers

Lock-free ring buffers between a producer and a consumer.

- [The ring buffer — measurements and the record](ring-buffer-measurements.md)
- [The ring buffer — one contiguous view of a stream](ring-buffer.md)

## Telemetry

In-band probes that record loop internals.

- [Telemetry — zero-cost scalar taps for running pipelines](telemetry.md)

## Stopping a run (interrupt)

The one flag every blocking wait consults.

- [Ending a wait — one contract for network, memory and disk](io-termination.md)

## Sample-format conversion (cvt)

Converters between float IQ and fixed-point integers.

- [Quantization Design](QUANTIZATION.md)

## Utilities

Numeric helpers shared across the modules.

- [API taxonomy: the DSP building-block hierarchy and its naming axis](api-taxonomy.md)
- [State Serialization — the standard bytes interface](state-serialization.md)
- [The Exponential Moving Average](ema.md)

<!-- domain-index:end -->
