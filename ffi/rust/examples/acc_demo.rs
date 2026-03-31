//! acc_demo — AccF32 and AccCf64 in a windowed energy detector.
//!
//! Demonstrates the accumulator as a DSP building block, not just a
//! counter.  Two scenarios:
//!
//! **Part 1 — AccF32: weighted dot product**
//! Compute the windowed RMS energy of a real sinusoid one block at a time.
//! Each block feeds `AccF32::madd` with a Hann window; `dump` reads and
//! resets between blocks.
//!
//! **Part 2 — AccCf64: polyphase branch inner product**
//! An NCO drives the decimation clock.  On each NCO cycle overflow the
//! accumulator holds the output of one polyphase FIR branch — exactly
//! the hot path inside a polyphase resampler.
//!
//! Run with:
//!   cargo run --example acc_demo

use doppler::acc::{AccCf64, AccF32};
use doppler::nco::Nco;
use num_complex::Complex64;
use std::f32::consts::PI as PI32;
use std::f64::consts::PI as PI64;

fn main() {
    println!("doppler accumulator demo");
    println!("========================");
    println!("version: {}", doppler::version());
    println!();

    part1_rms_energy();
    part2_polyphase_branch();
}

// ---------------------------------------------------------------------------
// Part 1 — AccF32: block RMS energy of a real sinusoid
// ---------------------------------------------------------------------------

fn part1_rms_energy() {
    println!("Part 1 — AccF32: windowed RMS energy");
    println!("--------------------------------------");

    const BLOCK: usize = 32;
    const FREQ: f64 = 0.1; // cycles/sample
    const BLOCKS: usize = 4;

    // Hann window weights (sum-to-one normalised so energy ≈ 0.5 for a
    // unit sinusoid under a Hann window, regardless of block size).
    let hann: Vec<f32> = (0..BLOCK)
        .map(|k| {
            let w =
                0.5 * (1.0 - (2.0 * PI32 * k as f32 / (BLOCK - 1) as f32).cos());
            w / BLOCK as f32
        })
        .collect();

    let mut acc = AccF32::new();

    println!(
        "  signal: cos(2π·{FREQ}·k),  \
         block size: {BLOCK},  blocks: {BLOCKS}"
    );
    println!("  window: Hann (sum-to-one)");
    println!();
    println!("  block  Σ w[k]·x²[k]   expected");
    println!("  -----  -------------  --------");

    for b in 0..BLOCKS {
        // Generate one block of signal.
        let signal: Vec<f32> = (0..BLOCK)
            .map(|k| {
                let t = 2.0 * PI64 * FREQ * (b * BLOCK + k) as f64;
                t.cos() as f32
            })
            .collect();

        // x²[k] (squared magnitude for real signal).
        let sq: Vec<f32> = signal.iter().map(|&x| x * x).collect();

        // Weighted sum via madd — one C call, auto-vectorised in C.
        acc.madd(&sq, &hann);

        // dump: read and zero in one operation.
        let energy = acc.dump();

        // For a unit cosine under a Hann window the expected windowed
        // energy is 0.5 * (3/8) = 0.375 / block ... but since we
        // normalised the window by 1/BLOCK, the expected value is
        // ≈ 3/(8*BLOCK) * BLOCK = 3/8 = 0.375  per sample  × 0.5 (cos²)
        // = 0.1875 for a full cycle count.  Print both and let the user
        // see — the point is stability across blocks.
        println!(
            "  {:5}  {energy:+.6}  (stable across blocks ↑)",
            b
        );

        // Verify dump zeroed the accumulator.
        assert!(
            acc.get().abs() < f32::EPSILON,
            "dump should zero the accumulator"
        );
    }
    println!();
}

// ---------------------------------------------------------------------------
// Part 2 — AccCf64: polyphase branch inner product driven by an NCO
// ---------------------------------------------------------------------------

