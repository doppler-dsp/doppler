- **A binary built from `pkg-config --libs doppler` (or `doppler_stream`) now
    runs.** `doppler.pc` named `-L${libdir}` but no rpath, so the program
    linked and then failed with `error while loading shared libraries:   libdoppler_stream.so.0.62` unless the prefix was on `LD_LIBRARY_PATH`,
    which an extracted release tarball (`jbx get-doppler`) never is. `Libs`
    now carries `-Wl,-rpath,${libdir}` (not on Windows), and `doppler_stream`
    inherits it through `Requires`. The C Quick Start's three-ways script now
    runs every binary with a cleared loader path, so the install is what is
    tested, not the shell around it.
