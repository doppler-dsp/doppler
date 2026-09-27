- **More of doppler builds on Windows**: `dp_doppler_wfmgen` is in
    `doppler.dll`, and the `wfmgen` CLI, the streaming and threaded C
    examples, and the timing test and bench build under clang-cl. The
    examples use `dp_thread.h` and `dp_thread_sleep_us` instead of pthreads
    and three private sleep macros. `doppler.stream`'s Python tests and
    specan's NATS-source tests now run on Windows (they were uncollected
    there since before #1575 made the extension build).
