//! fft_bench — FFT throughput: doppler (pocketfft) vs. RustFFT.
//!
//! Both libraries use out-of-place execution; planning time is excluded.
//!
//! Run with:
//!   cargo run --release --example fft_bench
//!
//! IMPORTANT: --release is required for meaningful numbers.

use doppler::fft::{Fft, Direction};
use num_complex::Complex64;
use rustfft::FftPlanner;
use std::time::Instant;

const ITERS_1D: &[(usize, usize)] = &[
    (1024, 2000),
    (4096, 1000),
    (16384, 200),
];

fn fill_signal(buf: &mut [Complex64]) {
    let n = buf.len() as f64;
    for (i, x) in buf.iter_mut().enumerate() {
        let t = 2.0 * std::f64::consts::PI * i as f64 / n;
        *x = Complex64::new(t.cos(), (2.0 * t).sin());
    }
}

fn bench_doppler_1d(n: usize, iters: usize) {
    let mut input = vec![Complex64::default(); n];
    let mut output = vec![Complex64::default(); n];
    fill_signal(&mut input);

    let fft = Fft::new(n, Direction::Forward);
    fft.execute_cf64(&input, &mut output); // warm-up

    let t0 = Instant::now();
    for _ in 0..iters {
        fft.execute_cf64(&input, &mut output);
    }
    let elapsed = t0.elapsed();

    let dt = elapsed.as_secs_f64() / iters as f64;
    let msps = n as f64 / dt / 1e6;
    println!("  doppler  (n={n:6}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn bench_rustfft_1d(n: usize, iters: usize) {
    let mut planner = FftPlanner::<f64>::new();
    let fft = planner.plan_fft_forward(n);

    let mut input = vec![Complex64::default(); n];
    let mut output = vec![Complex64::default(); n];
    let mut scratch =
        vec![Complex64::default(); fft.get_outofplace_scratch_len()];
    fill_signal(&mut input);

    fft.process_outofplace_with_scratch(
        &mut input,
        &mut output,
        &mut scratch,
    );

    let t0 = Instant::now();
    for _ in 0..iters {
        fft.process_outofplace_with_scratch(
            &mut input,
            &mut output,
            &mut scratch,
        );
    }
    let elapsed = t0.elapsed();

    let dt = elapsed.as_secs_f64() / iters as f64;
    let msps = n as f64 / dt / 1e6;
    println!("  rustfft  (n={n:6}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn main() {
    println!("=== doppler vs. RustFFT Benchmark ===");
    println!();
    println!("  doppler: pocketfft, per-instance plan, out-of-place");
    println!(
        "  RustFFT: FftPlanner (AVX2/SSE auto-vectorised), out-of-place"
    );
    println!("  (Planning time excluded from all measurements)");
    println!();

    println!("--- 1-D CF64 FFT ---");
    for &(n, iters) in ITERS_1D {
        bench_doppler_1d(n, iters);
        bench_rustfft_1d(n, iters);
        println!();
    }

    println!("Done.");
}
