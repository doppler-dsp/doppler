# Rust FFI

`ffi/rust/` provides idiomatic Rust wrappers around the C library.
All DSP logic runs in C; the crate is pure glue.

!!! note "Prerequisites"

    - Rust toolchain — [rustup](https://rustup.rs/)
    - C library built first: `make build`

## Build and test

```sh
--8<-- "tests/install/rust-test.sh:test"
```

## Examples

```sh
--8<-- "tests/install/rust-test.sh:examples"
```

!!! tip "rpath on Linux/macOS"

    `build.rs` bakes an rpath into each binary so they run without
    setting `LD_LIBRARY_PATH`.

## Modules

| Module | C functions wrapped                   |
| ------ | ------------------------------------- |
| `acc`  | `acc_f32_*`, `acc_cf64_*`             |
| `fft`  | `fft_create`, `fft_execute_cf32/cf64` |
| `fir`  | `fir_create`, `fir_execute`           |
| `lo`   | `lo_create`, `lo_execute_cf32`        |
| `nco`  | `nco_create`, `nco_steps_*`           |

## Using from another crate

```toml
[dependencies]
doppler = { path = "path/to/doppler/ffi/rust" }
```

## Sample types

| Rust type | C layout          | Description                               |
| --------- | ----------------- | ----------------------------------------- |
| `DpCf32`  | `float _Complex`  | `{i: f32, q: f32}` — `From<Complex<f32>>` |
| `DpCf64`  | `double _Complex` | `{i: f64, q: f64}` — `From<Complex<f64>>` |
| `DpCi8`   | `{int8_t i, q}`   | Complex i8 IQ pair                        |
| `DpCi16`  | `{int16_t i, q}`  | Complex i16 IQ pair                       |
| `DpCi32`  | `{int32_t i, q}`  | Complex i32 IQ pair                       |
