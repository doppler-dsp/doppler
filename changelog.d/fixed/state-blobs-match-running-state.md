- **State blobs match the running state: CIC carries R, MpskReceiver its
    centre, and nothing stale is serialized.** `CIC`'s blob carries R, so a
    restore follows `reconfigure()`, and `RateConverter`'s carries its rate
    as a reject key, so a blob from another rate is refused rather than run
    behind the target's; `MpskReceiver`'s carries the centre
    `norm_freq` sets, retuning the LO on restore. `hbdecim_r2c` and
    `hbdecim_q15` zero a dropped held sample, `CorrDetector`/`CorrDetector2D`
    reset clears the last dump, and `DsssBurstReceiver.set_state` clears
    `frame_valid`. Same logical state, same blob (#2642).
