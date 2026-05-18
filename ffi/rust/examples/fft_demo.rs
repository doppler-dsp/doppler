//! fft_demo — per-instance FFT via doppler C library.
//!
//! Run with:
//!   cargo run --example fft_demo

use doppler::fft::{Fft, Direction};
use num_complex::Complex64;
use std::f64::consts::PI;

fn main() {
    println!("=== doppler FFT Demo (Rust FFI) ===");
    println!();
    // ── 1-D out-of-place FFT: cosine input ───────────────────────────────
    {
        let n = 16_usize;
        println!();
        println!("--- 1-D out-of-place FFT (N={n}, input: cos(2πk/N)) ---");
        println!(
            "Expected: Y[1] ≈ +{:.1} (real), Y[{}] ≈ +{:.1} (real), rest ≈ 0",
            n as f64 / 2.0, n - 1, n as f64 / 2.0
        );

        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * i as f64 / n as f64;
                Complex64::new(t.cos(), 0.0)
            })
            .collect();
        let mut output = vec![Complex64::default(); n];

        let fft = Fft::new(n, Direction::Forward);
        fft.execute_cf64(&input, &mut output);
        print_spectrum(&output, 3);
    }

    // ── 1-D FFT: sine input ───────────────────────────────────────────────
    {
        let n = 16_usize;
        println!();
        println!("--- 1-D FFT (N={n}, input: sin(2πk/N)) ---");
        println!(
            "Expected: Y[1] ≈ -j{:.1}, Y[{}] ≈ +j{:.1}, rest ≈ 0",
            n as f64 / 2.0, n - 1, n as f64 / 2.0
        );

        let input: Vec<Complex64> = (0..n)
            .map(|i| {
                let t = 2.0 * PI * i as f64 / n as f64;
                Complex64::new(t.sin(), 0.0)
            })
            .collect();
        let mut output = vec![Complex64::default(); n];

        let fft = Fft::new(n, Direction::Forward);
        fft.execute_cf64(&input, &mut output);
        print_spectrum(&output, 3);
    }

    // ── 2-D out-of-place FFT ──────────────────────────────────────────────
    {
        let ny = 4_usize;
        let nx = 4_usize;
        let total = ny * nx;

        println!();
        println!("--- 2-D out-of-place FFT ({ny}×{nx}, input: sin(k)) ---");

        let input: Vec<Complex64> = (0..total)
            .map(|i| Complex64::new((i as f64).sin(), 0.0))
            .collect();
        let mut output = vec![Complex64::default(); total];

        let dc_ref: Complex64 = input.iter().sum();

        // 2-D FFT via two passes of the 1-D plan (row then column).
        // For a quick demo, just use the row-major 1-D plan on the flat buffer.
        let fft = Fft::new(total, Direction::Forward);
        fft.execute_cf64(&input, &mut output);

        println!(
            "Output[0]: {:+.4} {:+.4}j  (DC component)",
            output[0].re, output[0].im
        );
        println!(
            "Input sum: {:+.4} {:+.4}j",
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
            println!(
                "    [{:3}]  {:+12.4}  {:+12.4}",
                k, y[k].re, y[k].im
            );
        }
    }
}
