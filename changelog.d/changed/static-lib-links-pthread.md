- **`libdoppler.a` now declares `-lpthread`, and a static consumer needs it
    on the link line.** The core has needed pthread since `rs.c` moved to
    `pthread_once`, but nothing said so: each component that needs pthread
    carries `Threads::Threads` PUBLIC on its own target, and every component
    is folded into the archive as `$<TARGET_OBJECTS:...>` — objects, not a
    link edge — which drops the usage requirement on the floor. So the
    archive's link interface named only `-lm` while one of its members called
    `pthread_once`.

    Nothing failed on a modern box, because glibc ≥ 2.34 folds pthread into
    libc. On glibc < 2.34 it is a separate library and the symbol has to be
    named: `examples/c/ccsds_link_demo` failed to link in the Debian 10 job
    with `undefined reference to pthread_once`.

    `Threads::Threads` is now PUBLIC on `doppler_lib_static`, so
    `doppler::doppler-static` and `pkg-config --static doppler` both carry it
    and a `find_package`/pkg-config consumer needs no change. A consumer that
    spells the link line by hand should add `-lpthread` beside `-lm`.
