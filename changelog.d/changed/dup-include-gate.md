- **`make lint` fails when a C file includes the same header twice in one
    branch.** `burst_capture_core.h` included `correlator_core.h` three
    times, and three other headers repeated an include too. The include
    guard hid every copy from the compiler. `make lint-dup-include`
    (`scripts/check_dup_includes.py`) flags a repeat at the same `#if`
    nesting path, so the same header in an `#if` and its `#else` still
    passes.
