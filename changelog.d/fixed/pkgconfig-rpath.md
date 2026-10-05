- **The C Quick Start's pkg-config recipe runs.** It linked and then failed with
    `error while loading shared libraries: libdoppler_stream.so.0.62` from an
    extracted tarball, whose prefix is on nobody's loader path. The recipe now
    says where the library is, once (`-Wl,-rpath,$(pkg-config --variable=libdir   doppler_stream)`), and the install pages say when it is needed. The `.pc`
    deliberately carries no rpath: a cross build would embed the build host's
    sysroot, and the consumer could not remove it.
