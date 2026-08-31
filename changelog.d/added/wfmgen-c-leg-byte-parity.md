- **wfmgen's C API is now pinned against the CLI and Python, byte-for-byte.**
    The design doc promises one scene renders identically from all four APIs
    and calls C the primary one, but only Python↔CLI and JSON↔CLI were
    tested; what stood in for the C leg composed a scene twice in one process,
    which is determinism, not cross-API agreement.
    `native/validation/wfmgen_certify.c` renders through the struct API for
    `test_c_api_byte_parity_vs_wfmgen` to compare. All three legs agree.
- **The wfmgen byte-parity tests can no longer skip themselves.** They used a
    private copy of the CLI locator that returned `None` into a `skipif`, so
    the only evidence for that promise could report green having run nothing;
    they now use the shared `cli._runnable()`, which fails with the path.
