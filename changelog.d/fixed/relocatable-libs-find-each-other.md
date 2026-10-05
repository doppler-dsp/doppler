- **A pub/sub-only program built against a relocated install now starts.**
    `libdoppler_stream` needs `libdoppler`, but neither said where to look, so a
    program calling only `dp_pub_*` died with `libdoppler.so.0.62: cannot open   shared object file` on Debian and Ubuntu, whose gcc drops the `-ldoppler`
    it never calls. The shared stream library now carries `$ORIGIN`
    (`@loader_path` on macOS): it finds its sibling wherever the tree was
    extracted. Inert for `apt`/`dnf` installs.
