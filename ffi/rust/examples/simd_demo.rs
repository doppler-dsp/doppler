//! simd_demo — Rust FFI example: SIMD complex multiplication via doppler.
//!
//! Calls dp_c16_mul (SSE2 on x86-64, scalar fallback elsewhere) through
//! the safe `doppler::c16_mul` wrapper and verifies results against
//! Rust's own Complex64 arithmetic.
//!
//! Run with:
//!   cargo run --example simd_demo

use num_complex::Complex64;
use doppler::c16_mul;

fn run_case(label: &str, a: Complex64, b: Complex64) {
    let result = c16_mul(a, b);
    let expect = a * b;
    let err = (result - expect).norm();
    let status = if err < 1e-10 { "OK" } else { "MISMATCH" };
    println!(
        "  {:<32}  result: ({:+.6}, {:+.6})  expect: ({:+.6}, {:+.6})  err: {:.2e}  {}",
        label, result.re, result.im, expect.re, expect.im, err, status
    );
}

fn main() {
    println!("=== doppler SIMD Demo: c16_mul (Rust FFI) ===");
    println!();
    println!("Library version: {}", doppler::version());
    println!();
    println!("Each row: doppler::c16_mul(a, b) vs. Rust Complex64 a * b.");
    println!();

    use std::f64::consts::PI;

    run_case("(1+2j) * (3+4j)",
             Complex64::new(1.0, 2.0), Complex64::new(3.0, 4.0));

    run_case("(1+0j) * (0+1j) = 0+1j",
             Complex64::new(1.0, 0.0), Complex64::new(0.0, 1.0));

    run_case("(0+1j) * (0+1j) = -1",
             Complex64::new(0.0, 1.0), Complex64::new(0.0, 1.0));

    run_case("(-3+4j) * (2-5j)",
             Complex64::new(-3.0, 4.0), Complex64::new(2.0, -5.0));

    let p45 = Complex64::new((PI / 4.0).cos(), (PI / 4.0).sin());
    run_case("e^(j*pi/4) * e^(j*pi/4) = j", p45, p45);

    run_case("2 * (1+1j)",
             Complex64::new(2.0, 0.0), Complex64::new(1.0, 1.0));

    run_case("(1+2j) * 0",
             Complex64::new(1.0, 2.0), Complex64::new(0.0, 0.0));

    run_case("(1e8+2e8j) * (3e8+4e8j)",
             Complex64::new(1e8, 2e8), Complex64::new(3e8, 4e8));

    println!();
    println!("Done.");
}
