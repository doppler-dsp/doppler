//! fft_bench — FFT throughput comparison: doppler (C/FFTW) vs. RustFFT.
//!
//! Mirrors the methodology of c/bench/bench_fft.c so the numbers are
//! directly comparable.
//!
//! Both libraries:
//!   - Use out-of-place execution (input buffer → output buffer)
//!   - Exclude planning time from the measured loop
//!   - Run the same number of iterations at the same FFT sizes
//!
//! doppler uses FFTW PATIENT planning (optimal plan at cost of longer
//! planning time).  RustFFT plans ahead of time with FftPlanner.
//!
//! Run with:
//!   cargo run --release --example fft_bench
//!
//! IMPORTANT: --release is required for meaningful numbers.

use std::ffi::CString;
use std::time::Instant;

use num_complex::Complex64;
use rustfft::FftPlanner;
use doppler::fft;

const ITERS_1D: &[(usize, usize)] = &[
    (1024,  2000),
    (4096,  1000),
    (16384,  200),
];

const ITERS_2D: &[(usize, usize, usize)] = &[
    (64,  64,  2000),
    (128, 128,  500),
    (256, 256,  100),
];

fn fill_signal(buf: &mut [Complex64]) {
    let n = buf.len() as f64;
    for (i, x) in buf.iter_mut().enumerate() {
        let t = 2.0 * std::f64::consts::PI * i as f64 / n;
        *x = Complex64::new(t.cos(), (2.0 * t).sin());
    }
}

fn bench_na_1d(n: usize, iters: usize) {
    let mut input  = vec![Complex64::default(); n];
    let mut output = vec![Complex64::default(); n];

    // Setup with PATIENT — matches c/bench/bench_fft.c exactly.
    // patient planner may clobber input; we fill after.
    {
        let shape   = [n];
        let planner = CString::new("patient").unwrap();
        let wisdom  = CString::new("").unwrap();
        unsafe {
            doppler::fft::dp_fft_global_setup(
                shape.as_ptr(), 1, 1, 4,
                planner.as_ptr(), wisdom.as_ptr(),
            );
        }
    }

    fill_signal(&mut input);

    // First execute: binds plan to input/output pointers.
    fft::execute_1d(&input, &mut output);

    // Timed loop
    let t0 = Instant::now();
    for _ in 0..iters {
        fft::execute_1d(&input, &mut output);
    }
    let elapsed = t0.elapsed();

    let dt   = elapsed.as_secs_f64() / iters as f64;
    let msps = n as f64 / dt / 1e6;
    println!("  na fft1d (n={n:6}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn bench_rustfft_1d(n: usize, iters: usize) {
    let mut planner = FftPlanner::<f64>::new();
    let fft = planner.plan_fft_forward(n);

    let mut input   = vec![Complex64::default(); n];
    let mut output  = vec![Complex64::default(); n];
    let mut scratch = vec![Complex64::default(); fft.get_outofplace_scratch_len()];

    fill_signal(&mut input);

    // Warm-up
    fft.process_outofplace_with_scratch(&mut input, &mut output, &mut scratch);

    let t0 = Instant::now();
    for _ in 0..iters {
        fft.process_outofplace_with_scratch(&mut input, &mut output, &mut scratch);
    }
    let elapsed = t0.elapsed();

    let dt   = elapsed.as_secs_f64() / iters as f64;
    let msps = n as f64 / dt / 1e6;
    println!("  rustfft  (n={n:6}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn bench_na_2d(ny: usize, nx: usize, iters: usize) {
    let total = ny * nx;
    let mut input  = vec![Complex64::default(); total];
    let mut output = vec![Complex64::default(); total];

    {
        let shape   = [ny, nx];
        let planner = CString::new("patient").unwrap();
        let wisdom  = CString::new("").unwrap();
        unsafe {
            doppler::fft::dp_fft_global_setup(
                shape.as_ptr(), 2, 1, 4,
                planner.as_ptr(), wisdom.as_ptr(),
            );
        }
    }

    for (i, x) in input.iter_mut().enumerate() {
        *x = Complex64::new((i as f64).sin(), (i as f64).cos());
    }

    fft::execute_2d(&input, &mut output);

    let t0 = Instant::now();
    for _ in 0..iters {
        fft::execute_2d(&input, &mut output);
    }
    let elapsed = t0.elapsed();

    let dt   = elapsed.as_secs_f64() / iters as f64;
    let msps = total as f64 / dt / 1e6;
    println!("  na fft2d ({ny}x{nx:3}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn bench_rustfft_2d(ny: usize, nx: usize, iters: usize) {
    // RustFFT doesn't have a native 2-D FFT; use row+column decomposition.
    let mut planner = FftPlanner::<f64>::new();
    let fft_row = planner.plan_fft_forward(nx);
    let fft_col = planner.plan_fft_forward(ny);

    let total = ny * nx;
    let mut buf     = vec![Complex64::default(); total];
    let mut scratch_r = vec![Complex64::default(); fft_row.get_inplace_scratch_len()];
    let mut scratch_c = vec![Complex64::default(); fft_col.get_inplace_scratch_len()];

    for (i, x) in buf.iter_mut().enumerate() {
        *x = Complex64::new((i as f64).sin(), (i as f64).cos());
    }

    let mut tmp = vec![Complex64::default(); total];

    // 2-D FFT via row-then-column decomposition.
    macro_rules! fft_2d {
        ($buf:expr) => {{
            for row in 0..ny {
                fft_row.process_with_scratch(&mut $buf[row*nx..(row+1)*nx], &mut scratch_r);
            }
            for r in 0..ny {
                for c in 0..nx {
                    tmp[c*ny + r] = $buf[r*nx + c];
                }
            }
            for col in 0..nx {
                fft_col.process_with_scratch(&mut tmp[col*ny..(col+1)*ny], &mut scratch_c);
            }
            for r in 0..ny {
                for c in 0..nx {
                    $buf[r*nx + c] = tmp[c*ny + r];
                }
            }
        }};
    }

    // Warm-up
    fft_2d!(buf);

    let t0 = Instant::now();
    for _ in 0..iters {
        fft_2d!(buf);
    }
    let elapsed = t0.elapsed();

    let dt   = elapsed.as_secs_f64() / iters as f64;
    let msps = total as f64 / dt / 1e6;
    println!("  rustfft  ({ny}x{nx:3}):  dt = {dt:.6} s  MS/s = {msps:.2}");
}

fn main() {
    println!("=== doppler vs. RustFFT Benchmark ===");
    println!();
    println!("  doppler: FFTW PATIENT plan, 4 threads, out-of-place");
    println!("  RustFFT:      FftPlanner (AVX2/SSE auto-vectorised), out-of-place");
    println!("  (Planning time excluded from all measurements)");
    println!();

    println!("--- 1-D FFT ---");
    for &(n, iters) in ITERS_1D {
        bench_na_1d(n, iters);
        bench_rustfft_1d(n, iters);
        println!();
    }

    println!("--- 2-D FFT ---");
    for &(ny, nx, iters) in ITERS_2D {
        bench_na_2d(ny, nx, iters);
        bench_rustfft_2d(ny, nx, iters);
        println!();
    }

    println!("Done.");
}
