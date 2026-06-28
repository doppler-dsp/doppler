//! State serialization over the FFI — round-trip resume + envelope rejects.
//!
//! Each wrapper exposes the `dp_state.h` triplet (`state_bytes` /
//! `get_state` / `set_state`).  These assert that a mid-stream snapshot,
//! restored into a
//! fresh identically-built instance, resumes **bit-for-bit**, and that a
//! wrong-size or clobbered blob is rejected rather than reinterpreted.

use doppler::acc::{AccCf64, AccF32};
use doppler::fir::Fir;
use doppler::lo::Lo;
use doppler::nco::Nco;
use doppler::StateError;
use num_complex::{Complex, Complex64};

#[test]
fn lo_resumes_bit_for_bit() {
    let mut a = Lo::new(0.05);
    let mut warm = vec![Complex::<f32>::default(); 100];
    a.steps(&mut warm);

    let blob = a.get_state();
    assert_eq!(blob.len(), a.state_bytes());

    let mut b = Lo::new(0.05); // fresh, identical descriptor
    b.set_state(&blob).expect("restore");

    let mut oa = vec![Complex::<f32>::default(); 64];
    let mut ob = vec![Complex::<f32>::default(); 64];
    a.steps(&mut oa);
    b.steps(&mut ob);
    assert_eq!(oa, ob, "restored LO must match the original's continuation");
}

#[test]
fn lo_rejects_bad_blobs() {
    let a = Lo::new(0.05);
    let mut b = Lo::new(0.05);
    let blob = a.get_state();

    // Too short — caught by the size guard before touching C.
    assert!(matches!(
        b.set_state(&blob[..blob.len() - 1]),
        Err(StateError::Size { .. })
    ));
    // Right length, clobbered envelope magic — rejected by the C validator.
    let mut bad = blob.clone();
    bad[0] ^= 0xFF;
    assert_eq!(b.set_state(&bad), Err(StateError::Invalid));
    // A non-bytes / wrong-object blob of the right length is also rejected.
    assert_eq!(
        b.set_state(&vec![0u8; blob.len()]),
        Err(StateError::Invalid)
    );
}

#[test]
fn nco_resumes_bit_for_bit() {
    let mut a = Nco::new(0.01);
    let mut warm = vec![0u32; 100];
    a.steps_u32(&mut warm);

    let blob = a.get_state();
    let mut b = Nco::new(0.01);
    b.set_state(&blob).expect("restore");

    let (mut oa, mut ob) = (vec![0u32; 64], vec![0u32; 64]);
    a.steps_u32(&mut oa);
    b.steps_u32(&mut ob);
    assert_eq!(oa, ob);
}

#[test]
fn fir_resumes_bit_for_bit() {
    let taps: Vec<Complex<f32>> = [0.1, -0.2, 0.3, 0.6, 0.3, -0.2, 0.1]
        .iter()
        .map(|&r| Complex::new(r, 0.0))
        .collect();
    let block: Vec<Complex<f32>> = (0..256)
        .map(|i| Complex::new((i % 5) as f32 - 2.0, 0.2))
        .collect();

    let mut a = Fir::new(&taps);
    let _ = a.execute(&block); // warm the delay line

    let blob = a.get_state();
    let mut b = Fir::new(&taps);
    b.set_state(&blob).expect("restore");

    // The delay line carries across calls, so the next block must match.
    assert_eq!(a.execute(&block), b.execute(&block));
}

#[test]
fn acc_f32_resumes() {
    let mut a = AccF32::new();
    a.add(&[1.5, -0.25, 3.0, 2.0]);

    let blob = a.get_state();
    let mut b = AccF32::new();
    b.set_state(&blob).expect("restore");
    assert_eq!(b.get(), a.get());

    a.push(1.0);
    b.push(1.0);
    assert_eq!(b.get(), a.get());
}

#[test]
fn acc_cf64_resumes() {
    let mut a = AccCf64::new();
    a.add(&[Complex64::new(1.0, 2.0), Complex64::new(-1.0, 0.5)]);

    let blob = a.get_state();
    let mut b = AccCf64::new();
    b.set_state(&blob).expect("restore");
    assert_eq!(b.get(), a.get());
}
