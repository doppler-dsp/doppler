- **The downstream example's `Capture`/`RawCapture` bindings lacked jm's
    output-size guards.** Their `_ext_<obj>.c` fragments were sacred, so
    `jm apply` only warned that they no longer matched the manifest while
    jm's render moved on: `read(-1)` asked for SIZE_MAX samples and wrapped
    it to a negative dimension, and a kernel returning more than its buffer
    held was shaped past the allocation. Both fragments are now
    `fragment = "generated"`, so jm owns them and renders the guards;
    `read(-1)` raises `OverflowError`, pinned by a new test in the example's
    suite.
