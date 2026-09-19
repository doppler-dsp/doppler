- **Every release now publishes the C library for Windows:**
    `doppler-<version>-windows-x86_64.zip`, built with clang-cl against the
    MSVC runtime. The clang-cl build is now binding CI, and the same zip is
    packaged and consumed through `find_package` on every PR. See
    [Install the C library → Windows](docs/install/c.md#windows).
