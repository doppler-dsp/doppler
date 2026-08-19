- **The coverage run no longer borrows the developer's machine.** With its
    exit code finally being read, the job reported 44 failures and 6 errors —
    none about the code under test. Three gates asked for artifacts in
    `build/`, which this job never builds (it builds `build-cov`), and four
    asked for console scripts on `PATH`:

    | asked for                                            | who                               | why it passed locally                   |
    | ---------------------------------------------------- | --------------------------------- | --------------------------------------- |
    | `build/libdoppler.a`                                 | the C doc-snippet gate, 33 blocks | an ordinary build tree is sitting there |
    | `build/native/validation/validate_{conv,rs}_certify` | the conv/rs certify harnesses     | same                                    |
    | `wfmgen` on `PATH`                                   | 9 sh doc fences                   | an activated venv                       |
    | `doppler-source` / `doppler-fir` / `doppler-specan`  | the cli block tests               | same                                    |

    The build tree is now one derivation, `doppler.tests._repo.build_dir()`,
    reading `$DOPPLER_BUILD_DIR` and falling back to `<repo>/build`. That
    variable is not new — `ffi/rust/build.rs` has always read it and the
    coverage recipe already exported it for the cargo leg; the Python gates
    each spelled `build/` themselves instead. The recipe now exports it for
    the pytest leg too, and prepends the instrumented `wfmgen` and the venv's
    `bin` to `PATH`.

    The C snippets are **run instrumented** rather than excluded: a snippet
    cannot link against a clang source-based archive without
    `-fprofile-instr-generate -fcoverage-mapping`, so the gate adds them (and
    prefers clang) when `LLVM_PROFILE_FILE` says the run is instrumented.
    Each snippet then writes its own `.profraw` into the directory the recipe
    merges, so 33 documented C examples contribute to the number instead of
    being a hole in it.

    This is the same class as the two exit-code discards on either side of
    it: what a gate reads from its environment has to come from the run, not
    from whoever happens to be running it.
