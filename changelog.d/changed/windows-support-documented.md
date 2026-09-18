- **Windows is documented as a supported build platform for the C library.**
    [Build from Source → Windows](https://doppler-dsp.github.io/doppler/install/source/#windows)
    now gives the clang / clang-cl recipe CI runs and lists what the Windows
    build leaves out (NATS streaming, the `wfmgen` CLI, Python, Rust),
    replacing "doppler does not target Windows" there and in eleven other
    places. The retired-names gate now refuses that claim, and it scans
    `.github/`, which a prefix bug had silently skipped.
