- **An acquisition channel can capture bursts, not just detect them.**
    `CoarseChannel` builds a `BurstCapture` when given a `burst_len` (and a
    `PersistentBurstCapture` when given a ring path), so a bank reports the
    same detections *and* the aligned burst windows. `BurstCapture.detections()`
    is what makes one object enough: without it a bank wanting both would push
    every sample through two acquisition engines.
    Refs [#1174](https://github.com/doppler-dsp/doppler/issues/1174).
