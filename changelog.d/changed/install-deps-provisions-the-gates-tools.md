- **`make install-deps` provisions the dev tools the gates use.** A new
    `clang-rt` group installs clang's compiler runtime (the ASan and
    coverage-profile libraries) on apt, pacman and dnf, deriving apt's
    versioned name from the clang it finds, so `make test-asan` works on a
    fresh Debian 13 box. `[dev.brew]` gains `coreutils` (GNU `gdate`), and the
    `docs` group now lists doxygen and graphviz for pacman, brew, dnf and
    zypper, not just apt. The CI image reads the same declarations.
