# Gallery

Worked examples across every doppler subsystem — nearly all of them a
runnable script under [`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/main/src/doppler/examples),
grouped below by DSP domain. Most pages render a figure; **run `make gallery`
to regenerate every image**, or run a single script directly (each page lists
its command under *Reproduce* / *Run it*).

A page whose subject is a C API instead carries a compiled-and-run `c` fence
and points at its demo under
[`native/examples/`](https://github.com/doppler-dsp/doppler/tree/main/native/examples).

Looking for a runnable script that isn't a rendered walkthrough — LO, NCO,
FFT, ring buffers, the C API reference programs, or the NATS streaming demo?
Those live on the [Examples](../examples/index.md) page instead.

<!-- domain-index:start -->

## Signal sources

Oscillators and noise: the NCO, the LO and AWGN.

- [AWGN Generator](awgn.md)

## Waveform generation (wfm)

Compose, generate and stream any waveform: the Synth, scenes and Plans, codes, frames, and the wfmgen tool.

- [A Crowded Band — Many Signals, One Parallel prepare](crowded-band.md)
- [A Frame You Built, Generated](wfmgen-carried-frame.md)
- [A Framed Link, Checked by Its Description](framed-link.md)
- [Composing a Scene — .sum(), .add(), and Headroom](wfm-composition.md)
- [One Cache Slot for a Whole Background Field](plan-background.md)
- [type="symbols" — Bring Your Own Constellation](symbols.md)
- [Waveform I/O — One Capture, Four File Types](wfm-io.md)
- [wfmgen — One Engine, Every Waveform](wfmgen.md)

## Channel impairment

What a channel does to a signal: a Doppler channel with clock Doppler, delay and noise.

- [Doppler Channel — a Doppler Profile Driven by a Cosine](doppler-channel-profile.md)
- [Doppler Channel — Clock Doppler as a Propagation Impairment](doppler-channel.md)

## Rate conversion & resampling

Polyphase resampling, rate converters and halfband decimators.

- [CIC Decimation Filter](cic.md)
- [Farrow Interpolator](farrow.md)
- [HalfbandDecimatorQ15 — Fixed-Point Halfband Decimator](hbdecim_q15.md)
- [RateConverter — Automatic Cascade Selection](rate-converter.md)

## Down-conversion (DDC)

Complex- and real-input digital down-converters, and their matched flavors.

- [Ddcr — Real Passband to Baseband](ddc-fn.md)

## Spectral analysis

FFTs, correlators, power spectra and spectrograms.

- [2-D Acquisition Grid](detection2d.md)
- [A Streaming Spectrogram](spectrogram.md)
- [Correlation and Detection](corr.md)
- [Delay-Doppler Search](delay-doppler.md)
- [Four WCDMA Carriers — PSD, band_power, AccTrace](wcdma-carriers.md)

## Measurement suite

Tone, NPR and two-tone IMD metrics returned as named records.

- [Measurement Suite — ADC characterisation](measure.md)
- [Measurement Suite — two-tone IMD/TOI & notched-noise NPR](measure-imd-npr.md)

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [CarrierAcquisition: RRC Pulse Shaping](carrier-acq-rrc.md)

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [Arbitrary-Rate Symbol Recovery](ratesync.md)
- [Carrier Loop Stress](costas.md)
- [Code Loop Tracking](dll.md)
- [Costas Loop — Theory Validation](costas-theory.md)
- [DLL Code Loop — Theory Validation](dll-theory.md)
- [Full-Chain Lock-Up](receiver-lock.md)
- [Lock Detection: Verify Counts + Hysteresis](lockdet.md)
- [M-PSK Carrier Loop — Theory Validation](carrier-mpsk.md)
- [M-PSK Receiver — Performance Characterisation](mpsk-receiver-performance.md)
- [M-PSK Receiver — Pull-in, Lock, and BER](mpsk-receiver.md)
- [M-PSK Receiver — Sizing the Carrier Loop, and Riding a Doppler Profile](mpsk-doppler-profile.md)
- [NDA Carrier Loop — Theory Validation](carrier-nda.md)
- [Symbol Timing Recovery](symsync.md)
- [Timing Loop — Theory Validation](symsync-theory.md)

## Automatic gain control (AGC)

A log-domain feedback AGC.

- [AGC Settling — a design chart](agc-settling-design.md)
- [AGC — Step Response](agc.md)

## Detection statistics

Pd, Pfa and threshold models, a lock detector and a sync-word finder.

- [Detection Theory Curves](detection-curves.md)
- [Monte Carlo vs Marcum Q Theory](detection-sim.md)

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [A Message Over DSSS, Received and Deframed](data-source-link.md)
- [AsyncDsssPool: emitters that come and go](async-dsss-pool-demo.md)
- [AsyncDsssPool: ten emitters through six receivers](async-dsss-pool-scene.md)
- [AsyncDsssReceiver: a signal's lifecycle](async-dsss-receiver-lifecycle.md)
- [AsyncDsssReceiver: the specification link](async-dsss-receiver-spec.md)
- [DSSS Acquisition — Pd / Pfa vs Es/N0](dsss-acq-characterization.md)
- [DsssBurstReceiver — the Composed Burst Chain](dsss-burst-receiver.md)
- [DsssReceiver — the Composed Continuous DSSS Receiver](dsss-receiver.md)
- [Streaming Async Despreader](async-despread.md)

## M-PSK modulation

M-ary PSK symbol map and demap, hard, soft and differential.

- [M-PSK constellation (Gray-coded map / demap)](mpsk.md)

## Channel coding

Convolutional, Reed-Solomon and interleaving codes.

- [Name Your Own Code — and What Happens Past the Radius](coding.md)

## CCSDS literals

The values CCSDS 131.0-B picked for synchronization and coding.

- [A CCSDS CADU, as a Frame Description](ccsds-link.md)

## Bit-error rate (BER)

A BER meter that aligns a recovered stream to a reference.

- [Measuring an Error Rate, Defensibly](ber-awgn.md)

## Telemetry

In-band probes that record loop internals.

- [Capturing All Receiver Telemetry](mpsk-telemetry-capture.md)
- [Telemetry: Many Emitters, One Consumer](telemetry-fanin.md)

## Sample-format conversion (cvt)

Converters between float IQ and fixed-point integers.

- [ADC Quantisation — 3–8 Bits](adc.md)
- [cvt Quantization Noise](cvt-quantization.md)
- [Q15 vs UQ15 Quantization](q15-uq15.md)

<!-- domain-index:end -->
