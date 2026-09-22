- **just-makeit pin 0.85.0 → 0.86.0, and `wfm_sink` declares its
    platforms.** `[module.wfm_sink] platforms = ["linux", "macos"]` (jm
    gh-1463): on Windows the extension's CMake no longer names
    `stream_core_obj`, which stopped the whole configure, and
    `doppler.wfm` imports without `StreamSink` rather than failing on it.
    `doppler.wfm.compose` re-exports `StreamSink` only where it was built.
    Nothing changes on Linux or macOS.
