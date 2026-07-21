# Gallery

Worked examples across every doppler subsystem — each one a runnable script
under [`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/main/src/doppler/examples),
grouped below by DSP domain. Most pages render a figure; **run `make gallery`
to regenerate every image**, or run a single script directly (each page lists
its command under *Reproduce* / *Run it*).

Looking for a runnable script that isn't a rendered walkthrough — LO, NCO,
FFT, ring buffers, C programs, or the NATS streaming demo? Those live on the
[Examples](../examples/index.md) page instead.

## Sources & Waveforms

- [Waveform Generator](wfmgen.md) — tone / PN / QPSK / BPSK from one declarative `Synth`.
- [Symbols: bring your own constellation](symbols.md) — pi/4-QPSK and 16-QAM from an arbitrary complex stream.
- [Waveform Scenes](wfm-composition.md) — sum, add, headroom; a SoI under a CW interferer over one noise floor.
- [Waveform I/O](wfm-io.md) — one capture written to raw / CSV / BLUE / SigMF and read back.
- [Waveform JSON Round-Trip](wfm-json.md) — `--record` a scene to JSON and replay it byte-identically.
- [Waveform Write](wfm-write.md) — the shortest path from a `Composer` to a file and back.
- [WCDMA Carriers](wcdma-carriers.md) — four RRC channels measured with `PSD` and `AccTrace`.
- [Prepare Once, Sweep Many (Plan)](plan.md) — one declarative scene evaluated at many SNR operating points via the `Plan` stimulus engine.
- [AWGN](awgn.md) — complex Box-Muller noise, amplitude histogram, and flat PSD.
- [Doppler Channel](doppler-channel.md) — clock Doppler as an impairment: one ppm dilates the whole time base *and* shifts the carrier, so a code loop sees the rate error its carrier loop implies.

## Filters & Resampling

- [CIC Decimation](cic.md) — wideband IQ → CIC → narrowband slice; ~90 dB alias rejection.
- [RateConverter](rate-converter.md) — automatic CIC / halfband / polyphase cascade for any rate ratio.
- [Farrow Interpolator](farrow.md) — fractional-delay resampling.
- [HalfbandDecimatorQ15](hbdecim_q15.md) — fixed-point Q15 halfband 2:1 decimator for interleaved IQ int16.

## Down-Conversion (DDC)

- [Functional DDCR](ddc-fn.md) — real passband in, complex baseband out, caller-owned buffer, thread-per-shard scaling.

## Detection & Acquisition

- [Correlation](corr.md) — coherent integration, 2-D template matching, streaming CFAR.
- [Detection Curves](detection-curves.md) — Pd vs SNR and dwell from closed-form Marcum Q.
- [Monte Carlo](detection-sim.md) — 30,000-trial validation of the Marcum Q curves.
- [Lock Detection: Verify Counts](lockdet.md) — level + time hysteresis vs. a naive comparator on a marginal signal.
- [2-D Acquisition](detection2d.md) — GPS/CDMA Doppler × code-phase search with Bonferroni-corrected CFAR.
- [DSSS Acquisition & Despreading](dsss-despread.md) — burst acquisition into a streaming despreader.
- [DSSS Acquisition: Pd/Pfa](dsss-acq-characterization.md) — detection characterisation curves.
- [5-Burst DSSS Link (wfmgen's 3 Faces)](dsss-burst-pipeline.md) — every `wfmgen` production path feeding the same full receiver chain.
- [Streaming Async Despreader](async-despread.md) — `Dll(segments)` PN-epoch despreader.
- [Despreader (full continuous receiver)](despreader.md) — a combined carrier + code tracking receiver.
- [Full-Chain Lock-Up](receiver-lock.md) — `Dll -> Costas -> SymbolSync` cold-started with no code, carrier, or timing knowledge, watched over one shared `Telemetry` context.
- [DSSS Acquisition — Continuous Async-Data Modulation](dsss-acq-async-data.md) — Stage 1: exact code-phase/Doppler lock and per-epoch test-stat robustness under asynchronous BPSK data, CCSDS Gold code.
- [DSSS Despread — Acquisition-to-Dll Hand-off, Continuous Async-Data](dsss-despread-async-data.md) — Stage 2: the hand-off seeds `Dll` exactly, and `segments=4` tracks reliably where `segments=1` measurably degrades under the same asynchronous BPSK data.
- [Continuous Async DSSS Receiver](async-dsss-receiver.md) — Stage 3: `Acquisition -> Dll(segments) -> RateConverter -> MpskReceiver` at real chip/symbol rates — the despreader removes the code, an explicit resampler bridges it to a normal demodulator, and `Dll`'s own tracking-optimal `segments=4` decodes cleanly once wired that way.
- [DsssReceiver — the Composed Continuous DSSS Receiver](dsss-receiver.md) — the single-object payoff: everything Stage 3 hand-composed across four objects, collapsed into one `DsssReceiver` and one `steps()` call.
- [AsyncDsssReceiver: the SPEC Waveform](async-dsss-receiver-spec.md) — the packaged receiver decoding SPEC's own continuous async DSSS (CCSDS Gold-1023, 3.069 Mcps, 2700 bps) through physically-coupled clock Doppler, in both pass regimes (TCA-crossing ramp and ±50 kHz offset extremum), with the pre-despread carrier loop tracking the ramp and a binary symbol-lock indicator.
- [CarrierAcquisition: RRC Pulse Shaping](carrier-acq-rrc.md) — PSDMF residual-carrier estimation against an RRC-shaped BPSK stream, and why the `psd_template` override matters.

## Synchronization Loops

- [Carrier Loop Stress](costas.md) — Costas loop pull-in under noise.
- [Costas Loop (theory)](costas-theory.md) — loop-filter design and steady-state behaviour.
- [Code Loop Tracking](dll.md) — delay-locked loop code tracking.
- [DLL Code Loop (theory)](dll-theory.md) — discriminator and loop-filter derivation.
- [Symbol Timing Recovery](symsync.md) — timing-error-detector-driven resampling.
- [Timing Loop (theory)](symsync-theory.md) — the timing loop's transfer function.
- [M-PSK Carrier Loop (theory)](carrier-mpsk.md) — decision-directed carrier recovery.
- [NDA Carrier Loop (theory)](carrier-nda.md) — non-data-aided carrier recovery.

## Constellations & Receivers

- [M-PSK Constellation](mpsk.md) — M-PSK mapping and constellation rendering.
- [M-PSK Receiver](mpsk-receiver.md) — integrated carrier + timing + bit recovery.

## Measurement

- [Measurement Suite](measure.md) — SNR / SINAD / THD / SFDR / ENOB from `ToneMeasure`.
- [Measurement Suite: IMD & NPR](measure-imd-npr.md) — two-tone IMD/IM3, TOI, and notched-noise NPR.

## Quantization & Fixed-Point

- [ADC Quantisation](adc.md) — staircase resolution and noise floor across 3–8 bits.
- [Q15 vs UQ15](q15-uq15.md) — bipolar vs offset-binary encodings of the same Q15 step.
- [cvt Quantization](cvt-quantization.md) — the three cvt formats overlaid at identical noise floor.

## Gain Control

- [AGC](agc.md) — closed-loop power controller with decimated and per-sample loop updates.

## Telemetry

- [Telemetry: Many Emitters, One Consumer](telemetry-fanin.md) — three emitters fanning into one ring, one consumer.
