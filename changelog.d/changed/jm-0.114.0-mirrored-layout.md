- **just-makeit pin 0.112.0 → 0.114.0: each object's C test and benchmark
    now sits in a directory named for it.** `layout = "mirrored"` moved 311
    files to `native/tests/<obj>/` and `native/benchmarks/<obj>/`, beside
    `native/inc/doppler/<obj>/`. Generated methods also raise
    `RuntimeError("destroyed")` after `close()` instead of reaching C.
