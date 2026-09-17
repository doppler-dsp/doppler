- **Windows builds via clang-cl, behind an exploratory runner.**
    `native/inc/dp_complex.h` replaces `<complex.h>` in the hand-written C — a
    passthrough on Linux/macOS, and on Windows the same names from clang
    builtins plus real-valued libm, so no `_Fcomplex` crosses the ABI. The new
    `windows.yml` builds the C suite on `windows-latest`; it is not in
    `ci-passed`'s `needs` and publishes nothing. Rationale and the measured
    symbol set are in the header's file comment.
