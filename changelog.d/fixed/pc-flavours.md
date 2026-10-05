- **The pkg-config files come in the flavour their install needs.** A package
    install (`apt install`, `dnf install`; prefix `/usr`) gets the literal
    `/usr`, so pkg-config drops the system `-I/usr/include` and `-L` instead of
    putting them on every consumer's command line. Any other prefix (the
    release tarball, `jbx get-doppler`, `--prefix ~/.local`) keeps a prefix
    derived from the file's own location, so a plain `tar xf` still works.
    Chosen at install time (`cmake/install_pc.cmake`).
