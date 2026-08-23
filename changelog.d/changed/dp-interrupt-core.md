- **The interrupt primitive moved to the core library** as
    `native/inc/dp_interrupt.h` — `dp_interrupt()`, `dp_interrupted()`,
    `dp_resume()`, `dp_interrupt_on_signal()`, `dp_restore_signal()` and the
    latency accessors.

    It was in `stream_core.c`, which lives in the optional
    `libdoppler_stream`, so a build with no NATS could not link it — and two
    of its three users are core (a file writer, a ring buffer). Nothing had
    to be rewritten to move it: it never had a NATS dependency, being a
    `volatile sig_atomic_t` and four accessors over libc. Its first
    non-stream caller is the sample clock's pacing sleep.

    The `dp_stream_*` spellings still work, forwarding verbatim, and are
    **deprecated**: a general primitive should not carry the name of one of
    its three users. They are removed once their call sites migrate.

    One definition only — verified with `nm`: `libdoppler.so` exports it and
    `libdoppler_stream.so` has zero copies, which is what a process-wide flag
    requires.
