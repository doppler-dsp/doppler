# Design

Architecture decisions, API contracts, and the algorithm rationale behind
shipped features — for anyone who wants the "why" and "how it works"
underneath a feature they're already using, not just contributors. (Repo
layout and forward-looking roadmaps live under [Contributing](../dev/index.md)
instead.)

- [API Taxonomy](api-taxonomy.md) — the DSP building-block hierarchy and its
    naming axis, plus concrete rename proposals
- [The NCO](nco.md) — the 32-bit phase accumulator every rate in the
    library stands on: why an integer register instead of a double, the
    one float boundary, and how carrier and timing read the same thing
- [Exponential Moving Average](ema.md) — the running average every estimator
    in the library is built on: which of its two algebraic forms is the right
    one and why that was measured, what its boundaries guarantee, and why an
    EMA is not a loop filter
- [The Loop Filter](loop-filter.md) — the second-order PI every tracking loop
    in the library embeds by value: where its gains come from, the unit
    condition its `bn` promise depends on, and what it deliberately does not
    bound
- [Quantization](QUANTIZATION.md) — fixed-point pipeline design
- [CIC Decimator](cic.md) — the fixed-point input budget: DC gain and PAPR headroom
- [Measurement Suite](measurement-suite.md) — tone, NPR, and IMD metric internals
- [State Serialization](state-serialization.md) — the standard bytes interface for bit-exact checkpoint/resume
- [Telemetry](telemetry.md) — zero-cost scalar taps (loop stress, AGC gain) for running pipelines
- [Streaming](streaming.md) — the transport contract: the 96-byte SIGS
    envelope every role shares, how a frame too big for the broker is
    chunked and reassembled, the subjects and JetStream objects derived
    from an endpoint, and who owns which buffer
- [Ending a Wait](io-termination.md) — one contract for network,
    memory and disk: why "no data right now" is indistinguishable from
    "no data ever" on all three, the interrupt primitive that already
    exists and is misnamed, and the end-of-stream marker that does not
- [Capture Files](capture-files.md) — the file I/O subsystem underneath
    `Reader`/`Writer`: why the file type is decided by content, why one
    keyword codec serves both directions, why provenance is a value rather
    than a caveat, and what a streaming writer costs at close
- [Ending a Capture](end-of-capture.md) — the disk third of that
    contract: spooling an endless stream to disk while reading it back,
    why Ctrl+C must reach the reader through the file rather than around
    it, and what owning both ends of the file buys
- [The Polynomial-Phase Estimator](ppe.md) — the feedforward frequency/chirp-rate estimate a burst gets exactly one of: why the search is two-dimensional and coherent, why the transform is 4x its input, and why the caller strips the modulation
- [DSSS Acquisition](dsss-acquisition.md) — stateless, parallel, dynamics-capable acquisition architecture + roadmap
- [AsyncDsssReceiver](async-dsss-receiver.md) — the continuous DSSS receiver from spec to object: the target waveform (CCSDS Gold-1023, 3.069 Mcps, 2700 bps, ±50 kHz, \<500 Hz/s), the two acquisition front doors and the `DetectionEvent` hand-off, the asynchronous despreader and its look-back working design, the receiver as built, and what it must gain for the multi-emitter use case
- [DsssBurstReceiver](dsss-burst-receiver.md) — the burst chain composed in C: the three-stage `search → refine → demod` shape, why the hand-off needs a never-late epoch, and what a burst `DetectionEvent` must carry to stand alone
- [BurstCapture](burst-capture.md) — acquisition's output turned into aligned burst samples: why the period-resolving refine and the look-back need a home outside the receiver, why the object owns its own engine rather than taking foreign detections, and the one behaviour that changes
- [BurstBank](burst-bank.md) — the coarse-Doppler bank as one C object: why its cross-channel claim rule belongs beside the capture's, the layout rule now that the engine searches to its edge, rings named by centre so a pod can hold one channel, and the unknowns the characterization measures
- [CoarseChannel](coarse-channel.md) — question 1 of the bank design: is a channel its own object or a slice of the bank, judged against the two primary use cases (one process runs the whole bank; one pod runs one channel), with the trades and the work that answers it
- [Multi-peak acquisition](acq-multi-peak.md) — every emitter on one surface: why a single maximum per dwell hides every emitter but the strongest for as long as it is up, the peak list with exclusion zones and the cancellation the Gold code's cross-correlation floor forces beyond it, where each piece lives, the lock-detector release that ends an assignment, and the work that picks the branch
- [Automatic Gain Control](agc.md) — the log-domain level loop every receiver
    stands on: why the filter is in dB and the detector is not, why the loop
    must be total under any input, and what level alone cannot tell it
