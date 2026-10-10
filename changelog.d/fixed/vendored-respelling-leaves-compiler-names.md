- **Respelling the vendored symbols leaves the compiler's own names
    alone.** `cmake/prefix_vendored.cmake` renamed every global a vendored
    member defined, including clang ASan's `___asan_globals_registered`, in
    every member of `libdoppler.a`. A program linking its own instrumented
    object beside the archive then registered each global twice, which ASan
    reports as an ODR violation. Names reserved to the implementation
    (`__*`, `_[A-Z]*`) are now skipped (#2130).
