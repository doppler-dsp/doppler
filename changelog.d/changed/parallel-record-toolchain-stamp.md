- **The parallel-receivers record stamps its toolchain.** Each run's
    stamp carries the compiler CMake detected and its version, the C
    library, the build type with the flags it compiles with, and whether
    `DOPPLER_NATIVE` added `-march=native`. A re-run on another box then
    reads as a different toolchain, not as drift, and a schema-2 stamp
    missing any of these is refused. The record's seed-edge block renders
    the numbers its prose cites from the data.
