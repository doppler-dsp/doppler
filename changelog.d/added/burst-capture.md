- **`BurstCapture` — the look-back and the refine get a home.** Acquisition's
    `code_phase` is a lag MODULO one code period, so it never says WHICH
    preamble repetition a burst began in. Resolving that, and reaching back to
    a start already gone past, existed once — inside `DsssBurstReceiver`. The
    new object searches, refines, retains and emits the burst's SAMPLES, for a
    recorder, a corpus, or a second consumer.
    Design: [`burst-capture.md`](docs/design/burst-capture.md).
    Refs [#1166](https://github.com/doppler-dsp/doppler/issues/1166).
- **`PersistentBurstCapture` — the look-back in a file.** The ring's pages are
    a `MAP_SHARED` mapping of a path, so the samples ARE the file's contents:
    no mirror buffer, no flusher, no second copy. The checkpoint stops carrying
    the history — 16.68 MB → 21.6 kB at an 8029-symbol frame — and the
    look-back outlives the process, so a restored capture reaches back across
    a restart into a burst that began before it.
    See [`burst-capture.md` §9](docs/design/burst-capture.md).
- **The search under a capture is visible, and an unmeetable `pd` says so.**
    `BurstCapture` forwarded one of the engine's 27 read-backs and hardcoded
    the CFAR mode. It now carries the numbers that decide what gets captured —
    `doppler_bins`, `n_noncoh`, `code_bins`, `doppler_span_hz`, both detection
    gates and `straddle_loss` — plus `noise_mode` as a choice and
    `underpowered` as a **declared** warning, which the sibling
    `BurstAcquisition` can only hand-patch. Required configuration is still
    one parameter: `BurstCapture(code)`.
