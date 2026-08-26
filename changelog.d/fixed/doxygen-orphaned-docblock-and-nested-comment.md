- **Two Doxygen defects that made the zero-warning gate red, and a ceiling
    that would have made it red again.** All three surfaced together on the
    detection-certification branch, and none of them is a documentation
    typo — each is a comment that says something other than what it means.

    - **An orphaned doc comment attached itself to the next function.** When
        the FFT-bin fold moved from `acq_core.h` into `clib_common.h` as
        `dp_fftfreq_index()`, its `/** … */` block stayed behind with nothing
        to document. Doxygen does not drop such a block; it binds it to the
        *following* declaration — so `acq_build_handoff()` was rendered with
        two `@param`s it does not take and a `@return` while returning
        `void`, nine warnings' worth. The block is now the plain pointer
        comment it should have been, and the one definition lives where the
        code does.
    - **`objects/*.toml` inside a block comment opens a nested comment.**
        The path contains the literal characters `/` `*`. A C compiler
        shrugs; Doxygen reads it as a second comment opening and runs to end
        of file looking for its close, reporting the failure 940 lines below
        the cause. Rephrased.
    - **`DOT_GRAPH_MAX_NODES` was a count of our own modules.** Two headers
        have a graph whose node count *is* the module count — `clib_common.h`
        is included by every module, `doppler.h` includes every module — so
        any finite ceiling on those graphs is a ceiling on how many modules
        doppler may have, crossed by growth rather than by a defect. It had
        already been raised 50 → 100 once; this branch crossed it again at
        103\. Set to 10000, the largest value Doxygen accepts, so no growth
        of this library can reach it. Fixed in `Doxyfile.base` upstream too,
        so the next project copying it does not inherit the trap.
