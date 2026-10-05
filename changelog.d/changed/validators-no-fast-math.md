- **Clang no longer warns three times on every build, and the warnings gate
    now fails on a driver warning** (#1839). Three validation harnesses
    that pin `-ffp-contract=off` now turn fast-math off first, so clang 23
    has no override to report. Their tables are unchanged under clang and
    under gcc but for one last digit in `dll_jitter`, which now matches
    clang's output.
