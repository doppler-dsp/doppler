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

- [The NCO](nco.md) — the 32-bit phase accumulator every rate in the library stands on: why an integer register instead of a double, the one float boundary, and how carrier and timing read the same thing

## Waveform generation (wfm)

Compose, generate and stream any waveform: the Synth, scenes and Plans, codes, frames, and the wfmgen tool.

- [A Frame as a Description](frame-description.md) — a frame as a list of fields and a list of stages, each carrying the span it covers: why a chain of optional transforms cannot express CCSDS, and how a caller adds a stage doppler has never heard of
- [A Frame as a Description — the measurement record](frame-description-measurements.md) — the dated record behind it: the survey that counted seven spellings of a field, the prototype that proved one grammar lossless, and how the CCSDS sites were settled, under the same section letters
- [A Receiver Built from a Frame Description](rx-frame-description.md) — `BurstDemod` and `DsssBurstReceiver` told the frame instead of its length: the sync word, the frame and burst lengths and the CRC verdict derived from one description, the callers that compute them by hand today, and the order of work
- [Capture files — one reader, one writer, four containers](capture-files.md) — the file I/O subsystem underneath `Reader`/`Writer`: why the file type is decided by content, why one keyword codec serves both directions, why provenance is a value rather than a caveat, and what a streaming writer costs at close
- [Ending a capture — spooling an endless stream to disk while reading it back](end-of-capture.md) — the disk third of that contract: spooling an endless stream to disk while reading it back, why Ctrl+C must reach the reader through the file rather than around it, and what owning both ends of the file buys
- [One home for the waveform enum tables](waveform-enum-ssot.md) — a name table's ORDER is the C enum value, so a second copy mis-maps a flag instead of failing: what owns the tables, and the gate that holds the manifest, the header and the C enums to each other
- [The Payload as a Data Source](payload-data-source.md) — a frame declares `data:LEN` and `--data` names where the bits come from (a file, a literal, stdin, a seeded stream): fill, idle frames, run length, what is deleted, and the unknowns
- [The Payload as a Data Source — the measurement record](payload-data-source-measurements.md) — the chunker prototyped before any C, and the three rules it changed
- [Waveform amplitude & composition](wfmgen-composition.md) — level/power conventions for wfmgen
- [wfmgen — the waveform generator](wfmgen.md) — the spine for the tool: modulations, impairments and output streams from three interchangeable faces, which gate keeps each promise, and the eight things still unknown about it

## Channel impairment

What a channel does to a signal: a Doppler channel with clock Doppler, delay and noise.

- [The Doppler channel — one parameter, both clocks and the carrier](doppler-channel.md) — one parameter drives the dilation and the carrier: why a Doppler shift is not a frequency offset, the scalar and the profile forms, how the carrier is read off the resampler so it cannot depend on chunking, and how the composer and the Plan share one renderer.
- [The Doppler channel — the measurement record](doppler-channel-measurements.md) — the dated record: the first profile that was withdrawn and why, the resampler's 32-bit step against the closed form, the gap that carried the signal, and the Plan's speed against `compose()`.

## Rate conversion & resampling

Polyphase resampling, rate converters and halfband decimators.

- [CIC Decimator — the fixed-point input budget](cic.md) — the fixed-point input budget: DC gain and PAPR headroom
- [Continuously Variable Resampler](RESAMPLER.md) — polyphase resampler architecture, testing, and performance optimizations

## Spectral analysis

FFTs, correlators, power spectra and spectrograms.

- [Corr2D batches: many maps in one call](corr2d-batch.md) — many frames in one call: every map, or a coherent, power-sum, power-max or per-map-peak reduction, on one C fold loop that never touches the dwell
- [Corr2D hypothesis bank: operation order](corr2d-hypotheses.md) — pre-design study for #2683/#2670: which stages a bank of carrier and rate hypotheses shares, fractional carrier offsets mixed per fraction on an L = 4 grid, and the cost each ordering saves, timed in C
- [Corr2D: decoupled (interpolated) inverse length](corr2d-interpolated-inverse.md) — decoupled, pffft-friendly inverse FFT size + free sub-bin interpolation
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [The Spectrogram — chunks in, rows out](spectrogram.md) — chunks in, rows out: the use cases, the seven guarantees and the six things not yet known, why a spectrogram is PSD per frame rather than a second kernel, and the order the program is built in
- [The Spectrogram — the measurement record](spectrogram-measurements.md) — the dated record: PSD's kernel promoted byte-for-byte, a timing that turned out bimodal, and the prototype that found a bug in its own test

