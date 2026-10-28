- **`DelayDoppler`: the delay-Doppler map from raw samples.** A view on
    `Corr2D` that takes `ny` code periods back to back and returns the
    centered `ny_out x nx` map, with the slow-time FFT done in C and
    `ny_out` interpolating the Doppler axis. At `(256, 4096)` it took 9.0 ms
    on one thread and 1.5 ms on 20, against 56 ms for `Corr2D` plus numpy.
    `Corr2D`'s `nthreads` now works on its single-row fast path, with output
    bit-identical across thread counts (#2666).
