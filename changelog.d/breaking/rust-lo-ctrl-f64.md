- **`Lo::steps_ctrl` read twice its buffer from safe Rust**
    ([#911](https://github.com/doppler-dsp/doppler/issues/911)). The extern
    declared the control port `*const f32`; the C takes `const double *`, so
    C walked `8 * ctrl_len` bytes off a `4 * ctrl_len`-byte allocation, with
    the values reinterpreted regardless. **Breaking**: `steps_ctrl` now takes
    `&[f64]`. `examples/nco_demo.rs` was calling it with a *non-zero*
    `Vec<f32>`, so it over-read on every run.

    The existing test passed throughout because it used `vec![0.0_f32; 4]` —
    all-zero is the one input whose bit pattern is identical at both widths.
    The new test uses a real deviation and asserts the measured phase advance;
    against the original declarations it reports the deviation as **entirely
    ignored** (0.0999… where 0.15 was expected), since `0.05_f32` read as
    `f64` is a denormal.

    `ffi/rust/` is the one binding jm does not generate, so nothing compared
    it to the C — and this had happened once before (`cce1792f`). New
    `scripts/check_rust_abi.py` (`make lint-rust-abi`, and a pre-commit hook)
    checks every `extern "C"` declaration against the header: the name exists,
    the arity matches, and each parameter's element width agrees. 45
    declarations check clean; three sabotages caught.
