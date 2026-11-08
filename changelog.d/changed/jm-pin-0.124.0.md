- **just-makeit pin 0.123.0 → 0.124.0.** "Create with defaults"
    examples now call the constructor bare instead of restating every
    default (jm#2556), across 11 `.pyi` stubs and their runtime
    docstrings. The pin also brings `create_why`, `getter_error` and
    `error_sentinel`, which #2752 step 4's `Correlator` needs.
