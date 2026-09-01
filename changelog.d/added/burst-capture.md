- **`BurstCapture` — the look-back and the refine get a home.** Acquisition
    reports a code phase that is a lag MODULO one code period, so it names the
    alignment within a preamble repetition and never which one; resolving that
    and reaching back to a start already gone past existed exactly once, inside
    `DsssBurstReceiver`. A caller with a different composition — the shipped
    `dsss.orchestrator.Acquirer`, a recorder, an offline corpus — had to write
    it again or sweep acquisition in `reset()`-per-dwell dwells and hold the
    whole capture in RAM. The new object searches, refines, retains and emits
    the burst's SAMPLES, and stops there. It owns its acquisition engine rather
    than taking foreign results, because `samples_consumed` is stream-absolute
    only for an engine fed continuously and never reset — an invariant a taker
    would have to require and could not check.
    Design: [`burst-capture.md`](docs/design/burst-capture.md).
    Refs [#1166](https://github.com/doppler-dsp/doppler/issues/1166).
