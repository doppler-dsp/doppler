- **just-makeit pin 0.108.0 → 0.109.0: an object that lends its pointer as
    a capsule no longer frees it under a consumer that kept it.** `Telemetry`,
    `Frame`, `FrameDesc`, `SampleClock` and the `Composer` park their state at
    `close()`/`destroy()` and free it with their last holder, so a
    `set_telemetry` consumer reading after its producer closed now reads live
    memory instead of freed memory (jm gh-2346). The pin also brings the
    `json` property type behind `Reader.annotations`, and
    `composer_field_keys`, which ends ~20 lines of expected warnings per
    `jm apply` for the keys wfmgen's generator reads.
