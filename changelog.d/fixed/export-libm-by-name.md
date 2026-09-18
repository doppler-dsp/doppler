- **The C tarball links again for CMake consumers of the static library on
    Debian and Ubuntu.** v0.51.0 exported the build container's
    `/usr/lib64/libm.so` into `doppler-targets.cmake`; it exports `m` again,
    and a new gate (`make exported-link-check`, in CI and before every
    tarball) refuses an absolute path in any exported link interface. The
    post-release PyPI smoke also stopped racing PyPI's CDN: it now waits
    until `uv` itself can resolve the new version.
