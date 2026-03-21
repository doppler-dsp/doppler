//! fft_demo — Rust FFI example: 1-D FFT via doppler C library.
//!
//! Demonstrates the zero-copy global-plan FFT API from Rust.
//!
//! Run with:
//!   cargo run --example fft_demo
//!
//! The output should match the C fft_demo exactly:
//!   - Cosine input: energy at bins 1 and N-1 (real, ≈ +N/2)
//!   - Sine input:   energy at bins 1 and N-1 (imaginary, ≈ ∓j·N/2)
//!   - 2-D DC bin:   equals the sum of all inputs

use std::f64::consts::PI;

use num_complex::Complex64;
use doppler::fft::{self, Direction};

fn main() {
    println!("=== doppler FFT Demo (Rust FFI) ===");
    println!();
    println!("Library version: {}", doppler::version());
    println!();
    println!("Zero-copy pattern: allocate → setup (bind) → fill → execute.");

    // -----------------------------------------------------------------------
    // 1-D out-of-place FFT: cosine input
    // -----------------------------------------------------------------------
    {
        let n = 16_usize;

        println!();
        println!("--- 1-D out-of-place FFT (N={n}, input: cos(2πk/N)) ---");
        println!(
            "Expected: Y[1] ≈ +{:.1} (real), Y[{}] ≈ +{:.1} (real), rest ≈ 0",
            n as f64 / 2.0, n - 1, n as f64 / 2.0
        );

        let mut input  = vec![Complex64::default(); n];
        let mut output = vec![Complex64::default(); n];

        // Bind plan to these exact Vec allocations
        fft::setup_1d(n, Direction::Forward);

        // Fill after setup — no risk of planner clobbering data
        for (i, x) in input.iter_mut().enumerate() {
            let t = 2.0 * PI * i as f64 / n as f64;
            *x = Complex64::new(t.cos(), 0.0);
        }

        fft::execute_1d(&input, &mut output);

        print_spectrum(&output, 3);
    }

    // -----------------------------------------------------------------------
    // 1-D in-place FFT: sine input
    // -----------------------------------------------------------------------
    {
        let n = 16_usize;

        println!();
        println!("--- 1-D in-place FFT (N={n}, input: sin(2πk/N)) ---");
        println!(
            "Expected: Y[1] ≈ -j{:.1}, Y[{}] ≈ +j{:.1}, rest ≈ 0",
            n as f64 / 2.0, n - 1, n as f64 / 2.0
        );

        let mut data = vec![Complex64::default(); n];

        fft::setup_1d(n, Direction::Forward);

        for (i, x) in data.iter_mut().enumerate() {
            let t = 2.0 * PI * i as f64 / n as f64;
            *x = Complex64::new(t.sin(), 0.0);
        }

        fft::execute_1d_inplace(&mut data);

        print_spectrum(&data, 3);
    }

    // -----------------------------------------------------------------------
    // 2-D out-of-place FFT: sin(k) input
    // -----------------------------------------------------------------------
    {
        let ny = 4_usize;
        let nx = 4_usize;
        let total = ny * nx;

        println!();
        println!("--- 2-D out-of-place FFT ({ny}×{nx}, input: sin(k)) ---");

        let mut input  = vec![Complex64::default(); total];
        let mut output = vec![Complex64::default(); total];

        fft::setup_2d(ny, nx, Direction::Forward);

        for (i, x) in input.iter_mut().enumerate() {
            *x = Complex64::new((i as f64).sin(), 0.0);
        }

        let dc_ref: Complex64 = input.iter().sum();

        println!("Input (row-major):");
        for row in 0..ny {
            print!("  row {row}:");
            for col in 0..nx {
                print!("  {:+.4}", input[row * nx + col].re);
            }
            println!();
        }

        fft::execute_2d(&input, &mut output);

        println!(
            "Output[0,0]: {:+.4} {:+.4}j  (DC = sum of inputs)",
            output[0].re, output[0].im
        );
        println!(
            "Expected DC: {:+.4} {:+.4}j",
            dc_ref.re, dc_ref.im
        );
    }

    println!();
    println!("Demo complete.");
}

fn print_spectrum(y: &[Complex64], show: usize) {
    println!("    {:<5}  {:>12}  {:>12}", "bin", "real", "imag");
    let n = y.len();
    for k in 0..show.min(n) {
        println!("    [{:3}]  {:+12.4}  {:+12.4}", k, y[k].re, y[k].im);
    }
    if 2 * show < n {
        println!("     ...");
        for k in (n - show)..n {
            println!("    [{:3}]  {:+12.4}  {:+12.4}", k, y[k].re, y[k].im);
        }
    }
}
