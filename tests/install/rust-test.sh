#!/usr/bin/env bash
set -euo pipefail

# --8<-- [start:test]
make rust-test
# --8<-- [end:test]

# --8<-- [start:examples]
cargo run --manifest-path ffi/rust/Cargo.toml --example fft_demo
cargo run --manifest-path ffi/rust/Cargo.toml --example nco_demo
cargo run --manifest-path ffi/rust/Cargo.toml --example acc_demo
# --8<-- [end:examples]
