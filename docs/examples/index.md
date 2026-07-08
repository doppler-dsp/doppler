# Examples

Worked examples across every doppler subsystem — each one a runnable script
under [`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/main/src/doppler/examples),
grouped below by DSP domain. Most pages render a figure; **run `make gallery`
to regenerate every image**, or run a single script directly (each page lists
its command under *Reproduce* / *Run it*).

C programs calling the library directly live on the [C examples](c.md) page;
the end-to-end ZMQ pipeline is on the [Streaming](streaming.md) page.

## Sources & Waveforms

- [Waveform Generator](../gallery/wfmgen.md) — tone / PN / QPSK / BPSK from one declarative `Synth`.
- [Symbols: bring your own constellation](../gallery/symbols.md) — pi/4-QPSK and 16-QAM from an arbitrary complex stream.
- [Waveform Scenes](../gallery/wfm-composition.md) — sum, add, headroom; a SoI under a CW interferer over one noise floor.
- [Waveform I/O](../gallery/wfm-io.md) — one capture written to raw / CSV / BLUE / SigMF and read back.
- [Waveform JSON Round-Trip](../gallery/wfm-json.md) — `--record` a scene to JSON and replay it byte-identically.
- [Waveform Write](../gallery/wfm-write.md) — the shortest path from a `Composer` to a file and back.
- [WCDMA Carriers](../gallery/wcdma-carriers.md) — four RRC channels measured with `PSD` and `AccTrace`.
- [AWGN](../gallery/awgn.md) — complex Box-Muller noise, amplitude histogram, and flat PSD.
- [LO](python-lo.md) — complex phasor generator with FM control and phase continuity.
- [NCO](python-nco.md) — raw uint32 phase accumulator with overflow carry and fixed-point scaling.

## Filters & Resampling

- [CIC Decimation](../gallery/cic.md) — wideband IQ → CIC → narrowband slice; ~90 dB alias rejection.
- [RateConverter](../gallery/rate-converter.md) — automatic CIC / halfband / polyphase cascade for any rate ratio.
- [Farrow Interpolator](../gallery/farrow.md) — fractional-delay resampling.
- [HalfbandDecimatorQ15](../gallery/hbdecim_q15.md) — fixed-point Q15 halfband 2:1 decimator for interleaved IQ int16.

## Down-Conversion (DDC)

- [Functional DDCR](../gallery/ddc-fn.md) — real passband in, complex baseband out, caller-owned buffer, thread-per-shard scaling.

## Detection & Acquisition

- [Correlation](../gallery/corr.md) — coherent integration, 2-D template matching, streaming CFAR.
- [Detection Curves](../gallery/detection-curves.md) — Pd vs SNR and dwell from closed-form Marcum Q.
- [Monte Carlo](../gallery/detection-sim.md) — 30,000-trial validation of the Marcum Q curves.
- [2-D Acquisition](../gallery/detection2d.md) — GPS/CDMA Doppler × code-phase search with Bonferroni-corrected CFAR.
- [DSSS Acquisition & Despreading](../gallery/dsss-despread.md) — burst acquisition into a streaming despreader.
- [DSSS Acquisition: Pd/Pfa](../gallery/dsss-acq-characterization.md) — detection characterisation curves.
- [Streaming Async Despreader](../gallery/async-despread.md) — `Dll(segments)` PN-epoch despreader.
- [Despreader (full continuous receiver)](../gallery/despreader.md) — a combined carrier + code tracking receiver.

## Synchronization Loops

- [Carrier Loop Stress](../gallery/costas.md) — Costas loop pull-in under noise.
- [Costas Loop (theory)](../gallery/costas-theory.md) — loop-filter design and steady-state behaviour.
- [Code Loop Tracking](../gallery/dll.md) — delay-locked loop code tracking.
- [DLL Code Loop (theory)](../gallery/dll-theory.md) — discriminator and loop-filter derivation.
- [Symbol Timing Recovery](../gallery/symsync.md) — timing-error-detector-driven resampling.
- [Timing Loop (theory)](../gallery/symsync-theory.md) — the timing loop's transfer function.
- [M-PSK Carrier Loop (theory)](../gallery/carrier-mpsk.md) — decision-directed carrier recovery.
- [NDA Carrier Loop (theory)](../gallery/carrier-nda.md) — non-data-aided carrier recovery.

## Constellations & Receivers

- [M-PSK Constellation](../gallery/mpsk.md) — M-PSK mapping and constellation rendering.
- [M-PSK Receiver](../gallery/mpsk-receiver.md) — integrated carrier + timing + bit recovery.

## Measurement

- [Measurement Suite](../gallery/measure.md) — SNR / SINAD / THD / SFDR / ENOB from `ToneMeasure`.
- [Measurement Suite: IMD & NPR](../gallery/measure-imd-npr.md) — two-tone IMD/IM3, TOI, and notched-noise NPR.

## Quantization & Fixed-Point

- [ADC Quantisation](../gallery/adc.md) — staircase resolution and noise floor across 3–8 bits.
- [Q15 vs UQ15](../gallery/q15-uq15.md) — bipolar vs offset-binary encodings of the same Q15 step.
- [cvt Quantization](../gallery/cvt-quantization.md) — the three cvt formats overlaid at identical noise floor.

## Gain Control

- [AGC](../gallery/agc.md) — closed-loop power controller with decimated and per-sample loop updates.

## Fundamentals

- [FFT](python-fft.md) — per-instance 1-D and 2-D FFT with plan reuse.
- [Ring Buffers](python-buffers.md) — double-mapped lock-free ring buffers for producer/consumer pipelines.
- [C examples](c.md) — minimal C99 programs calling the library directly.
- [Streaming](streaming.md) — ZMQ-based signal streaming end-to-end.