## Spectrum analyzer

Specan: a windowed, averaged power spectrum in dBFS.

- [Spectrum Analyzer](specan.md) — how span and RBW set the rate, the transform length and the Kaiser beta, and why every RBW gets the same −90 dB skirt

## Measurement suite

Tone, NPR and two-tone IMD metrics returned as named records.

- [Measurement Suite — single-tone ADC / spectral metrics](measurement-suite.md) — tone, NPR, and IMD metric internals

## Acquisition

Finding a signal before anything tracks it: bursts, preambles and carriers.

- [BurstBank — the coarse-Doppler bank as one C object](burst-bank.md) — the coarse-Doppler bank as one C object: why its cross-channel claim rule belongs beside the capture's, the layout rule now that the engine searches to its edge, rings named by centre so a pod can hold one channel, and the unknowns the characterization measures
- [BurstCapture: acquisition's output, turned into bursts](burst-capture.md) — acquisition's output turned into aligned burst samples: why the period-resolving refine and the look-back need a home outside the receiver, why the object owns its own engine rather than taking foreign detections, and the one behaviour that changes
- [CoarseChannel — is a channel an object, or a slice of the bank?](coarse-channel.md) — question 1 of the bank design: is a channel its own object or a slice of the bank, judged against the two primary use cases (one process runs the whole bank; one pod runs one channel), with the trades and the work that answers it
- [Design — pure-functional acquisition kernel (elastic fleet)](acq-fn.md) — pure-functional acquisition kernel for the elastic fleet
- [One correlator: the step-3 design](correlator.md) — step 3 of the correlator consolidation (#2752): the seven correlation objects become one C object, correlation/correlator/, its layers and constructors, the duplications it removes, the one deliberate C break, the unknowns, and the sub-steps with their oracles and speed gates
- [The acquisition surface — the measurement record](acq-surface-measurements.md) — the dated runs behind the design page under the same section numbers: §18's prototypes of the one delay-Doppler map (slow time first, the code-rate ramp on a shared transform) and the bench-box baseline it must meet
- [The acquisition surface: the DSSS search engine](acq-surface.md) — the DSSS search engine's arithmetic as equations, from the de-rotated per-tile correlation to the CFAR gate and the peak list, then the architecture, what `pfa` means for a peak detector, the compute cost and what is open

## Tracking & synchronization

Loop filters, carrier and timing recovery, and the narrowband receivers built on them.

- [MPSK Receiver](mpsk.md) — streaming M-PSK receiver architecture and carrier-recovery design, the constellation primitive it reuses (§9, soft decisions included), and the record of the collapse that made the real face a view (§12)
- [Receiver Test Harness](rx-test.md) — inventory of the stimulus, measurement and gate layers a receiver measurement rests on, and where they do not yet meet
- [Symbol Timing on a Rate Cascade](ratesync-timing.md) — RateSync: why the matched filter and the interpolator are one dot product, why `ctrl` is referenced to the terminal stage's rate, and why the T/2 parity resolves itself
- [SymbolSync Timing Lock Detector](timing_lock_detector.md) — SymbolSync's Gardner/DTTL lock statistic and sizing formula
- [The Loop Filter](loop-filter.md) — the second-order PI every tracking loop in the library embeds by value: where its gains come from, the unit condition its `bn` promise depends on, and what it deliberately does not bound

## Automatic gain control (AGC)

A log-domain feedback AGC.

- [Automatic Gain Control](agc.md) — the log-domain level loop every receiver stands on: why the filter is in dB and the detector is not, why the loop must be total under any input, and what level alone cannot tell it

## Detection statistics

Pd, Pfa and threshold models, a lock detector and a sync-word finder.

- [Detection Sizing — the four laws behind one prefix](detection.md) — the four statistical laws behind one `det_` prefix: which one a statistic actually obeys, why non-coherent looks raise their own threshold, and the 41x an estimated noise reference costs
- [Lock Detection — the reasoning](lock-detect.md) — the sizing chain every lock detector shares, and the independence it assumes

## Spread spectrum (DSSS)

Despreading, polynomial-phase estimation and the DSSS receivers.

- [AsyncDsssReceiver and AsyncDsssPool — continuous DSSS, from spec to object](async-dsss-receiver.md) — continuous DSSS, from spec to object: the derived code period, the acquisition hand-off, the asynchronous despreader, the receiver's states, the pool's one search, what it costs and the status record
- [AsyncDsssReceiver and AsyncDsssPool — the measurement record](async-dsss-receiver-measurements.md) — the dated runs behind the design page under the same section numbers: the pool's lock, arrival and cost runs (§6, §9), the pinned bench and the search profile, and every step of the receiver's build (§12)
- [DsssBurstReceiver: the burst chain, composed in C](dsss-burst-receiver.md) — the burst chain composed in C: the three-stage `search → refine → demod` shape, why the hand-off needs a never-late epoch, and what a burst `DetectionEvent` must carry to stand alone
- [The Polynomial-Phase Estimator — the reasoning](ppe.md) — the feedforward frequency/chirp-rate estimate a burst gets exactly one of: why the search is two-dimensional and coherent, why the transform is 4x its input, and why the caller strips the modulation

## Channel coding

Convolutional, Reed-Solomon and interleaving codes.

- [Interleaving — spreading a burst across codewords](interleaving.md) — the permutation, the three ways to reason about it wrongly, and the measured gain: E to E×depth, and the bound that still bites
- [Reed-Solomon](reed-solomon.md) — the outer code as a description: the two offsets a textbook omits (`j0 != 1`, and a root stride that is not 1), why Chien iterates positions rather than field elements, and what a refusal is not
- [The FEC Receive Half](fec-receive.md) — the Viterbi, the node sync it needs first, and the lock detector's two error probabilities: why the code's transparency means polarity cannot be resolved by the decoder
- [The Viterbi Decoder](viterbi.md) — the CCSDS inner code decoded: the trellis in the encoder's own terms, the branch metric it inherits, and why 5·K traceback is 17 % above the floor

## Streaming

Publish/subscribe, push/pull and request/reply of IQ over NATS.

- [Streaming — one envelope, six roles, two planes](streaming.md) — the transport contract: the 64-byte `dp_header_t` envelope every role shares, how a frame too big for the broker is chunked and reassembled, the subjects and JetStream objects derived from an endpoint, and who owns which buffer

## Buffers

Lock-free ring buffers between a producer and a consumer.

- [The ring buffer — measurements and the record](ring-buffer-measurements.md) — the dated record: what the ring costs per sample, what the non-blocking surface adds, and the three premises that were wrong.
- [The ring buffer — one contiguous view of a stream](ring-buffer.md) — zero-copy, wrap-free, lock-free: what each promise means, the blocking and non-blocking surfaces, and the two streaming patterns.

## Telemetry

In-band probes that record loop internals.

- [Telemetry — zero-cost scalar taps for running pipelines](telemetry.md) — zero-cost scalar taps (loop stress, AGC gain) for running pipelines

## Stopping a run (interrupt)

The one flag every blocking wait consults.

- [Ending a wait — one contract for network, memory and disk](io-termination.md) — one contract for network, memory and disk: why "no data right now" is indistinguishable from "no data ever" on all three, the interrupt primitive that already exists and is misnamed, and the end-of-stream marker that does not

## Sample-format conversion (cvt)

Converters between float IQ and fixed-point integers.

- [Quantization Design](QUANTIZATION.md) — fixed-point pipeline design

## Utilities

Numeric helpers shared across the modules.

- [API taxonomy: the DSP building-block hierarchy and its naming axis](api-taxonomy.md) — the DSP building-block hierarchy and its naming axis, plus concrete rename proposals
- [State Serialization — the standard bytes interface](state-serialization.md) — the standard bytes interface for bit-exact checkpoint/resume
- [The Exponential Moving Average](ema.md) — the running average every estimator in the library is built on: which of its two algebraic forms is the right one and why that was measured, what its boundaries guarantee, and why an EMA is not a loop filter

<!-- domain-index:end -->
