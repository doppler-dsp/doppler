- **The install pages' examples paste and run.** The by-hand tarball recipe no
    longer scrapes the GitHub API with `sed`; the Windows, vcpkg, `.deb`/`.rpm`
    and docker examples no longer say `X.Y.Z` or a file glob. Each carries the
    current release's real version and file names, stamped at release time by
    `make docs-relink` (which now also stamps the Debian runtime package's
    series, `libdoppler-dsp0.62`). The C Quick Start says how to get `jbx`, and
    the Python Quick Start says it is Python, sends C readers to the C one, and
    shows how to create the scene file `wfmgen --from-file` reads.
