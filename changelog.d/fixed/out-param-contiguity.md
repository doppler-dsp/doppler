- **A non-contiguous `out=` is refused instead of silently ignored.** On 19
    methods across 17 objects — `AGC.steps`, every `cvt` converter,
    `DelayCf64.ptr` / `push_ptr`, `LockDet.steps`, `MovingAverage.steps`,
    `Resampler.execute` / `execute_ctrl`, `Farrow.delay` — a strided `out=`
    was copied, the copy was filled, and the caller's array was never
    written; a fresh array came back and nothing raised. Now `TypeError`,
    like a wrong dtype, with a gate that reads every binding
    ([#1440](https://github.com/doppler-dsp/doppler/issues/1440)).