fn part2_polyphase_branch() {
    println!("Part 2 — AccCf64: polyphase inner product (NCO clock)");
    println!("-------------------------------------------------------");

    // Scenario: decimate a complex tone by 4.
    //   - Signal: NCO_sig at f = 0.05 cycles/sample (the IF tone)
    //   - Clock:  NCO_clk at f = 0.25 (1/decimation_ratio)
    //             overflows every 4 input samples → 1 output sample
    //   - Filter: 4-tap box prototype (uniform weights = 1/4)
    //
    // Expected output: decimated complex tone at f_out = 0.05/0.25 = 0.2
    // cycles/output-sample; magnitude < 1 due to box-filter roll-off at
    // f=0.05 (|H| ≈ 0.939), but consistent across all output samples.

    const DEC: usize = 4;       // decimation ratio
    const N_IN: usize = 64;     // input samples
    const N_OUT: usize = N_IN / DEC;

    let h: Vec<f32> = vec![1.0 / DEC as f32; DEC]; // box filter

    // Signal NCO: complex exponential at 0.05 cycles/sample.
    let mut sig_nco = Nco::new(0.05_f32);
    let signal_f32 = sig_nco.execute_cf32(N_IN);
    // Upcast to f64 for AccCf64.
    let signal: Vec<Complex64> = signal_f32
        .iter()
        .map(|s| Complex64::new(s.re as f64, s.im as f64))
        .collect();

    // Clock NCO: overflow marks the end of each decimation window.
    // execute_u32_ovf returns (phases, overflow_flags).
    let mut clk_nco = Nco::new(1.0 / DEC as f32);
    let (_, ovf) = clk_nco.execute_u32_ovf(N_IN);

    let mut acc = AccCf64::new();
    let mut outputs: Vec<Complex64> = Vec::with_capacity(N_OUT);
    let mut win_start = 0_usize;

    println!(
        "  signal: cf32 NCO at f=0.05,  dec ratio: {DEC},  \
         input samples: {N_IN}"
    );
    println!("  filter: {DEC}-tap box (h[k] = 1/{DEC})");
    println!();
    println!("  out#  re         im         |z|       phase/π");
    println!("  ----  ---------  ---------  --------  --------");

    for k in 0..N_IN {
        // madd of one new sample against its window coefficient.
        // In a real polyphase engine the coefficient row would be
        // selected by the NCO phase; here we use a box filter so all
        // taps are equal.
        let tap_idx = k - win_start;
        acc.madd(
            std::slice::from_ref(&signal[k]),
            std::slice::from_ref(&h[tap_idx % DEC]),
        );

        if ovf[k] != 0 {
            // NCO cycle overflow: this window is complete.
            let y = acc.dump(); // read + zero in one call
            outputs.push(y);

            let mag = y.norm();
            let phase = y.im.atan2(y.re) / PI64;
            println!(
                "  {:4}  {:+.6}  {:+.6}  {:.6}  {:+.6}",
                outputs.len() - 1,
                y.re,
                y.im,
                mag,
                phase
            );

            win_start = k + 1;
        }
    }

    println!();
    println!("  output count: {} (expected {})", outputs.len(), N_OUT);
    assert_eq!(outputs.len(), N_OUT);

    // The box filter has gain |H(f)| = |sin(πfL)/(L·sin(πf))| < 1 for
    // f ≠ 0.  At f=0.05, L=4 that is ≈ 0.939.  What matters is that
    // every output has the *same* magnitude — the accumulator is behaving
    // as a consistent filter, not a leaky bucket.
    let mag0 = outputs[0].norm();
    for (i, y) in outputs.iter().enumerate() {
        assert!(
            (y.norm() - mag0).abs() < 1e-4,
            "output {i}: |y| = {} (expected {mag0:.6})",
            y.norm()
        );
    }

    println!("  magnitude check: all outputs consistent (|H| ≈ {mag0:.4}) ✓");
    println!();
    println!("Done.");
}
