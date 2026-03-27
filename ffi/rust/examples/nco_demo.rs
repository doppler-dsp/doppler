//! NCO demo — generates a tone and prints the first few IQ samples.
//!
//! Run with:
//!   cargo run --example nco_demo
//!
//! The NCO runs at 0.1 cycles/sample (period = 10 samples).
//! Sample 0 starts at phase 0; sample 1 should be rotated by 2π·0.1 rad.

use doppler::nco::Nco;
use std::f32::consts::PI;

fn main() {
    println!("doppler NCO demo");
    println!("================");
    println!("version: {}", doppler::version());
    println!();

    // ── Single-frequency tone ──────────────────────────────────────────────
    let freq = 0.1_f32; // cycles / sample  (period = 10)
    let n = 20_usize;

    let mut nco = Nco::new(freq);
    let samples = nco.execute_cf32(n);

    println!(
        "Tone at f = {freq} cycles/sample  ({n} samples)\n"
    );
    println!("  k   re        im        |z|       angle/π");
    println!("  --- --------- --------- --------- ---------");
    for (k, s) in samples.iter().enumerate() {
        let mag = (s.re * s.re + s.im * s.im).sqrt();
        let angle = s.im.atan2(s.re) / PI;
        println!(
            "  {k:3}  {:+.5}  {:+.5}  {:.5}   {:+.5}",
            s.re, s.im, mag, angle
        );
    }
    println!();

    // ── FM modulation via ctrl port ────────────────────────────────────────
    // Apply a sinusoidal frequency deviation: Δf = 0.01·sin(2π·k/N)
    println!("FM control-port demo (Δf = 0.01·sin(2πk/N))");
    println!("  k   re        im");
    println!("  --- --------- ---------");

    let ctrl: Vec<f32> = (0..n)
        .map(|k| 0.01 * (2.0 * PI * k as f32 / n as f32).sin())
        .collect();
    nco.reset();
    let fm_samples = nco.execute_cf32_ctrl(&ctrl, n);

    for (k, s) in fm_samples.iter().enumerate() {
        println!("  {k:3}  {:+.5}  {:+.5}", s.re, s.im);
    }
    println!();

    // ── Raw phase accumulator ──────────────────────────────────────────────
    println!("Raw u32 phase accumulator (first 10 values)");
    nco.reset();
    let phases = nco.execute_u32(10);
    for (k, &p) in phases.iter().enumerate() {
        let norm = p as f64 / u32::MAX as f64;
        println!("  phase[{k}] = {p:#010x}  ({norm:.5} cycles)");
    }
    println!();

    // ── Overflow / cycle detection ────────────────────────────────────────
    println!("Overflow flags at f = 0.5 (expect wrap every 2 samples)");
    let mut nco2 = Nco::new(0.5);
    let (_, ovf) = nco2.execute_u32_ovf(10);
    let flags: Vec<u8> = ovf.clone();
    println!("  ovf = {flags:?}");
    println!();

    println!("Done.");
}
