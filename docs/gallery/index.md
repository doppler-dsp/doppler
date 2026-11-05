# Gallery

Worked examples across every doppler subsystem — nearly all of them a
runnable script under [`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/v0.79.0/src/doppler/examples),
grouped below by DSP domain. Most pages render a figure; **run `make gallery`
to regenerate every image**, or run a single script directly (each page lists
its command under *Reproduce* / *Run it*).

A page whose subject is a C API instead carries a compiled-and-run `c` fence
and points at its demo under
[`native/examples/`](https://github.com/doppler-dsp/doppler/tree/v0.79.0/native/examples).

Looking for a runnable script that isn't a rendered walkthrough — LO, NCO,
FFT, ring buffers, the C API reference programs, or the NATS streaming demo?
Those live on the [Examples](../examples/index.md) page instead.

<!-- domain-index:start -->

## Signal sources

Oscillators and noise: the NCO, the LO and AWGN.

- [AWGN Generator](awgn.md) — complex Box-Muller noise, amplitude histogram, and flat PSD.

## Waveform generation (wfm)

Compose, generate and stream any waveform: the Synth, scenes and Plans, codes, frames, and the wfmgen tool.

- [A Crowded Band — Many Signals, One Parallel prepare](crowded-band.md) — twenty RRC carriers in one segment, prepared in parallel across cores; `Plan.render(enable=...)` thins them for free.
- [A Frame You Built, Generated](wfmgen-carried-frame.md) — `wfm_source_t.frame`: a source carries the description a caller built, so a layout no flag spells reaches the samples; `--acq-code`, `--sync` and `--crc` build one through `dp_wfm_frame_fixed()`
- [A Framed Link, Checked by Its Description](framed-link.md) — framed BPSK/QPSK from one `FrameDesc`, received by `MpskReceiver`, judged by `FrameDesc.check`.
- [Composing a Scene — .sum(), .add(), and Headroom](wfm-composition.md) — sum, add, headroom; a SoI under a CW interferer over one noise floor.
- [One Cache Slot for a Whole Background Field](plan-background.md) — `background=True` folds a static emitter population into a single pre-summed cache entry: 67x smaller, and the whole field becomes one overridable control.
- [type="symbols" — Bring Your Own Constellation](symbols.md) — pi/4-QPSK and 16-QAM from an arbitrary complex stream.
- [Waveform I/O — One Capture, Four File Types](wfm-io.md) — one capture written to raw / CSV / BLUE / SigMF and read back.
- [wfmgen — One Engine, Every Waveform](wfmgen.md) — tone / PN / QPSK / BPSK from one declarative `Synth`.

## Channel impairment

What a channel does to a signal: a Doppler channel with clock Doppler, delay and noise.

- [Doppler Channel — a Doppler Profile Driven by a Cosine](doppler-channel-profile.md) — one cosine period as the Doppler: the carrier follows `fc·d(t)` to 1.4 Hz of 50 kHz, the code slip peaks at 2.5 chips (2.44 in theory) and returns to zero, and no straight line explains any of it.
- [Doppler Channel — Clock Doppler as a Propagation Impairment](doppler-channel.md) — clock Doppler as an impairment: one ppm dilates the whole time base *and* shifts the carrier, so a code loop sees the rate error its carrier loop implies.

## Rate conversion & resampling

Polyphase resampling, rate converters and halfband decimators.

- [CIC Decimation Filter](cic.md) — wideband IQ → CIC → narrowband slice; 84 dB measured alias rejection (R=8, N=4).
- [Farrow Interpolator](farrow.md) — fractional-delay resampling.
- [HalfbandDecimatorQ15 — Fixed-Point Halfband Decimator](hbdecim_q15.md) — fixed-point Q15 halfband 2:1 decimator for interleaved IQ int16.
- [RateConverter — Automatic Cascade Selection](rate-converter.md) — automatic CIC / halfband / polyphase cascade for any rate ratio.

## Down-conversion (DDC)

Complex- and real-input digital down-converters, and their matched flavors.

- [Ddcr — Real Passband to Baseband](ddc-fn.md) — real passband in, complex baseband out, caller-owned buffer, thread-per-shard scaling.

## Spectral analysis

FFTs, correlators, power spectra and spectrograms.

- [2-D Acquisition Grid](detection2d.md) — GPS/CDMA Doppler × code-phase search with Šidák-corrected CFAR.
- [A Streaming Spectrogram](spectrogram.md) — a chirp and a tone as one row per 64 samples, in power and in dB, on a DC-centred axis.
- [Correlation and Detection](corr.md) — coherent integration, 2-D template matching, streaming CFAR.
- [Delay-Doppler Search](delay-doppler.md) — a delay-Doppler map, a coarse carrier bank and the refine that recovers its straddle loss, plus a batch of hypotheses in one call.
- [Four WCDMA Carriers — PSD, band_power, AccTrace](wcdma-carriers.md) — four RRC channels measured with `PSD` and `AccTrace`.

## Measurement suite

Tone, NPR and two-tone IMD metrics returned as named records.

- [Measurement Suite — ADC characterisation](measure.md) — SNR / SINAD / THD / SFDR / ENOB from `ToneMeasure`.
- [Measurement Suite — two-tone IMD/TOI & notched-noise NPR](measure-imd-npr.md) — two-tone IMD/IM3, TOI, and notched-noise NPR.

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [CarrierAcquisition: RRC Pulse Shaping](carrier-acq-rrc.md) — PSDMF residual-carrier estimation against an RRC-shaped BPSK stream, and why the `psd_template` override matters.

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [Arbitrary-Rate Symbol Recovery](ratesync.md) — matched filtering and symbol timing in one dot product, at a non-integer samples-per-symbol.
- [Carrier Loop Stress](costas.md) — Costas loop pull-in under noise.
- [Code Loop Tracking](dll.md) — delay-locked loop code tracking.
- [Costas Loop — Theory Validation](costas-theory.md) — loop-filter design and steady-state behaviour.
- [DLL Code Loop — Theory Validation](dll-theory.md) — discriminator and loop-filter derivation.
- [Full-Chain Lock-Up](receiver-lock.md) — `Dll -> Costas -> SymbolSync` cold-started with no code, carrier, or timing knowledge, watched over one shared `Telemetry` context.
- [Lock Detection: Verify Counts + Hysteresis](lockdet.md) — level + time hysteresis vs. a naive comparator on a marginal signal.
- [M-PSK Carrier Loop — Theory Validation](carrier-mpsk.md) — decision-directed carrier recovery.
- [M-PSK Receiver — Performance Characterisation](mpsk-receiver-performance.md) — EVM, SER, lock time, false alarm and level invariance over random geometries.
- [M-PSK Receiver — Pull-in, Lock, and BER](mpsk-receiver.md) — integrated carrier + timing + bit recovery.
- [M-PSK Receiver — Sizing the Carrier Loop, and Riding a Doppler Profile](mpsk-doppler-profile.md) — sizing bn_carrier for a predictable acquisition, and what a step and two ramps actually cost.
- [NDA Carrier Loop — Theory Validation](carrier-nda.md) — non-data-aided carrier recovery.
- [Symbol Timing Recovery](symsync.md) — timing-error-detector-driven resampling.
- [Timing Loop — Theory Validation](symsync-theory.md) — the timing loop's transfer function.

## Automatic gain control (AGC)

A log-domain feedback AGC.

- [AGC Settling — a design chart](agc-settling-design.md) — how long to wait, when `1/(4·loop_bw)` is only a floor.
- [AGC — Step Response](agc.md) — closed-loop power controller with decimated and per-sample loop updates.

## Detection statistics

Pd, Pfa and threshold models, a lock detector and a sync-word finder.

- [Detection Theory Curves](detection-curves.md) — Pd vs SNR and dwell from closed-form Marcum Q.
- [Monte Carlo vs Marcum Q Theory](detection-sim.md) — 30,000-trial validation of the Marcum Q curves.

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [A Message Over DSSS, Received and Deframed](data-source-link.md) — a data source sent one chunk per burst, received by `DsssBurstReceiver`, and returned byte for byte by `FrameDesc.deframe`.
- [AsyncDsssPool: emitters that come and go](async-dsss-pool-demo.md) — three receivers, four emitters on one Gold code: each held by one receiver, a departure freeing its receiver, and the newcomer taking it, read from the pool's own status after every push.
- [AsyncDsssPool: ten emitters through six receivers](async-dsss-pool-scene.md) — ten emitters on one Gold code, each on its own Doppler ramp and within ±8 dB of the others, come and go through six receivers: each held by one receiver from soon after it arrives, a freed receiver taking the next, and a weak emitter waiting for the strong one that masks it.
- [AsyncDsssReceiver: a signal's lifecycle](async-dsss-receiver-lifecycle.md) — one packaged receiver through an emitter's life: searched, tracked through a fade, released when it departs, reset, and re-acquired on its return, read from its own state after every push
- [AsyncDsssReceiver: the specification link](async-dsss-receiver-spec.md) — one packaged receiver on a link whose Doppler stretches the chip clock as well as the carrier, through both regimes of a pass: carrier-to-code aiding, a missed hand-off searched again, and the decode scored by BER, EVM and M2M4
- [DSSS Acquisition — Pd / Pfa vs Es/N0](dsss-acq-characterization.md) — detection characterisation curves.
- [DsssBurstReceiver — the Composed Burst Chain](dsss-burst-receiver.md) — the end-to-end DSSS burst walkthrough: a train sent from one frame description on all three `wfmgen` faces, then `BurstAcquisition -> refine -> BurstDemod` behind one `push()`, the same answer at every block size
- [DsssReceiver — the Composed Continuous DSSS Receiver](dsss-receiver.md) — `Acquisition -> Dll(segments) -> RateConverter -> MpskReceiver` at real chip/symbol rates, collapsed into one `DsssReceiver` and one `steps()` call -- the despreader removes the code and an explicit resampler bridges it to a normal demodulator.
- [Streaming Async Despreader](async-despread.md) — `Dll(segments)` despreading a Doppler-dilated code on an asynchronous data clock, unaided and with the carrier's rate aid.

## M-PSK modulation

M-ary PSK symbol map and demap, hard, soft and differential.

- [M-PSK constellation (Gray-coded map / demap)](mpsk.md) — M-PSK mapping and constellation rendering.

## Channel coding

Convolutional, Reed-Solomon and interleaving codes.

- [Name Your Own Code — and What Happens Past the Radius](coding.md) — a Reed-Solomon outer code and a convolutional inner code that are nobody's standard, run end to end; and what a bounded-distance decoder does past its radius, where a refusal is safe and a miscorrection is not

## CCSDS literals

The values CCSDS 131.0-B picked for synchronization and coding.

- [A CCSDS CADU, as a Frame Description](ccsds-link.md) — a frame as a list of fields and a list of stages, each carrying the span it covers: why a pipeline cannot express the one thing CCSDS's stages disagree about, and what an outer code reports on the receive side that a CRC cannot

## Bit-error rate (BER)

A BER meter that aligns a recovered stream to a reference.

- [Measuring an Error Rate, Defensibly](ber-awgn.md) — `BerMeter` on AWGN: detected alignment, inverse binomial sampling, and the exact confidence interval.

## Telemetry

In-band probes that record loop internals.

- [Capturing All Receiver Telemetry](mpsk-telemetry-capture.md) — every MpskReceiver probe captured and stored from one ring.
- [Telemetry: Many Emitters, One Consumer](telemetry-fanin.md) — three emitters fanning into one ring, one consumer.

## Sample-format conversion (cvt)

Converters between float IQ and fixed-point integers.

- [ADC Quantisation — 3–8 Bits](adc.md) — staircase resolution and noise floor across 3–8 bits.
- [cvt Quantization Noise](cvt-quantization.md) — the three cvt formats overlaid at identical noise floor.
- [Q15 vs UQ15 Quantization](q15-uq15.md) — bipolar vs offset-binary encodings of the same Q15 step.

<!-- domain-index:end -->