- [Detection Sizing](detection.md) — the four statistical laws behind one `det_` prefix: which one a statistic actually obeys, why non-coherent looks raise their own threshold, and the 41x an estimated noise reference costs
- [Lock Detection](lock-detect.md) — the sizing chain every lock detector shares, and the independence it assumes
- [Timing Lock Detector](timing_lock_detector.md) — SymbolSync's Gardner/DTTL lock statistic and sizing formula
- [Symbol Timing on a Rate Cascade](ratesync-timing.md) — RateSync: why the matched filter and the interpolator are one dot product, why `ctrl` is referenced to the terminal stage's rate, and why the T/2 parity resolves itself
- [MPSK Receiver](mpsk.md) — streaming M-PSK receiver architecture and carrier-recovery design, the constellation primitive it reuses (§9, soft decisions included), and the record of the collapse that made the real face a view (§12)
- [The Viterbi Decoder](viterbi.md) — the CCSDS inner code decoded: the trellis in the encoder's own terms, the branch metric it inherits, and why 5·K traceback is 33 % above the floor
- [Reed-Solomon](reed-solomon.md) — the outer code as a description: the two offsets a textbook omits (`j0 != 1`, and a root stride that is not 1), why Chien iterates positions rather than field elements, and what a refusal is not
- [The FEC Receive Half](fec-receive.md) — the Viterbi, the node sync it needs first, and the lock detector's two error probabilities: why the code's transparency means polarity cannot be resolved by the decoder
- [Interleaving — spreading a burst across codewords](interleaving.md) — the
    permutation, the three ways to reason about it wrongly, and the measured
    gain: E to E×depth, and the bound that still bites
- [A Frame as a Description](frame-description.md) — a frame as a list of
    fields and a list of stages, each stage carrying the span it covers: why
    a chain of optional transforms is the representation that cannot express
    CCSDS, and why the standard is the first configuration rather than the
    subject
- [Receiver Test Harness](rx-test.md) — inventory of the stimulus, measurement and gate layers a receiver measurement rests on, and where they do not yet meet
- [Corr2D Interpolated Inverse](corr2d-interpolated-inverse.md) — decoupled, pffft-friendly inverse FFT size + free sub-bin interpolation
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [wfmgen — the waveform generator](wfmgen.md) — the spine for the tool:
    modulations, impairments and output streams from three interchangeable
    faces, what it promises and which gate keeps each promise, the
    seventeen primitives it composes rather than re-implements, and the
    eight things still unknown about it — including that it states no
    throughput and no certified envelope
- [Waveform Amplitude & Composition](wfmgen-composition.md) — level/power conventions for wfmgen
- [One Home for the Waveform Enum Tables](waveform-enum-ssot.md) — a name
    table's ORDER is the C enum value, so a second copy mis-maps a flag
    instead of failing: what owns the tables, and the gate that holds the
    manifest, the header and the C enums to each other
- [Continuously Variable Resampler](RESAMPLER.md) — polyphase resampler architecture, testing, and performance optimizations
- [Acquisition Kernel](acq-fn.md) — pure-functional acquisition kernel for the elastic fleet
