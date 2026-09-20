- **A version-site file states the version once, and lint holds it.** A
    comment in `CMakeLists.txt` spelled the SONAME chain with a literal
    version, so `ci-changes` classified the v0.53.0 release PR as
    `src=true` and ran the whole matrix — and would have at every release.
    `make lint-version-literals` checks it per PR, from the one site table.
