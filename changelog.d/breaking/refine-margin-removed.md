- **`refine_margin` is removed** from `BurstCapture`, `PersistentBurstCapture`
    and `DsssBurstReceiver` — property, event field and C accessors. Nothing
    branched on it, and no threshold was portable: its floor is
    `(reps-1)/reps`, so it rose with depth. Both state blobs change shape
    (`BURST_CAPTURE_STATE_VERSION` 3, `DSSS_BURST_RECEIVER_STATE_VERSION` 7);
    an older blob is refused at the envelope. Closes
    [#1310](https://github.com/doppler-dsp/doppler/issues/1310).
