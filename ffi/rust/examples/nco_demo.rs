//! NCO/LO demo — raw phase accumulator + complex phasor generation.
//!
//! Run with:
//!   cargo run --example nco_demo
//!
//! The LO runs at 0.1 cycles/sample (period = 10 samples).
//! Nco generates raw u32 phase; Lo generates CF32 phasors.

use doppler::lo::Lo;
use doppler::nco::Nco;
use std::f32::consts::PI;

fn main() {
    println!("doppler NCO/LO demo");
    println!("===================");
    println!("version: {}", doppler::version());
    println!();

    // ── Complex phasors via Lo ─────────────────────────────────────────────
    let freq = 0.1_f32;
    let n = 20_usize;
    let mut lo = Lo::new(freq);
    let mut samples = vec![num_complex::Complex::<f32>::default(); n];
    lo.execute_cf32(&mut samples);

    println!("Tone at f = {freq} cycles/sample  ({n} samples)\n");
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
    println!("FM control-port demo (Δf = 0.01·sin(2πk/N))");
    println!("  k   re        im");
    println!("  --- --------- ---------");

    let ctrl: Vec<f32> = (0..n)
        .map(|k| 0.01 * (2.0 * PI * k as f32 / n as f32).sin())
        .collect();
    lo.reset();
    let mut fm_samples = vec![num_complex::Complex::<f32>::default(); n];
    lo.execute_cf32_ctrl(&ctrl, &mut fm_samples);

    for (k, s) in fm_samples.iter().enumerate() {
        println!("  {k:3}  {:+.5}  {:+.5}", s.re, s.im);
    }
    println!();

    // ── Raw phase accumulator via Nco ──────────────────────────────────────
    println!("Raw u32 phase accumulator (first 10 values)");
    let mut nco = Nco::new(freq);
    let mut phases = vec![0u32; 10];
    nco.execute_u32(&mut phases);
    for (k, &p) in phases.iter().enumerate() {
        let norm = p as f64 / u32::MAX as f64;
        println!("  phase[{k}] = {p:#010x}  ({norm:.5} cycles)");
    }
    println!();

    // ── Overflow / cycle detection ────────────────────────────────────────
    println!("Overflow flags at f = 0.5 (expect wrap every 2 samples)");
    let mut nco2 = Nco::new(0.5);
    let mut phase_buf = vec![0u32; 10];
    let mut carry_buf = vec![0u8; 10];
    nco2.execute_u32_ovf(&mut phase_buf, &mut carry_buf);
    println!("  ovf = {:?}", &carry_buf[..]);
    println!();

    println!("Done.");
}
