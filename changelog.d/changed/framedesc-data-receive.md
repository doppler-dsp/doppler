- **A `FrameDesc` with a data field now builds, and its receive face works.**
    `build()` lays the description out from its lengths and proves its stages
    runnable; `bits()` has no single frame and returns none; `deframe()` and
    `check()` read the layout instead of returning zeros
    ([#1789](https://github.com/doppler-dsp/doppler/issues/1789)).
