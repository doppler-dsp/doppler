- **`make release-pr` no longer dies halfway through the version bump on
    a box with no Rust crate cache** (#2722). `cargo fetch --locked` now
    runs before any file is written, so a missing cache fails with the
    tree untouched instead of leaving a half-done bump to back out.
