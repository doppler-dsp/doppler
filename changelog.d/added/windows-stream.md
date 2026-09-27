- **The NATS stream layer builds on Windows**
    ([#1575](https://github.com/doppler-dsp/doppler/issues/1575)):
    `libdoppler_stream` (the `dp_pub_*`/`dp_sub_*` wire layer and the wfm
    stream sink), `doppler.stream` and `doppler.wfm.StreamSink` now build
    under clang-cl with the vendored nats.c's own Windows port, and the wire
    format test runs there (closes
    [#1361](https://github.com/doppler-dsp/doppler/issues/1361)). The stream
    tests and benchmark use doppler's portable thread and clock primitives
    instead of pthreads, BSD sockets and POSIX semaphores.
