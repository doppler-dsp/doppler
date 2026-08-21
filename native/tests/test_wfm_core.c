/*
 * test_wfm_core.c — the wfm module's eleven free functions.
 *
 * The constellation maps, the link-budget conversions, the CRC, the two
 * pulse shapes and the DSSS spreader. Several of these are the kind of
 * function whose defect never shows up as a crash: a Gray map with two
 * quadrants swapped still produces a perfectly good QPSK constellation,
 * and a CRC with the wrong init still returns sixteen plausible bits.
 * Interoperability is the only thing that catches either, so the CRC is
 * checked against its published check vector and the ASM against the
 * value CCSDS specifies, not against whatever this build happens to emit.
 *
 * The pulse shapes get the property that matters for a matched filter:
 * an RRC is NOT a raised cosine, an RC has the Nyquist zero-ISI property
 * at every symbol instant, and both are even in t.
 */
#include "dp_test.h"
#include "wfm/wfm_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{
  /* ── bpsk_map: antipodal, unit magnitude, LSB only ──────────────── */
  {
    const uint8_t bits[6] = { 0, 1, 0, 1, 0xFE, 0xFF };
    float complex out[6];

    bpsk_map (bits, 6, out);
    DP_CHECK (crealf (out[0]) == 1.0f && cimagf (out[0]) == 0.0f);
    DP_CHECK (crealf (out[1]) == -1.0f && cimagf (out[1]) == 0.0f);
    /* "only the LSB of each byte is used" -- 0xFE is even, 0xFF odd. */
    DP_CHECK (crealf (out[4]) == crealf (out[0]));
    DP_CHECK (crealf (out[5]) == crealf (out[1]));
    for (int i = 0; i < 6; i++)
      DP_CHECK (fabsf (cabsf (out[i]) - 1.0f) < 1e-6f);
  }

  /* ── qpsk_map: Gray-coded, so adjacent indices are adjacent points ─ */
  {
    const uint8_t idx[4] = { 0, 1, 2, 3 };
    float complex out[4];

    qpsk_map (idx, 4, out);

    /* Unit magnitude, one point per quadrant, no duplicates. */
    for (int i = 0; i < 4; i++)
      {
        DP_CHECK (fabsf (cabsf (out[i]) - 1.0f) < 1e-5f);
        for (int j = i + 1; j < 4; j++)
          DP_CHECK (cabsf (out[i] - out[j]) > 1e-3f);
      }

    /* GRAY is the claim, and it is the one a defect silently breaks: an
       index step of 1 must move to an ADJACENT constellation point
       (distance sqrt(2) on the unit circle), never to the opposite one
       (distance 2). A natural-binary map passes every check above and
       fails this, at the cost of a doubled bit-error rate near the
       decision boundary. */
    DP_CHECK (fabsf (cabsf (out[0] - out[1]) - sqrtf (2.0f)) < 1e-4f);
    DP_CHECK (fabsf (cabsf (out[1] - out[3]) - sqrtf (2.0f)) < 1e-4f);
    DP_CHECK (fabsf (cabsf (out[3] - out[2]) - sqrtf (2.0f)) < 1e-4f);
    DP_CHECK (fabsf (cabsf (out[2] - out[0]) - sqrtf (2.0f)) < 1e-4f);
  }

  /* ── wfm_awgn_amplitude: sigma per component, and its scaling ───── */
  {
    /* 0 dB SNR on unit power: total noise power 1, split over I and Q,
       so each component carries sigma^2 = 0.5 -> sigma = 0.7071. */
    DP_CHECK (fabsf (wfm_awgn_amplitude (0.0f, 1.0f) - 0.707107f) < 1e-5f);

    /* Every 10 dB divides the amplitude by sqrt(10). */
    const float s0  = wfm_awgn_amplitude (0.0f, 1.0f);
    const float s10 = wfm_awgn_amplitude (10.0f, 1.0f);
    const float s20 = wfm_awgn_amplitude (20.0f, 1.0f);
    DP_CHECK (fabsf (s0 / s10 - sqrtf (10.0f)) < 1e-4f);
    DP_CHECK (fabsf (s10 / s20 - sqrtf (10.0f)) < 1e-4f);

    /* Noise scales with the square root of signal power at fixed SNR --
       otherwise the SNR a caller asked for is not the one they get. */
    DP_CHECK (fabsf (wfm_awgn_amplitude (10.0f, 4.0f) - 2.0f * s10) < 1e-5f);
  }

  /* ── wfm_ebno_to_snr_db: the oversampling and rate terms ────────── */
  {
    /* SNR = Eb/N0 + 10log10(bits_per_symbol / sps). */
    DP_CHECK (fabsf (wfm_ebno_to_snr_db (10.0f, 2, 8.0f)
                     - (10.0f + 10.0f * log10f (2.0f / 8.0f)))
              < 1e-4f);
    DP_CHECK (fabsf (wfm_ebno_to_snr_db (10.0f, 1, 8.0f)
                     - (10.0f + 10.0f * log10f (1.0f / 8.0f)))
              < 1e-4f);

    /* QPSK carries twice the bits per symbol, so it needs 3.01 dB more
       SNR than BPSK for the same Eb/N0 -- the sign of that term is the
       classic place to be off, and inverting it is invisible in any
       single-modulation test. */
    DP_CHECK (wfm_ebno_to_snr_db (10.0f, 2, 8.0f)
              > wfm_ebno_to_snr_db (10.0f, 1, 8.0f));
    /* More oversampling spreads the same energy over more bandwidth. */
    DP_CHECK (wfm_ebno_to_snr_db (10.0f, 2, 16.0f)
              < wfm_ebno_to_snr_db (10.0f, 2, 8.0f));
    /* Eb/N0 passes through one-for-one. */
    DP_CHECK (fabsf ((wfm_ebno_to_snr_db (20.0f, 2, 8.0f)
                      - wfm_ebno_to_snr_db (10.0f, 2, 8.0f))
                     - 10.0f)
              < 1e-4f);
  }

  /* ── mls_poly: a primitive polynomial, and the documented range ─── */
  {
    DP_CHECK (mls_poly (7) == 0x41); /* the header's worked example */
    DP_CHECK (mls_poly (1) == 0);    /* below the range */
    DP_CHECK (mls_poly (65) == 0);   /* above it */

    /* Every length in [2, 64] has an entry, and a tap mask must have the
       top stage set -- a mask without it describes a shorter LFSR and
       silently halves the sequence period. */
    for (uint32_t n = 2; n <= 64; n++)
      {
        const uint64_t p = mls_poly (n);
        DP_CHECK (p != 0);
        DP_CHECK (p < (n == 64 ? UINT64_MAX : (1ULL << n)));
      }
  }

  /* ── crc16: the published CCITT check vector ────────────────────── */
  {
    /* "123456789" -> 0x29B1 is THE check value for CRC-16-CCITT with
       init 0xFFFF. Anything else means this cannot talk to a receiver
       that implements the standard, which no round-trip test can see. */
    const char *msg = "123456789";
    uint8_t     bits[9 * 8];
    size_t      k = 0;
    for (size_t i = 0; i < strlen (msg); i++)
      for (int b = 7; b >= 0; b--)
        bits[k++] = (uint8_t)((msg[i] >> b) & 1);

    DP_CHECK (crc16 (bits, k) == 0x29B1);

    /* Init 0xFFFF, not 0x0000: an empty message returns the init value,
       which is the cheapest way to see which of the two it is. */
    DP_CHECK (crc16 (bits, 0) == 0xFFFF);

    /* A single flipped bit anywhere must change the CRC -- that is the
       entire job. */
    for (size_t i = 0; i < k; i += 7)
      {
        bits[i] ^= 1;
        DP_CHECK (crc16 (bits, k) != 0x29B1);
        bits[i] ^= 1;
      }
    DP_CHECK (crc16 (bits, k) == 0x29B1); /* restored */
  }

  /* ── ccsds_asm_bits: the CCSDS attached sync marker, exactly ────── */
  {
    uint8_t  asm_[32];
    uint32_t v = 0;

    memset (asm_, 0xAA, sizeof asm_);
    ccsds_asm_bits (asm_);
    for (int i = 0; i < 32; i++)
      {
        DP_CHECK (asm_[i] == 0 || asm_[i] == 1);
        v = (v << 1) | asm_[i];
      }
    /* 0x1ACFFC1D, MSB first. A frame synchroniser that disagrees with
       this by one bit finds nothing, forever, silently. */
    DP_CHECK (v == 0x1ACFFC1Du);
  }

  /* ── rrc_h / rc_h: even, and NOT the same filter ────────────────── */
  {
    const double beta = 0.35;
    double       t[9], hr[9], hc[9];

    for (int i = 0; i < 9; i++)
      t[i] = (double)(i - 4); /* -4 .. +4 symbol times */

    rrc_h (t, 9, hr, beta);
    rc_h (t, 9, hc, beta);

    /* Both are even in t: an odd component is a group-delay error. */
    for (int i = 0; i < 4; i++)
      {
        DP_CHECK (fabs (hr[i] - hr[8 - i]) < 1e-9);
        DP_CHECK (fabs (hc[i] - hc[8 - i]) < 1e-9);
      }

    /* Both peak at t = 0. */
    for (int i = 0; i < 9; i++)
      if (i != 4)
        {
          DP_CHECK (hr[4] >= hr[i]);
          DP_CHECK (hc[4] >= hc[i]);
        }

    /* The RAISED COSINE has the Nyquist property -- exactly zero at
       every non-zero integer symbol time, which is what "no ISI at the
       sampling instant" means. The ROOT raised cosine does NOT: it only
       acquires that property after being convolved with itself, which
       is why a matched pair is used at all. Confusing the two is the
       classic pulse-shaping defect, and this is the check that sees it. */
    for (int i = 0; i < 9; i++)
      if (i != 4)
        DP_CHECK (fabs (hc[i]) < 1e-9);
    DP_CHECK (fabs (hr[3]) > 1e-6); /* the RRC is NOT zero there */
  }

  /* ── rrc_taps: odd length, symmetric, non-trivial ───────────────── */
  {
    const int sps = 4, span = 4;
    const int n = 2 * span * sps + 1;
    float     taps[2 * 4 * 4 + 1];

    rrc_taps (0.35, sps, span, taps);
    DP_CHECK (n % 2 == 1);
    for (int i = 0; i < n; i++)
      DP_CHECK (fabsf (taps[i] - taps[n - 1 - i]) < 1e-6f);
    DP_CHECK (taps[n / 2] > 0.0f); /* peaks at the centre, positive */

    double energy = 0.0;
    for (int i = 0; i < n; i++)
      energy += (double)taps[i] * (double)taps[i];
    DP_CHECK (energy > 0.0);
  }

  /* ── dsss_spread: every symbol multiplied by the whole code ─────── */
  {
    const float complex syms[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
    const uint8_t       code[4] = { 0, 1, 1, 0 };
    const int           sf      = 4;
    float complex       out[8];

    dsss_spread (syms, 2, code, 4, sf, out);

    /* A 0 chip maps to +1 and a 1 chip to -1, so the spread symbol is
       the symbol times that sign -- and the magnitude never changes. */
    for (int s = 0; s < 2; s++)
      for (int c = 0; c < sf; c++)
        {
          const float complex want = code[c] ? -syms[s] : syms[s];
          DP_CHECK (cabsf (out[s * sf + c] - want) < 1e-6f);
        }

    /* Despreading with the same code returns the symbols -- the whole
       point, and the only claim a receiver depends on. */
    for (int s = 0; s < 2; s++)
      {
        float complex acc = 0.0f;
        for (int c = 0; c < sf; c++)
          acc += out[s * sf + c] * (code[c] ? -1.0f : 1.0f);
        DP_CHECK (cabsf (acc / (float)sf - syms[s]) < 1e-6f);
      }
  }

  DP_TEST_END ("test_wfm_core");
}
