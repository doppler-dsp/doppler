//! acc_demo — AccF32 and AccCf64 in a windowed energy detector.
//!
//! **Part 1 — AccF32: weighted dot product**
//! Windowed RMS energy of a real sinusoid one block at a time.
//!
//! **Part 2 — AccCf64: polyphase branch inner product**
//! An NCO drives the decimation clock; Lo generates the complex signal.
//!
//! Run with:
//!   cargo run --example acc_demo

use doppler::acc::{AccCf64, AccF32};
use doppler::lo::Lo;
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

fn part1_rms_energy() {
    println!("Part 1 — AccF32: windowed RMS energy");
    println!("--------------------------------------");

    const BLOCK: usize = 32;
    const FREQ: f64 = 0.1;
    const BLOCKS: usize = 4;

    let hann: Vec<f32> = (0..BLOCK)
        .map(|k| {
            let w = 0.5
                * (1.0
                    - (2.0 * PI32 * k as f32 / (BLOCK - 1) as f32).cos());
            w / BLOCK as f32
        })
        .collect();

    let mut acc = AccF32::new();

    println!(
        "  signal: cos(2π·{FREQ}·k),  block size: {BLOCK},  blocks: {BLOCKS}"
    );
    println!("  window: Hann (sum-to-one)");
    println!();
    println!("  block  Σ w[k]·x²[k]");
    println!("  -----  -------------");

    for b in 0..BLOCKS {
        let signal: Vec<f32> = (0..BLOCK)
            .map(|k| {
                let t = 2.0 * PI64 * FREQ * (b * BLOCK + k) as f64;
                t.cos() as f32
            })
            .collect();

        let sq: Vec<f32> = signal.iter().map(|&x| x * x).collect();
        acc.madd(&sq, &hann);
        let energy = acc.dump();

        println!("  {:5}  {energy:+.6}", b);
        assert!(
            acc.get().abs() < f32::EPSILON,
            "dump should zero the accumulator"
        );
    }
    println!();
}

fn part2_polyphase_branch() {
    println!("Part 2 — AccCf64: polyphase inner product (NCO clock)");
    println!("-------------------------------------------------------");

    const DEC: usize = 4;
    const N_IN: usize = 64;
    const N_OUT: usize = N_IN / DEC;

    let h: Vec<f32> = vec![1.0 / DEC as f32; DEC];

    // Complex signal from Lo at f=0.05.
    let mut sig_lo = Lo::new(0.05_f32);
    let mut sig_cf32 =
        vec![num_complex::Complex::<f32>::default(); N_IN];
    sig_lo.execute_cf32(&mut sig_cf32);
    let signal: Vec<Complex64> = sig_cf32
        .iter()
        .map(|s| Complex64::new(s.re as f64, s.im as f64))
        .collect();

    // Clock NCO: overflow marks end of each decimation window.
    let mut clk_nco = Nco::new(1.0 / DEC as f32);
    let mut phase_buf = vec![0u32; N_IN];
    let mut ovf = vec![0u8; N_IN];
    clk_nco.execute_u32_ovf(&mut phase_buf, &mut ovf);

    let mut acc = AccCf64::new();
    let mut outputs: Vec<Complex64> = Vec::with_capacity(N_OUT);
    let mut win_start = 0_usize;

    println!(
        "  signal: Lo at f=0.05,  dec ratio: {DEC},  input samples: {N_IN}"
    );
    println!("  filter: {DEC}-tap box (h[k] = 1/{DEC})");
    println!();
    println!("  out#  re         im         |z|       phase/π");
    println!("  ----  ---------  ---------  --------  --------");

    for k in 0..N_IN {
        let tap_idx = k - win_start;
        acc.madd(
            std::slice::from_ref(&signal[k]),
            std::slice::from_ref(&h[tap_idx % DEC]),
        );

        if ovf[k] != 0 {
            let y = acc.dump();
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

    let mag0 = outputs[0].norm();
    for (i, y) in outputs.iter().enumerate() {
        assert!(
            (y.norm() - mag0).abs() < 1e-4,
            "output {i}: |y| = {} (expected {mag0:.6})",
            y.norm()
        );
    }

    println!(
        "  magnitude check: all outputs consistent (|H| ≈ {mag0:.4}) ✓"
    );
    println!();
    println!("Done.");
}
