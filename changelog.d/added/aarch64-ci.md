- **The C suite runs on Linux aarch64 in CI** (`Build on ubuntu-24.04-arm`,
    binding via `CI passed`). The aarch64 wheels were already published
    from that runner; their tests had never run there (#1561). Three
    exactness tests that failed only at the ULP level under `-ffast-math`
    now assert a tolerance.
