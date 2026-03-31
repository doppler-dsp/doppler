/// C-compatible sample types mirroring the doppler wire formats.

/// Complex 32-bit float sample (`{float i; float q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCf32 {
    pub i: f32,
    pub q: f32,
}

impl From<DpCf32> for num_complex::Complex<f32> {
    fn from(s: DpCf32) -> Self {
        num_complex::Complex::new(s.i, s.q)
    }
}

impl From<num_complex::Complex<f32>> for DpCf32 {
    fn from(c: num_complex::Complex<f32>) -> Self {
        DpCf32 { i: c.re, q: c.im }
    }
}

/// Complex 8-bit integer sample (`{int8_t i; int8_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi8 {
    pub i: i8,
    pub q: i8,
}

/// Complex 16-bit integer sample (`{int16_t i; int16_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi16 {
    pub i: i16,
    pub q: i16,
}

/// Complex 32-bit integer sample (`{int32_t i; int32_t q;}`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DpCi32 {
    pub i: i32,
    pub q: i32,
}
