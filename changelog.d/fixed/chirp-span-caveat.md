- **A standalone chirp's sweep depends on how you read it** (#1115) — found
    certifying `Synth`, documented rather than fixed. The slope needs a span;
    `steps()` self-pins to its first block and `step()` never pins at all, so
    a chirp read sample-by-sample is a constant tone at `--freq` and one read
    in blocks sweeps within the first block and holds. The CLI and
    `Segment`/`Composer` pin the length up front and are unaffected. The
    waveform guide now warns where a Python caller meets it.
