- **One fast dB conversion, `dp_power_to_db_f32`, and PSD reads dB through
    it** (#2094, #2074). It is 10·log10 within 0.01 dB (measured 3.25e-4 over
    every float32), exact at every power of two, with PSD's −200 dB floor,
    and vectorized: PSD's dB conversion is about 6× faster. Every PSD and
    Spectrogram dB reading is now this function of the linear one, bit for
    bit, and moves by at most 3.25e-4 dB. Python:
    `doppler.spectral.power_to_db_f32`. Other converters: #2108.
