- **State blobs match the running state: CIC carries R, MpskReceiver its
    centre, and nothing stale is serialized.** `CIC`'s blob carries R, so a
    restore follows `reconfigure()`; `MpskReceiver`'s carries the centre
    `norm_freq` sets, retuning the LO on restore. `hbdecim_r2c` and
    `hbdecim_q15` zero a dropped held sample, `CorrDetector`/`CorrDetector2D`
    reset clears the last dump, and `DsssBurstReceiver.set_state` clears
    `frame_valid`. Same logical state, same blob (#2642).
