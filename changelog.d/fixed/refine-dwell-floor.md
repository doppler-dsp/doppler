- **`AsyncDsssReceiver`'s refine dwell has a floor, `refine_min_blocks`
    (default 7), on the receiver and the pool**
    ([#1265](https://github.com/doppler-dsp/doppler/issues/1265)): the dwell
    was sized for detection alone and shrank to two blocks at 45 dB-Hz, where
    the estimate's 210 Hz noise put one hand-over in sixty outside the
    tracking chain's pull-in — a receiver tracking the code with its carrier
    never locked, then a second receiver on the same emitter. Seven blocks
    (42 ms) hold the estimate to 77 Hz; a bigger detection margin is the wrong
    lever (at 40 dB-Hz it sizes seconds, and the row's phase drifts out of the
    zone meanwhile). Design §12.16.
