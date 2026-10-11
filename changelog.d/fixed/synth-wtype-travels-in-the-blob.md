- **`set_wtype` now travels in the synth blob.** The waveform type was
    restored from the target, not the blob, so a blob taken from one engine
    and restored into another of a different type read as the target's type.
    The type is now the first payload word (blob version 4), and a restore
    refuses a type the target cannot run, with the target unchanged (#2095).
