- **just-makeit pin 0.76.4 → 0.77.2.** Windows is clang-cl with no flag
    (MinGW retired, so doppler's `platforms` key is gone), every generated
    component gains a `test_<obj>_symbols.c` that fails at link time when a
    binding calls a function nothing defines, and each generated directory
    gains an `<dir>_extra.cmake` hook. doppler drove three fixes in it:
    jm#1381 (a composer's `ranged` key refused), jm#1382 (`upgrade`
    respelling prose) and the Windows DLL export defect it shares with
    [#1397](https://github.com/doppler-dsp/doppler/pull/1397).
