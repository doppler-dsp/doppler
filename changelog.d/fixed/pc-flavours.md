- **The pkg-config files now come in the flavour their install needs.** A
    package install (`apt install`, `dnf install`; prefix `/usr`) gets the
    conventional file: the literal `/usr`, so pkg-config drops the system
    `-I/usr/include` and `-L` instead of putting them on every consumer's
    command line, and no rpath, because `ldconfig` finds the library. A
    relocatable install (the release tarball, `jbx get-doppler`, `--prefix   ~/.local`) keeps the location-derived prefix and an rpath, so what it
    links, it runs. Chosen at install time (`cmake/install_pc.cmake`).
