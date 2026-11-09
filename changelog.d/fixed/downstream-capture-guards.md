- **The downstream example's `Capture`/`RawCapture` bindings lacked jm's
    output-size guards.** Their sacred `_ext_<obj>.c` fragments fell behind
    jm's render: `read(-1)` asked for SIZE_MAX samples and wrapped it to a
    negative dimension, and a kernel returning more than its buffer held
    was shaped past the allocation. Both are now `fragment = "generated"`,
    so `read(-1)` raises `OverflowError` (pinned by a new test). The
    re-render also makes `summary(1)` raise `TypeError` (it took and ignored
    arguments) and names the types `iqtools.capture.Capture`/`RawCapture`
    (was `capture.*`).
