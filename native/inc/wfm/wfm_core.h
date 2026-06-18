/**
 * @file wfm_core.h
 * @brief Wfmgen module — public C API.
 */
#ifndef WFM_CORE_H
#define WFM_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Map binary bits {0, 1} to BPSK constellation symbols (cf32).
 * The mapping is: 0 -> +1 + 0j, 1 -> -1 + 0j.  Output is unit-power
 * (each symbol has magnitude 1).  The imaginary component is always zero.
 * Typically used before a carrier multiply and noise addition to build a
 * BPSK burst without the full Synth engine.
 *
 * @param bits      Array of uint8 values; only the LSB of each byte is used.
 * @param bits_len  Number of elements in ``bits``.
 * @param out       Output buffer of at least ``bits_len`` cf32 elements.
 * @code
 * >>> from doppler.wfm import bpsk_map
 * >>> import numpy as np
 * >>> bits = np.array([0, 1, 0, 1], dtype=np.uint8)
 * >>> bpsk_map(bits).tolist()
 * [(1+0j), (-1+0j), (1+0j), (-1+0j)]
 * @endcode
 */
void bpsk_map(const uint8_t *bits, size_t bits_len, float complex *out);

/**
 * @brief Map QPSK symbol indices {0, 1, 2, 3} to Gray-coded symbols (cf32).
 * Gray coding: adjacent indices differ in exactly one bit, minimising
 * BER at low SNR.  Bit 0 (LSB) controls I, bit 1 controls Q:
 *   I = (1 - 2*b_i) / sqrt(2),  Q = (1 - 2*b_q) / sqrt(2).
 * Output is unit-power (|sym| = 1.0 exactly).  The four constellation
 * points lie at the cardinal diagonals of the IQ plane.
 *
 * @param syms      Array of uint8 symbol indices; values must be in {0,1,2,3}.
 *              Bits above position 1 are ignored.
 * @param syms_len  Number of elements in ``syms``.
 * @param out       Output buffer of at least ``syms_len`` cf32 elements.
 * @code
 * >>> from doppler.wfm import qpsk_map
 * >>> import numpy as np
 * >>> idx = np.array([0, 1, 2, 3], dtype=np.uint8)
 * >>> out = qpsk_map(idx)
 * >>> [round(float(v.real), 4) for v in out]
 * [0.7071, -0.7071, 0.7071, -0.7071]
 * >>> [round(float(v.imag), 4) for v in out]
 * [0.7071, 0.7071, -0.7071, -0.7071]
 * @endcode
 */
void qpsk_map(const uint8_t *syms, size_t syms_len, float complex *out);

/**
 * @brief Compute the per-component AWGN amplitude for a target SNR.
 * The AWGN engine uses equal-power I and Q noise: complex noise power is
 * 2 * amplitude².  This function inverts that relationship for a given
 * ``signal_power`` and ``snr_db`` (measured over the full sample-rate
 * bandwidth):
 *   amplitude = sqrt(signal_power / (2 * 10^(snr_db / 10))).
 * Pass the result directly to ``awgn_create`` to get the exact noise level
 * that corresponds to the requested SNR.
 *
 * @param snr_db  Target SNR in dB, referenced to the full sample rate.
 * @param signal_power  RMS power of the signal (e.g. 1.0 for unit-power
 *              complex tones or unit-energy BPSK/QPSK symbols).
 * @return Per-component AWGN amplitude (sigma for one I or Q channel).
 * @code
 * >>> from doppler.wfm import wfm_awgn_amplitude
 * >>> round(float(wfm_awgn_amplitude(10.0, 1.0)), 6)
 * 0.223607
 * >>> round(float(wfm_awgn_amplitude(0.0, 1.0)), 6)
 * 0.707107
 * @endcode
 */
float wfm_awgn_amplitude(float snr_db, float signal_power);

/**
 * @brief Convert Eb/No (dB) to SNR (dB) over the full sample-rate band.
 * Digital communication systems are typically specified in Eb/No; doppler
 * uses an fs-band SNR internally.  The conversion is:
 *   SNR_fs = Eb/No + 10 log10(bits_per_symbol) - 10 log10(samples_per_symbol)
 * For BPSK (bits_per_symbol=1, sps=8) at Eb/No=10 dB this gives ~0.97 dB.
 * For QPSK (bits_per_symbol=2, sps=8) at Eb/No=10 dB this gives ~3.98 dB.
 *
 * @param ebno_db  Eb/No in dB (energy per bit over noise spectral density).
 * @param bits_per_symbol  Bits carried per modulation symbol: 1 for BPSK,
 *              2 for QPSK.
 * @param samples_per_symbol  Oversampling ratio (sps), e.g. 8.0.
 * @return SNR in dB measured over the full sample-rate bandwidth.
 * @code
 * >>> from doppler.wfm import wfm_ebno_to_snr_db
 * >>> round(float(wfm_ebno_to_snr_db(10.0, 2, 8.0)), 4)
 * 3.9794
 * >>> round(float(wfm_ebno_to_snr_db(10.0, 1, 8.0)), 4)
 * 0.9691
 * @endcode
 */
float wfm_ebno_to_snr_db(float ebno_db, int bits_per_symbol, float samples_per_symbol);

/**
 * @brief Maximal-length-sequence primitive polynomial for a length-@p n LFSR.
 * Returns the tap mask (in the same bit convention the synth/PN engine uses
 * for `pn_poly = 0`) that drives an @p n-stage Fibonacci LFSR through its full
 * 2^n - 1 state period.  Thin public alias over the synth engine's MLS table;
 * valid for @p n in 2..64 and returns 0 otherwise.
 *
 * @param n  LFSR length in stages (2..64).
 * @return Primitive-polynomial tap mask, or 0 if @p n is out of range.
 * @code
 * >>> from doppler.wfm import mls_poly
 * >>> hex(mls_poly(7))
 * '0x41'
 * @endcode
 */
uint64_t mls_poly(uint32_t n);
void rrc_taps(double beta, int sps, int span, float *out);
void dsss_spread(const float complex *syms, size_t syms_len, const uint8_t *code, size_t code_len, int sf, float complex *out);
#ifdef __cplusplus
}
#endif

#endif /* WFM_CORE_H */
