- **`make coverage` completes on a many-core box.** It ran the timing and
    two-process examples under full xdist, so on 20 cores the run died before
    `llvm-cov` and wrote no report at all. They now get the example gate's
    serial pass. ([#1376](https://github.com/doppler-dsp/doppler/issues/1376))
