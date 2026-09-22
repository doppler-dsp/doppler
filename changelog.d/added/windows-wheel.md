- **A Windows wheel.** `pip install doppler-dsp` now installs a pre-built
    `win_amd64` wheel for Python 3.9–3.14, built with clang-cl and repaired
    by delvewheel, and smoke-tested before the upload and again from PyPI
    after it. It leaves out the NATS stream layer, which is not ported yet
    (`doppler.stream`, `doppler.wfm.StreamSink`, the `wfmgen` command;
    [#1364](https://github.com/doppler-dsp/doppler/issues/1364)).
