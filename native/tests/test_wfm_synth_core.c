#include "dp_state_test.h"
#include "dp_test.h"
#include "mpsk/mpsk_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  wfm_synth_state_t *obj
      = wfm_synth_create (0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)wfm_synth_step (obj);

  /* reset */
  wfm_synth_reset (obj);

  /* ── clean (snr >= WFM_SYNTH_SNR_CLEAN) generates no AWGN; baseband no LO ──
   */
  {
    /* clean tone with a freq offset: LO present, no AWGN */
    wfm_synth_state_t *c = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 100.0,
                                             0, 1, 8, 7, 0, 0, 0.0);
    DP_CHECK (c && c->awgn == NULL && c->lo != NULL);
    if (c)
      wfm_synth_destroy (c);

    /* noisy tone: AWGN present */
    wfm_synth_state_t *nz = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 10.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    DP_CHECK (nz && nz->awgn != NULL);
    if (nz)
      wfm_synth_destroy (nz);

    /* baseband (freq 0): no LO */
    wfm_synth_state_t *bb = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    DP_CHECK (bb && bb->lo == NULL && bb->awgn == NULL);
    if (bb)
      wfm_synth_destroy (bb);

    /* noise type always has AWGN, even at high snr */
    wfm_synth_state_t *ns = wfm_synth_create (WFM_SYNTH_NOISE, 1e6, 0.0, 100.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    DP_CHECK (ns && ns->awgn != NULL);
    if (ns)
      wfm_synth_destroy (ns);
  }

  /* ── RRC pulse shaping: step()==steps(), shaping changes the output ────────
   */
  {
    /* a small symmetric low-pass FIR stands in for the RRC taps here */
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    wfm_synth_state_t *rs = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0,
                                              0, 7, 4, 7, 0, 0, 0.0);
    DP_CHECK (rs && rs->fir == NULL);
    DP_CHECK (wfm_synth_set_rrc (rs, taps, 5) == 0);
    DP_CHECK (rs->shaper != NULL && rs->fir == NULL);
    float complex y[256];
    wfm_synth_steps (rs, y, 256);

    /* step() must reproduce steps() bit-for-bit */
    wfm_synth_state_t *rs2 = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0,
                                               0, 7, 4, 7, 0, 0, 0.0);
    wfm_synth_set_rrc (rs2, taps, 5);
    int match = 1;
    for (int i = 0; i < 256; i++)
      if (wfm_synth_step (rs2) != y[i])
        match = 0;
    DP_CHECK (match);

    /* shaping changes the output vs the unshaped (rect) synth */
    wfm_synth_state_t *rect = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0,
                                                100.0, 0, 7, 4, 7, 0, 0, 0.0);
    float complex      r[256];
    wfm_synth_steps (rect, r, 256);
    int differs = 0;
    for (int i = 0; i < 256; i++)
      if (r[i] != y[i])
        differs = 1;
    DP_CHECK (differs);

    /* set_rrc is a no-op on a non-modulated synth, and rejects bad args */
    DP_CHECK (wfm_synth_set_rrc (obj, taps, 5) == 0); /* obj is a tone */
    DP_CHECK (wfm_synth_set_rrc (rs, NULL, 0) == -1);

    wfm_synth_destroy (rs);
    wfm_synth_destroy (rs2);
    wfm_synth_destroy (rect);
  }

  /* ── RRC at a NON-power-of-two sps: the dense-FIR fallback (resamp's branch
   *    select needs a pow-2 phase count), and step()==steps() still holds. */
  {
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    wfm_synth_state_t *fs3     = wfm_synth_create (
        WFM_SYNTH_PN, 1e6, 0.0, 100.0, 0, 7, 3, 7, 0, 0, 0.0); /* sps=3 */
    DP_CHECK (fs3 && wfm_synth_set_rrc (fs3, taps, 5) == 0);
    DP_CHECK (fs3 && fs3->fir != NULL
              && fs3->shaper == NULL); /* dense fallback */
    float complex y3[192];
    wfm_synth_steps (fs3, y3, 192);
    wfm_synth_state_t *fs3b = wfm_synth_create (WFM_SYNTH_PN, 1e6, 0.0, 100.0,
                                                0, 7, 3, 7, 0, 0, 0.0);
    wfm_synth_set_rrc (fs3b, taps, 5);
    int m3 = 1;
    for (int i = 0; i < 192; i++)
      if (wfm_synth_step (fs3b) != y3[i])
        m3 = 0;
    DP_CHECK (m3); /* step()==steps() on the dense-FIR fallback path */
    wfm_synth_destroy (fs3);
    wfm_synth_destroy (fs3b);
  }

  /* ── bits: user pattern, mapping, cycling, step()==steps() ────────────────
   */
  {
    const uint8_t pat[6] = { 1, 0, 1, 1, 0, 0 };
    /* bpsk, sps=2 → 12 samples for one pass; build via steps() */
    wfm_synth_state_t *bs = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                              0, 1, 2, 7, 0, 0, 0.0);
    DP_CHECK (bs && bs->lo == NULL && bs->awgn == NULL && bs->pn == NULL);
    DP_CHECK (wfm_synth_set_bits (bs, pat, 6, 1) == 0); /* 1 = bpsk */
    float complex y[24];
    wfm_synth_steps (bs, y, 24); /* two passes (cycled) */
    /* bpsk: bit 1 -> -1, bit 0 -> +1; symbol centre at each sps-block */
    DP_CHECK (dp_nearf (crealf (y[0]), -1.0f, 1e-5f)); /* bit 1 */
    DP_CHECK (dp_nearf (crealf (y[2]), 1.0f, 1e-5f));  /* bit 0 */
    DP_CHECK (
        dp_nearf (crealf (y[12]), -1.0f, 1e-5f)); /* cycled: bit 1 again */
    int cyc = 1;
    for (int i = 0; i < 12; i++)
      if (y[i] != y[i + 12])
        cyc = 0;
    DP_CHECK (cyc); /* the pattern repeats every 12 samples */

    /* step() must match steps() bit-for-bit */
    wfm_synth_state_t *bs2 = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                               0, 1, 2, 7, 0, 0, 0.0);
    wfm_synth_set_bits (bs2, pat, 6, 1);
    int match = 1;
    for (int i = 0; i < 24; i++)
      if (wfm_synth_step (bs2) != y[i])
        match = 0;
    DP_CHECK (match);

    /* reset rewinds the pattern */
    wfm_synth_reset (bs);
    DP_CHECK (wfm_synth_step (bs) == y[0]);

    /* set_bits is a no-op on a non-bits synth, and rejects bad args */
    DP_CHECK (wfm_synth_set_bits (obj, pat, 6, 1) == 0);  /* obj is a tone */
    DP_CHECK (wfm_synth_set_bits (bs, pat, 6, 9) == -1);  /* bad modulation */
    DP_CHECK (wfm_synth_set_bits (bs, NULL, 0, 1) == -1); /* empty */

    wfm_synth_destroy (bs);
    wfm_synth_destroy (bs2);
  }

  /* ── the bits->symbol map is the LIBRARY's, at every order ───────────────
     The property no agreement between two copies can establish. `set_bits`
     had four inlined copies of this map and the QPSK one disagreed with
     `mpsk_constellation()` on two of its four labels -- same constellation,
     swapped assignment. Every copy agreed with every other, so a consistency
     check passed; what fails is a round trip through the CANONICAL demapper,
     which is the thing that actually scores bit errors (dp_ber_score).
     Sabotage `wfm_synth_bit_symbol`'s label packing and this goes red. */
  {
    const int mods[3] = { 1, 2, 3 }; /* bits/symbol: BPSK, QPSK, 8PSK */
    for (int mi = 0; mi < 3; mi++)
      {
        int bmod = mods[mi];
        int m    = 1 << bmod;
        /* 24 bits divides by 1, 2 and 3, so the pattern is a whole number of
           symbols at every order and the cycling never straddles one. The
           bits COUNT UP so every one of the M labels is exercised -- an
           alternating pattern makes each order emit one or two labels for
           ever, and a label the test never produces is a label the mapping
           can get wrong undetected. */
        uint8_t bp[24];
        for (int i = 0; i < 24; i++)
          bp[i] = (uint8_t)((i / bmod >> (bmod - 1 - i % bmod)) & 1);
        wfm_synth_state_t *b = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0,
                                                 100.0, 0, 1, 1, 7, 0, 0, 0.0);
        DP_CHECK (b != NULL);
        DP_CHECK (wfm_synth_set_bits (b, bp, 24, bmod) == 0);
        {
          size_t        nsym = 24u / (size_t)bmod;
          float complex y[24];
          wfm_synth_steps (b, y, nsym); /* sps = 1: one sample per symbol */
          for (size_t k = 0; k < nsym; k++)
            {
              unsigned g = 0u;
              for (int t = 0; t < bmod; t++) /* MSB-first, as documented */
                g = (g << 1) | (unsigned)bp[k * (size_t)bmod + (size_t)t];
              {
                float complex ahat;
                unsigned      got = mpsk_slice (y[k], m, &ahat);
                DP_CHECK (got == g);
                DP_CHECK (dp_nearf (cabsf (y[k]), 1.0f, 1e-5f));
              }
            }
        }
        wfm_synth_destroy (b);
      }
  }

  /* ── bits + RRC: set_rrc shapes the bit stream, step()==steps() ───────────
   * Regression for the silent no-op where bits accepted RRC but emitted
   * rectangular pulses (set_rrc gated out bits; the bits paths ignored fir).
   */
  {
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    const uint8_t      pat[6]  = { 1, 0, 1, 1, 0, 0 };
    wfm_synth_state_t *bs = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                              0, 1, 4, 7, 0, 0, 0.0);
    DP_CHECK (wfm_synth_set_bits (bs, pat, 6, 1) == 0); /* bpsk */
    DP_CHECK (wfm_synth_set_rrc (bs, taps, 5) == 0); /* now accepted on bits */
    DP_CHECK (bs->shaper != NULL && bs->fir == NULL);
    float complex y[256];
    wfm_synth_steps (bs, y, 256);

    /* step() must reproduce steps() bit-for-bit (chunk-invariant FIR) */
    wfm_synth_state_t *bs2 = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                               0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_bits (bs2, pat, 6, 1);
    wfm_synth_set_rrc (bs2, taps, 5);
    int match = 1;
    for (int i = 0; i < 256; i++)
      if (wfm_synth_step (bs2) != y[i])
        match = 0;
    DP_CHECK (match);

    /* shaping changes the output vs the unshaped (rect) bits synth */
    wfm_synth_state_t *rect = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0,
                                                100.0, 0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_bits (rect, pat, 6, 1);
    float complex r[256];
    wfm_synth_steps (rect, r, 256);
    int differs = 0;
    for (int i = 0; i < 256; i++)
      if (r[i] != y[i])
        differs = 1;
    DP_CHECK (differs);

    wfm_synth_destroy (bs);
    wfm_synth_destroy (bs2);
    wfm_synth_destroy (rect);
  }

  /* ── symbols: user complex-symbol stream, mapping, cycling, step()==steps()
   * ─
   */
  {
    /* Four-point constellation; the symbol IS the output (no bit mapping). */
    const float complex syms[4] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I,
                                    -1.0f + 0.0f * I, 0.0f - 1.0f * I };
    wfm_synth_state_t  *ss = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                               100.0, 0, 1, 2, 7, 0, 0, 0.0);
    DP_CHECK (ss && ss->lo == NULL && ss->awgn == NULL && ss->pn == NULL);
    DP_CHECK (wfm_synth_set_symbols (ss, syms, 4) == 0);
    float complex y[16];
    wfm_synth_steps (ss, y, 16); /* 4 syms * 2 sps = 8/pass → two passes */
    /* symbol centre at each sps-block equals the symbol itself */
    DP_CHECK (dp_cnearf (y[0], syms[0], 1e-5f));
    DP_CHECK (dp_cnearf (y[2], syms[1], 1e-5f));
    DP_CHECK (dp_cnearf (y[4], syms[2], 1e-5f));
    DP_CHECK (dp_cnearf (y[6], syms[3], 1e-5f));
    DP_CHECK (dp_cnearf (y[8], syms[0], 1e-5f)); /* cycled */
    int cyc = 1;
    for (int i = 0; i < 8; i++)
      if (y[i] != y[i + 8])
        cyc = 0;
    DP_CHECK (cyc); /* the stream repeats every 8 samples */

    /* step() must match steps() bit-for-bit */
    wfm_synth_state_t *ss2 = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                               100.0, 0, 1, 2, 7, 0, 0, 0.0);
    wfm_synth_set_symbols (ss2, syms, 4);
    int match = 1;
    for (int i = 0; i < 16; i++)
      if (wfm_synth_step (ss2) != y[i])
        match = 0;
    DP_CHECK (match);

    /* reset rewinds the stream */
    wfm_synth_reset (ss);
    DP_CHECK (wfm_synth_step (ss) == y[0]);

    /* set_symbols is a no-op on a non-symbols synth, and rejects bad args */
    DP_CHECK (wfm_synth_set_symbols (obj, syms, 4) == 0); /* obj is a tone */
    DP_CHECK (wfm_synth_set_symbols (ss, NULL, 0) == -1);

    wfm_synth_destroy (ss);
    wfm_synth_destroy (ss2);
  }

  /* ── symbols + RRC: set_rrc shapes the symbol stream, step()==steps() ──────
   */
  {
    const float         taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    const float complex syms[3]
        = { 1.0f + 1.0f * I, -1.0f + 1.0f * I, 1.0f - 1.0f * I };
    wfm_synth_state_t *ss = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                              100.0, 0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_symbols (ss, syms, 3);
    DP_CHECK (wfm_synth_set_rrc (ss, taps, 5) == 0); /* accepted on symbols */
    DP_CHECK (ss->shaper != NULL && ss->fir == NULL);
    float complex y[192];
    wfm_synth_steps (ss, y, 192);

    wfm_synth_state_t *ss2 = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                               100.0, 0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_symbols (ss2, syms, 3);
    wfm_synth_set_rrc (ss2, taps, 5);
    int match = 1;
    for (int i = 0; i < 192; i++)
      if (wfm_synth_step (ss2) != y[i])
        match = 0;
    DP_CHECK (match);

    wfm_synth_state_t *rect = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                                100.0, 0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_symbols (rect, syms, 3);
    float complex r[192];
    wfm_synth_steps (rect, r, 192);
    int differs = 0;
    for (int i = 0; i < 192; i++)
      if (r[i] != y[i])
        differs = 1;
    DP_CHECK (differs);

    wfm_synth_destroy (ss);
    wfm_synth_destroy (ss2);
    wfm_synth_destroy (rect);
  }

  /* ── chirp (LFM): linear sweep, phase-continuous, byte-identical paths ────
   */
  {
    /* A clean chirp builds neither a static LO (it synthesises its own swept
     * carrier) nor AWGN; an up-chirp sweeps f_start→f_end over its span. */
    const double       fs = 1e6, f0 = 1e5, f1 = 3e5;
    const size_t       N  = 4096;
    wfm_synth_state_t *cu = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f0, 100.0,
                                              0, 1, 8, 7, 0, 0, f1);
    DP_CHECK (cu && cu->lo == NULL && cu->awgn == NULL);
    wfm_synth_set_chirp_span (cu, N);

    float complex *y = malloc (N * sizeof *y);
    DP_CHECK (y != NULL);
    wfm_synth_steps (cu, y, N);

    /* unit magnitude everywhere (a pure FM tone has constant envelope) */
    DP_CHECK (dp_nearf (cabsf (y[0]), 1.0f, 1e-4f));
    DP_CHECK (dp_nearf (cabsf (y[N / 2]), 1.0f, 1e-4f));
    DP_CHECK (dp_nearf (cabsf (y[N - 1]), 1.0f, 1e-4f));

    /* instantaneous frequency rises: estimate it from the phase increment
     * (cycles/sample) at the start vs. the end of the sweep. */
    double w_lo = carg (y[1] * conjf (y[0])) / 6.283185307179586; /* ≈ f0/fs */
    double w_hi
        = carg (y[N - 1] * conjf (y[N - 2])) / 6.283185307179586; /* ≈ f1/fs */
    DP_CHECK (dp_nearf (w_lo, f0 / fs, 2e-3f));
    DP_CHECK (dp_nearf (w_hi, f1 / fs, 2e-3f));

    /* step() and steps() must agree bit-for-bit (the #67 lesson). */
    wfm_synth_state_t *cs = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f0, 100.0,
                                              0, 1, 8, 7, 0, 0, f1);
    wfm_synth_set_chirp_span (cs, N);
    int step_match = 1;
    for (size_t i = 0; i < N; i++)
      if (wfm_synth_step (cs) != y[i])
        step_match = 0;
    DP_CHECK (step_match);

    /* reset rewinds the sweep to sample 0 (reproducible). */
    float complex y0 = y[0];
    wfm_synth_reset (cu);
    DP_CHECK (wfm_synth_step (cu) == y0);

    /* down-chirp: f_end < f_start sweeps the other way (high → low). */
    float complex *d = malloc (N * sizeof *d);
    DP_CHECK (d != NULL);
    wfm_synth_state_t *cd = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f1, 100.0,
                                              0, 1, 8, 7, 0, 0, f0);
    wfm_synth_set_chirp_span (cd, N);
    wfm_synth_steps (cd, d, N);
    double wd_lo = carg (d[1] * conjf (d[0])) / 6.283185307179586;
    double wd_hi = carg (d[N - 1] * conjf (d[N - 2])) / 6.283185307179586;
    DP_CHECK (dp_nearf (wd_lo, f1 / fs, 2e-3f)); /* starts high */
    DP_CHECK (dp_nearf (wd_hi, f0 / fs, 2e-3f)); /* ends low   */

    free (d);
    free (y);
    wfm_synth_destroy (cu);
    wfm_synth_destroy (cs);
    wfm_synth_destroy (cd);
  }

  wfm_synth_destroy (obj);
  /* serializable state — running scalars + present children
   * (presence-flagged). */
  {
    float complex      out[256];
    wfm_synth_state_t *a
        = wfm_synth_create (0, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
    wfm_synth_state_t *b
        = wfm_synth_create (0, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
    DP_CHECK (a != NULL && b != NULL);
    wfm_synth_steps (a, out, 256);
    DP_STATE_ROUNDTRIP_TEST (wfm_synth, a, b);
    DP_CHECK (b->sym_pos == a->sym_pos && b->chirp_n == a->chirp_n);
    DP_CHECK (b->cur_re == a->cur_re && b->bit_idx == a->bit_idx);
    wfm_synth_destroy (a);
    wfm_synth_destroy (b);
  }

  /* symbols serialization: a mid-stream split resumes bit-exact, and
   * sym_read_idx survives the round-trip. */
  {
    const float complex syms[5]
        = { 1 + 0 * I, 0 + 1 * I, -1 + 0 * I, 0 - 1 * I, 1 + 1 * I };
    float complex      ref[128], part[40], cont[88];
    wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                             100.0, 0, 1, 3, 7, 0, 0, 0.0);
    wfm_synth_state_t *b = wfm_synth_create (WFM_SYNTH_SYMBOLS, 1e6, 0.0,
                                             100.0, 0, 1, 3, 7, 0, 0, 0.0);
    wfm_synth_set_symbols (a, syms, 5);
    wfm_synth_set_symbols (b, syms, 5);
    wfm_synth_steps (a, ref, 128); /* full reference */
    wfm_synth_reset (a);
    wfm_synth_steps (a, part, 40); /* feed a partway → sym_read_idx advances */
    size_t nb   = wfm_synth_state_bytes (a);
    void  *blob = malloc (nb);
    wfm_synth_get_state (a, blob);
    DP_CHECK (wfm_synth_set_state (b, blob) == 0);
    DP_CHECK (b->sym_read_idx == a->sym_read_idx);
    wfm_synth_steps (b, cont, 88); /* resume from the split */
    int ok = 1;
    for (int i = 0; i < 40; i++)
      if (part[i] != ref[i])
        ok = 0;
    for (int i = 0; i < 88; i++)
      if (cont[i] != ref[40 + i])
        ok = 0;
    DP_CHECK (ok); /* part ++ cont == ref, bit-for-bit across the split */
    /* envelope reject: clobber the magic */
    ((uint8_t *)blob)[0] ^= 0xFFu;
    DP_CHECK (wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
    free (blob);
    wfm_synth_destroy (a);
    wfm_synth_destroy (b);
  }

  /* dsss: set_dsss assembles the two-code burst (preamble + spread frame)
   * into the bits machinery, a mid-burst split resumes bit-exact (noisy, so
   * the AWGN child state is exercised too), and bad geometry is rejected. */
  {
    const uint8_t acq[8]   = { 1, 0, 1, 1, 0, 0, 1, 0 };
    const uint8_t dcode[4] = { 0, 1, 1, 0 };
    const uint8_t sync[2]  = { 1, 0 };
    const uint8_t pay[5]   = { 1, 0, 0, 1, 1 };
    /* 8*3 + (2+5+16)*4 = 116 chips × sps 2 = 232 samples per burst pass */
    float complex      ref[232], part[100], cont[132];
    wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_DSSS, 1e6, 0.0, 3.0, 1,
                                             9, 2, 7, 0, 0, 0.0);
    wfm_synth_state_t *b = wfm_synth_create (WFM_SYNTH_DSSS, 1e6, 0.0, 3.0, 1,
                                             9, 2, 7, 0, 0, 0.0);
    DP_CHECK (a != NULL && b != NULL);
    DP_CHECK (wfm_synth_set_dsss (a, acq, 8, 3, dcode, 4, sync, 2, pay, 5, 1)
              == 0);
    DP_CHECK (wfm_synth_set_dsss (b, acq, 8, 3, dcode, 4, sync, 2, pay, 5, 1)
              == 0);
    DP_CHECK (a->n_bits == 116 && a->bit_mod == 1);
    /* the head of the pattern is the unmodulated tiled preamble */
    for (int i = 0; i < 8; i++)
      DP_CHECK (a->bits[i] == acq[i] && a->bits[8 + i] == acq[i]);
    wfm_synth_steps (a, ref, 232);
    wfm_synth_reset (a);
    wfm_synth_steps (a, part, 100); /* split mid-burst */
    size_t nb   = wfm_synth_state_bytes (a);
    void  *blob = malloc (nb);
    wfm_synth_get_state (a, blob);
    DP_CHECK (wfm_synth_set_state (b, blob) == 0);
    wfm_synth_steps (b, cont, 132);
    int ok = 1;
    for (int i = 0; i < 100; i++)
      if (part[i] != ref[i])
        ok = 0;
    for (int i = 0; i < 132; i++)
      if (cont[i] != ref[100 + i])
        ok = 0;
    DP_CHECK (ok); /* part ++ cont == ref across the split, noise included */
    ((uint8_t *)blob)[0] ^= 0xFFu; /* envelope reject */
    DP_CHECK (wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
    free (blob);
    /* geometry rejects: frame bits without a data code; empty burst;
     * no-op on a non-dsss synth. */
    DP_CHECK (wfm_synth_set_dsss (a, acq, 8, 3, NULL, 0, sync, 2, pay, 5, 1)
              == -1);
    DP_CHECK (wfm_synth_set_dsss (a, NULL, 0, 0, dcode, 4, NULL, 0, NULL, 0, 0)
              == -1);
    wfm_synth_state_t *tn = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0,
                                              0, 1, 1, 7, 0, 0, 0.0);
    DP_CHECK (wfm_synth_set_dsss (tn, acq, 8, 3, dcode, 4, sync, 2, pay, 5, 1)
              == 0); /* no-op for other types */
    wfm_synth_destroy (tn);
    wfm_synth_destroy (a);
    wfm_synth_destroy (b);
  }

  /* continuous asynchronous dsss: the lazy per-sample generator (no
   * materialised burst). Covers all three data modes, the step()==steps()
   * byte-identity the shared kernel guarantees, a mid-stream resume that
   * carries the running chip/symbol clocks (and the PN child for prbs), and
   * the geometry rejects. chips_per_symbol = (fs/spc)/symbol_rate is
   * deliberately non-integer ((1e6/2)/2100 = 238.095), the asynchronicity. */
  {
    const size_t sf = 31, spc = 2;
    const double fs = 1e6, cps = (fs / (double)spc) / 2100.0;
    uint8_t      code[31];
    for (size_t i = 0; i < sf; i++)
      code[i] = (uint8_t)((i * 7 + 1) & 1u);
    const uint8_t pay[5] = { 1, 0, 1, 1, 0 };
    const int     modes[3]
        = { WFM_DSSS_DATA_NONE, WFM_DSSS_DATA_BITS, WFM_DSSS_DATA_PRBS };

    for (int mi = 0; mi < 3; mi++)
      {
        int                mode = modes[mi];
        wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_DSSS, fs, 0.0, 3.0,
                                                 1, 9, (int)spc, 7, 0, 0, 0.0);
        wfm_synth_state_t *b = wfm_synth_create (WFM_SYNTH_DSSS, fs, 0.0, 3.0,
                                                 1, 9, (int)spc, 7, 0, 0, 0.0);
        DP_CHECK (a != NULL && b != NULL);
        const uint8_t *d  = (mode == WFM_DSSS_DATA_BITS) ? pay : NULL;
        size_t         nd = (mode == WFM_DSSS_DATA_BITS) ? 5 : 0;
        DP_CHECK (wfm_synth_set_dsss_cont (a, code, sf, cps, mode, d, nd)
                  == 0);
        DP_CHECK (wfm_synth_set_dsss_cont (b, code, sf, cps, mode, d, nd)
                  == 0);

        /* step() and steps() must agree bit-for-bit (shared chip kernel). */
        float complex blk[600];
        wfm_synth_steps (a, blk, 600);
        int idn = 1;
        for (int i = 0; i < 600; i++)
          if (wfm_synth_step (b) != blk[i])
            {
              idn = 0;
              break;
            }
        DP_CHECK (idn); /* step()==steps() for this data mode */

        /* mid-stream resume: a's state -> a fresh c, then both run on and the
         * outputs coincide (noisy, so the AWGN child rides too; prbs also
         * carries the PN child across the split). */
        wfm_synth_reset (a);
        float complex ref[600], part[250], cont[350];
        wfm_synth_steps (a, ref, 600);
        wfm_synth_reset (a);
        wfm_synth_steps (a, part, 250);
        size_t nb   = wfm_synth_state_bytes (a);
        void  *blob = malloc (nb);
        wfm_synth_get_state (a, blob);
        wfm_synth_state_t *c = wfm_synth_create (WFM_SYNTH_DSSS, fs, 0.0, 3.0,
                                                 1, 9, (int)spc, 7, 0, 0, 0.0);
        DP_CHECK (wfm_synth_set_dsss_cont (c, code, sf, cps, mode, d, nd)
                  == 0);
        DP_CHECK (wfm_synth_set_state (c, blob) == 0);
        wfm_synth_steps (c, cont, 350);
        int ok = 1;
        for (int i = 0; i < 250; i++)
          if (part[i] != ref[i])
            ok = 0;
        for (int i = 0; i < 350; i++)
          if (cont[i] != ref[250 + i])
            ok = 0;
        DP_CHECK (ok); /* part ++ cont == ref across the split */
        ((uint8_t *)blob)[0] ^= 0xFFu; /* envelope reject */
        DP_CHECK (wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
        free (blob);
        wfm_synth_destroy (a);
        wfm_synth_destroy (b);
        wfm_synth_destroy (c);
      }

    /* code-only emits the pure code at nominal polarity (+code): chip k,
     * sampled at its chip-start, is code[k] ? -1 : +1. */
    {
      wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_DSSS, fs, 0.0, 100.0,
                                               0, 9, (int)spc, 7, 0, 0, 0.0);
      DP_CHECK (wfm_synth_set_dsss_cont (a, code, sf, cps, WFM_DSSS_DATA_NONE,
                                         NULL, 0)
                == 0);
      /* heap, not a `blk[sf * spc]` VLA: sf/spc are const size_t, not integer
       * constant expressions, so the array would be a (folded) VLA -- clang
       * warns -Wgnu-folding-constant. */
      float complex *blk = malloc (sf * spc * sizeof *blk);
      wfm_synth_steps (a, blk, sf * spc);
      int ok = 1;
      for (size_t k = 0; k < sf; k++)
        {
          float want = code[k] ? -1.0f : 1.0f;
          if (crealf (blk[k * spc]) != want)
            ok = 0;
        }
      DP_CHECK (ok); /* code-only == +code */
      free (blk);
      wfm_synth_destroy (a);
    }

    /* geometry rejects: no code; cps < 1; prbs with a bad pn_length (no PN);
     * BITS with no payload; no-op on a non-dsss synth. */
    {
      wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_DSSS, fs, 0.0, 100.0,
                                               0, 9, (int)spc, 7, 0, 0, 0.0);
      DP_CHECK (wfm_synth_set_dsss_cont (a, NULL, 0, cps, WFM_DSSS_DATA_NONE,
                                         NULL, 0)
                == -1);
      DP_CHECK (wfm_synth_set_dsss_cont (a, code, sf, 0.5, WFM_DSSS_DATA_NONE,
                                         NULL, 0)
                == -1); /* cps < 1 */
      DP_CHECK (wfm_synth_set_dsss_cont (a, code, sf, cps, WFM_DSSS_DATA_BITS,
                                         NULL, 0)
                == -1); /* BITS without payload */
      wfm_synth_destroy (a);
      /* prbs with an out-of-table pn_length leaves the PN NULL -> reject. */
      wfm_synth_state_t *bad = wfm_synth_create (
          WFM_SYNTH_DSSS, fs, 0.0, 100.0, 0, 9, (int)spc, 99, 0, 0, 0.0);
      DP_CHECK (bad
                != NULL); /* burst dsss still builds over a bad pn_length */
      DP_CHECK (bad->pn == NULL);
      DP_CHECK (wfm_synth_set_dsss_cont (bad, code, sf, cps,
                                         WFM_DSSS_DATA_PRBS, NULL, 0)
                == -1);
      wfm_synth_destroy (bad);
      wfm_synth_state_t *tn = wfm_synth_create (WFM_SYNTH_TONE, fs, 0.0, 100.0,
                                                0, 1, 1, 7, 0, 0, 0.0);
      DP_CHECK (wfm_synth_set_dsss_cont (tn, code, sf, cps, WFM_DSSS_DATA_NONE,
                                         NULL, 0)
                == 0); /* no-op for other types */
      wfm_synth_destroy (tn);
    }
  }

  /* ── polyphase shaper serialization: the resamp shaper child + the `primed`
   *    latency flag resume bit-for-bit mid-stream, alongside the lo/awgn/pn
   *    children (pn carrier + noise + RRC shaping at a power-of-two sps). */
  {
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    wfm_synth_state_t *a = wfm_synth_create (WFM_SYNTH_PN, 1e6, 1000.0, 5.0, 1,
                                             7, 4, 7, 0, 0, 0.0);
    DP_CHECK (a && wfm_synth_set_rrc (a, taps, 5) == 0);
    DP_CHECK (a && a->shaper != NULL && a->lo != NULL && a->awgn != NULL
              && a->pn != NULL);
    float complex ref[512], part[200], cont[312];
    wfm_synth_steps (a, ref, 512);  /* uninterrupted reference */
    wfm_synth_reset (a);            /* re-arm priming + rewind children */
    wfm_synth_steps (a, part, 200); /* first leg, past the sps priming */
    size_t nb   = wfm_synth_state_bytes (a);
    void  *blob = malloc (nb);
    wfm_synth_get_state (a, blob);
    wfm_synth_state_t *c = wfm_synth_create (WFM_SYNTH_PN, 1e6, 1000.0, 5.0, 1,
                                             7, 4, 7, 0, 0, 0.0);
    DP_CHECK (c && wfm_synth_set_rrc (c, taps, 5) == 0);
    DP_CHECK (wfm_synth_set_state (c, blob) == 0);
    wfm_synth_steps (c, cont, 312); /* resume from the handed-off state */
    int ok = 1;
    for (int i = 0; i < 200; i++)
      if (part[i] != ref[i])
        ok = 0;
    for (int i = 0; i < 312; i++)
      if (cont[i] != ref[200 + i])
        ok = 0;
    DP_CHECK (
        ok); /* shaper + primed resume: part ++ cont == ref bit-for-bit */
    ((uint8_t *)blob)[0] ^= 0xFFu; /* envelope reject leaves c untouched */
    DP_CHECK (wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
    free (blob);
    wfm_synth_destroy (a);
    wfm_synth_destroy (c);
  }

  /* ── §A  wfm_synth_snr_over_fs / wfm_synth_bps: the ONE SNR conversion ────
   *
   * The header calls wfm_synth_snr_over_fs "the one place this arithmetic
   * lives" and says getting it wrong is SILENT — the waveform is still a
   * waveform, at an SNR nobody asked for. Both it and wfm_synth_bps had ZERO
   * mentions in any C test in the tree: the only thing pinning either was
   * test_wfm_compose.c's noise-power table, which reaches them through
   * wfm_snr_over_fs() and only for the six segment-level cases it lists.
   *
   * The expected values below are LITERALS derived by hand from the formula
   * in the doc comment, never by calling the function — the same rule the
   * compose test learned. 10*log10(2) = 3.010299956639812 and
   * 10*log10(8) = 9.030899869919435; the rest is addition.
   */
  {
    const double fs_db = 12.0;
    /* mode 1 (over fs): the figure passes through untouched, whatever the
       span or the bits-per-symbol — those terms belong to the other modes. */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (1, 1, 8.0, fs_db), 12.0, 1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (1, 2, 8.0, fs_db), 12.0, 1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (1, 2, 1.0, fs_db), 12.0, 1e-12);
    /* mode 3 (Es/No): snr − 10log10(span). The header's own worked example
       says 12 dB at 8 samples/symbol is 2.969 dB over fs. */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (3, 1, 8.0, fs_db), 2.969100130080565,
                   1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (3, 2, 8.0, fs_db), 2.969100130080565,
                   1e-12); /* bps is NOT in the Es branch */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (3, 1, 5.0, 3.0), -3.989700043360187,
                   1e-12);
    /* mode 2 (Eb/No): snr + 10log10(bps) − 10log10(span). The same figure
       read as Eb/No on QPSK is exactly 10log10(2) = 3.010 dB hotter. */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (2, 2, 8.0, fs_db), 5.979400086720377,
                   1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (2, 1, 8.0, fs_db), 2.969100130080565,
                   1e-12); /* bps 1: Eb/No == Es/No */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (2, 1, 1.0, 7.0), 7.0, 1e-12);
    /* the span guard: a non-positive span is treated as 1 sample, so the
       conversion degenerates to the identity rather than to −inf/NaN. */
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (3, 1, 0.0, fs_db), 12.0, 1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (3, 1, -5.0, fs_db), 12.0, 1e-12);
    DP_CHECK_NEAR (wfm_synth_snr_over_fs (2, 2, 0.0, fs_db),
                   15.010299956639812, 1e-12);
    /* bps: QPSK carries two bits, EVERY other type carries one — including
       dsss, which is what makes ebno == esno for a DSSS source. Asserted
       over the whole enum, so a tenth type cannot be added without a
       deliberate answer here. */
    for (int t = WFM_SYNTH_TONE; t <= WFM_SYNTH_DSSS; t++)
      DP_CHECK (wfm_synth_bps (t) == (t == WFM_SYNTH_QPSK ? 2 : 1));
  }

  /* ── §B  create()'s `auto` resolves DSSS to fs, and that is DELIBERATE ────
   *
   * The generator and the composer resolve `auto` DIFFERENTLY for dsss: the
   * composer says Es/No because only it knows the spreading factor, while
   * create() says fs because the codes attach afterwards. The divergence is
   * documented in both files and asserted in neither, so nothing would have
   * noticed the two agreeing (which would silently move every bundled DSSS
   * source's noise by 10log10(sf·sps)).
   *
   * A codeless dsss synth holds cur_re = 0, so its output IS the noise term
   * and the measured power is the noise power directly. At snr = 9 dB the
   * two candidate answers are far apart: fs gives 10^(−0.9) = 0.125893,
   * while Es/No at sps = 8 would give 10^(−(9−9.0309)/10) = 1.00714.
   */
  {
    const size_t       n  = 200000;
    wfm_synth_state_t *ds = wfm_synth_create (WFM_SYNTH_DSSS, 1e6, 0.0, 9.0, 0,
                                              11, 8, 9, 0, 0, 0.0);
    DP_REQUIRE_MSG (ds != NULL, "auto/dsss: create");
    float complex *y = malloc (n * sizeof *y);
    DP_REQUIRE_MSG (y != NULL, "auto/dsss: alloc");
    wfm_synth_steps (ds, y, n);
    double p = 0.0;
    for (size_t i = 0; i < n; i++)
      p += (double)(crealf (y[i]) * crealf (y[i])
                    + cimagf (y[i]) * cimagf (y[i]));
    p /= (double)n;
    DP_CHECK_NEAR (p, 0.125893, 0.005); /* fs, NOT the 1.007 of Es/No */
    free (y);
    wfm_synth_destroy (ds);
  }

  /* ── §C  set_dsss_chips: the install path a wfm_frame_desc_t burst takes ──
   *
   * A public entry point with zero mentions anywhere in the tree. It is the
   * spreading half of set_dsss(), split out so a caller who assembled the
   * frame himself installs it through the SAME path — so the properties that
   * matter are the BPSK mapping, that the chips are copied (the header says
   * "@p chips stays the caller's"), and the rejects.
   */
  {
    uint8_t            chips[6] = { 1, 0, 0, 1, 1, 0 };
    wfm_synth_state_t *dc = wfm_synth_create (WFM_SYNTH_DSSS, 1e6, 0.0, 100.0,
                                              1, 3, 2, 9, 0, 0, 0.0);
    DP_REQUIRE_MSG (dc != NULL, "set_dsss_chips: create");
    DP_CHECK (wfm_synth_set_dsss_chips (dc, chips, 6) == 0);
    /* chip 1 → −1, chip 0 → +1, each held for the create-time sps (2). */
    float complex y[12];
    wfm_synth_steps (dc, y, 12);
    for (size_t k = 0; k < 6; k++)
      {
        float want = chips[k] ? -1.0f : 1.0f;
        DP_CHECK_NEAR (crealf (y[2 * k]), want, 1e-6);
        DP_CHECK_NEAR (crealf (y[2 * k + 1]), want, 1e-6);
        DP_CHECK_NEAR (cimagf (y[2 * k]), 0.0f, 1e-6);
      }
    /* COPIED, not borrowed: mutating the caller's array after the call must
       not change a single output sample. A borrow would sail through every
       assertion above and only fail once the caller's buffer went away. */
    wfm_synth_reset (dc);
    for (size_t k = 0; k < 6; k++)
      chips[k] ^= 1u; /* invert every chip in the CALLER's array */
    float complex y2[12];
    wfm_synth_steps (dc, y2, 12);
    int same = 1;
    for (size_t i = 0; i < 12; i++)
      if (y[i] != y2[i])
        same = 0;
    DP_CHECK (same); /* the synth kept its own copy */
    /* rejects, and the documented no-op for every other type */
    DP_CHECK (wfm_synth_set_dsss_chips (dc, NULL, 6) == -1);
    DP_CHECK (wfm_synth_set_dsss_chips (dc, chips, 0) == -1);
    wfm_synth_state_t *tn2 = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0,
                                               1, 3, 2, 9, 0, 0, 0.0);
    DP_REQUIRE_MSG (tn2 != NULL, "set_dsss_chips: tone create");
    DP_CHECK (wfm_synth_set_dsss_chips (tn2, chips, 6) == 0); /* no-op */
    DP_CHECK (tn2->bits == NULL); /* and it really did nothing */
    wfm_synth_destroy (tn2);
    wfm_synth_destroy (dc);
  }

  /* ── §D  reseed_noise: NEW noise, and the signal must not move ───────────
   *
   * The composer calls this to give each repeat of a segment a fresh noise
   * realization while the underlying waveform stays bit-identical. Untested
   * anywhere. The falsifiable half is the SECOND clause: if the reseed
   * reached the PN as well, the symbol sequence would change too, and the
   * two runs would differ by SYMBOL scale (≈2) instead of noise scale. At
   * 40 dB over fs the noise is σ ≈ 0.007 per component, so the gap between
   * "noise moved" and "signal moved" is two orders of magnitude wide.
   */
  {
    const size_t   n = 4000;
    float complex *a = malloc (n * sizeof *a);
    float complex *b = malloc (n * sizeof *b);
    DP_REQUIRE_MSG (a && b, "reseed: alloc");
    wfm_synth_state_t *ra = wfm_synth_create (WFM_SYNTH_BPSK, 1e6, 0.0, 40.0,
                                              1, 7, 8, 7, 0, 0, 0.0);
    wfm_synth_state_t *rb = wfm_synth_create (WFM_SYNTH_BPSK, 1e6, 0.0, 40.0,
                                              1, 7, 8, 7, 0, 0, 0.0);
    DP_REQUIRE_MSG (ra && rb, "reseed: create");
    /* Advance BOTH first, so the PN is somewhere other than its initial
       register. Reseeding a synth whose LFSR has never stepped cannot show
       the signal moving — a sabotage that rewound the PN inside
       reseed_noise() passed this section until the warm-up was added. */
    wfm_synth_steps (ra, a, 500);
    wfm_synth_steps (rb, b, 500);
    wfm_synth_steps (ra, a, n);
    wfm_synth_reseed_noise (rb, 999u); /* only the noise is reseeded */
    wfm_synth_steps (rb, b, n);
    double dmax = 0.0, amax = 0.0;
    for (size_t i = 0; i < n; i++)
      {
        double d = cabs ((double complex)a[i] - (double complex)b[i]);
        if (d > dmax)
          dmax = d;
        double m = cabs ((double complex)a[i]);
        if (m > amax)
          amax = m;
      }
    DP_CHECK (dmax > 1e-4); /* the reseed DID change the noise */
    DP_CHECK (dmax < 0.5);  /* but the symbols did not move (that is ≈2) */
    DP_CHECK (amax > 0.5);  /* precondition: there is a signal to move */
    /* a clean synth has no AWGN child, so reseeding it is a no-op — the
       output stays bit-identical rather than silently gaining noise. */
    wfm_synth_state_t *cl = wfm_synth_create (WFM_SYNTH_BPSK, 1e6, 0.0, 100.0,
                                              1, 7, 8, 7, 0, 0, 0.0);
    DP_REQUIRE_MSG (cl != NULL, "reseed/clean: create");
    float complex c1[64], c2[64];
    wfm_synth_steps (cl, c1, 64);
    wfm_synth_reset (cl);
    wfm_synth_reseed_noise (cl, 12345u);
    wfm_synth_steps (cl, c2, 64);
    int clean_same = 1;
    for (size_t i = 0; i < 64; i++)
      if (c1[i] != c2[i])
        clean_same = 0;
    DP_CHECK (clean_same);
    wfm_synth_reseed_noise (NULL, 1u); /* documented NULL no-op */
    wfm_synth_destroy (cl);
    wfm_synth_destroy (ra);
    wfm_synth_destroy (rb);
    free (a);
    free (b);
  }

  /* ── §E  noise_steps: a gap is the SEAMLESS continuation of the on-time ──
   *
   * The composer renders a segment's off-time through this, and the header's
   * claim is precise: it draws the identical AWGN sub-sequences the on-time
   * path would have drawn, because it chunks its awgn_generate calls exactly
   * as wfm_synth_steps does (the vectorized awgn path is NOT block-boundary
   * invariant, so the call PATTERN is the thing that has to match, not just
   * the sample count). The object's own test never touched it — it was
   * reachable only through test_wfm_compose.c.
   *
   * type=noise holds cur_re = 0, so the output IS the noise term and the two
   * paths can be compared bit-for-bit with no signal to subtract. The gap
   * length crosses the internal CH = 2048 chunk boundary on purpose: at 3000
   * samples a mismatched chunking diverges from sample 2048 onward, and at
   * any length below 2048 it could not.
   */
  {
    const size_t   n1 = 1000, n2 = 3000;
    float complex *g  = malloc (n2 * sizeof *g);
    float complex *on = malloc (n2 * sizeof *on);
    float complex *sk = malloc (n1 * sizeof *sk);
    DP_REQUIRE_MSG (g && on && sk, "noise_steps: alloc");
    wfm_synth_state_t *na = wfm_synth_create (WFM_SYNTH_NOISE, 1e6, 0.0, 100.0,
                                              1, 4, 8, 7, 0, 0, 0.0);
    wfm_synth_state_t *nb = wfm_synth_create (WFM_SYNTH_NOISE, 1e6, 0.0, 100.0,
                                              1, 4, 8, 7, 0, 0, 0.0);
    DP_REQUIRE_MSG (na && nb, "noise_steps: create");
    wfm_synth_steps (na, sk, n1); /* both advance identically first */
    wfm_synth_steps (nb, sk, n1);
    wfm_synth_noise_steps (na, g, n2); /* the gap */
    wfm_synth_steps (nb, on, n2);      /* the on-time it must match */
    int    seam      = 1;
    size_t first_bad = n2;
    for (size_t i = 0; i < n2; i++)
      if (g[i] != on[i])
        {
          if (seam)
            first_bad = i;
          seam = 0;
        }
    DP_CHECK_MSG (seam, "gap noise continues the on-time RNG stream");
    if (!seam)
      fprintf (stderr, "  first divergence at sample %zu of %zu\n", first_bad,
               n2);
    /* precondition: the stream is not all zeros, or the comparison above
       would hold for a noise_steps that generated nothing at all. */
    double e = 0.0;
    for (size_t i = 0; i < n2; i++)
      e += (double)(crealf (g[i]) * crealf (g[i]));
    DP_CHECK (e > 0.0);
    /* a clean synth has no AWGN child: exact zeros, nothing advanced. */
    wfm_synth_state_t *cn = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0,
                                              1, 4, 8, 7, 0, 0, 0.0);
    DP_REQUIRE_MSG (cn != NULL, "noise_steps/clean: create");
    float complex z[16];
    for (size_t i = 0; i < 16; i++)
      z[i] = 7.0f + 7.0f * I; /* poison, so "wrote zeros" is falsifiable */
    wfm_synth_noise_steps (cn, z, 16);
    int zeros = 1;
    for (size_t i = 0; i < 16; i++)
      if (z[i] != 0.0f + 0.0f * I)
        zeros = 0;
    DP_CHECK (zeros);
    wfm_synth_noise_steps (NULL, z, 16); /* documented NULL no-op */
    wfm_synth_destroy (cn);
    wfm_synth_destroy (na);
    wfm_synth_destroy (nb);
    free (g);
    free (on);
    free (sk);
  }

  /* ── §F  the ten accessors — read-back AND the behaviour each claims ─────
   *
   * All ten had zero mentions in any C test. Read-back alone would be
   * satisfied by a pair that stored into a field nothing reads, so each
   * setter is followed by the effect its doc comment promises.
   */
  {
    wfm_synth_state_t *ac = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0,
                                              1, 5, 4, 7, 0, 0, 0.0);
    DP_REQUIRE_MSG (ac != NULL, "accessors: create");
    DP_CHECK (wfm_synth_get_wtype (ac) == WFM_SYNTH_QPSK);
    DP_CHECK (wfm_synth_get_nsps (ac) == 4);
    wfm_synth_set_wtype (ac, WFM_SYNTH_BPSK);
    DP_CHECK (wfm_synth_get_wtype (ac) == WFM_SYNTH_BPSK);
    wfm_synth_set_wtype (ac, WFM_SYNTH_QPSK);
    wfm_synth_set_nsps (ac, 2);
    DP_CHECK (wfm_synth_get_nsps (ac) == 2);
    wfm_synth_set_nsps (ac, 4);
    /* sym_pos runs 0..nsps−1 and wraps: after k steps from a fresh reset it
       reads k mod nsps, and sym_pos == 0 means the NEXT sample starts a
       fresh symbol — which is what the doc comment offers for framing. */
    wfm_synth_reset (ac);
    DP_CHECK (wfm_synth_get_sym_pos (ac) == 0);
    for (int k = 1; k <= 9; k++)
      {
        (void)wfm_synth_step (ac);
        DP_CHECK (wfm_synth_get_sym_pos (ac) == k % 4);
      }
    /* cur_re/cur_im are the held symbol: for QPSK both legs are ±1/√2. */
    wfm_synth_reset (ac);
    (void)wfm_synth_step (ac);
    const float q = 0.70710678118654752f;
    DP_CHECK_NEAR (fabsf (wfm_synth_get_cur_re (ac)), q, 1e-6);
    DP_CHECK_NEAR (fabsf (wfm_synth_get_cur_im (ac)), q, 1e-6);
    /* an injected symbol takes effect within the current hold ... */
    wfm_synth_set_cur_re (ac, 0.25f);
    wfm_synth_set_cur_im (ac, -0.5f);
    DP_CHECK_NEAR (wfm_synth_get_cur_re (ac), 0.25f, 1e-9);
    DP_CHECK_NEAR (wfm_synth_get_cur_im (ac), -0.5f, 1e-9);
    float complex held = wfm_synth_step (ac); /* sym_pos is 1..3: mid-hold */
    DP_CHECK_NEAR (crealf (held), 0.25f, 1e-6);
    DP_CHECK_NEAR (cimagf (held), -0.5f, 1e-6);
    /* ... and injecting sym_pos = 0 forces the NEXT step to latch a fresh
       symbol from the LFSR, overwriting the injected one. */
    wfm_synth_set_sym_pos (ac, 0);
    DP_CHECK (wfm_synth_get_sym_pos (ac) == 0);
    float complex fresh = wfm_synth_step (ac);
    DP_CHECK_NEAR (fabsf (crealf (fresh)), q, 1e-6); /* latched, not 0.25 */
    DP_CHECK_NEAR (fabsf (cimagf (fresh)), q, 1e-6);
    wfm_synth_destroy (ac);
  }

  /* ── §G  RRC shaping against an EXTERNAL truth (both branches) ───────────
   *
   * Two claims meet here, and each was pinned only against the other path:
   * set_rrc's "the taps are scaled by sqrt(sps) internally for unit transmit
   * power, so every caller passes the raw taps", and shaper_prime's "the
   * polyphase output aligns with the dense FIR to float precision". The
   * existing sections compare step() against steps() and shaped against
   * unshaped — both structurally blind to a shared error in the shaping
   * itself, exactly the trap the validation page warns about.
   *
   * The truth used here is neither path: a direct convolution of the
   * sqrt(sps)-scaled taps with the sps-upsampled symbol impulse train,
   * evaluated in double. sps = 4 takes the polyphase branch (power of two)
   * and sps = 3 the dense-FIR fallback, so ONE truth covers both.
   */
  {
    const float         taps[5] = { 0.1f, -0.3f, 0.8f, 0.25f, -0.05f };
    const size_t        ntaps   = 5;
    const float complex syms[4] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I,
                                    -1.0f + 0.0f * I, 0.5f - 0.5f * I };
    const int sps_cases[2]      = { 4, 3 }; /* polyphase, then dense FIR */
    for (int c = 0; c < 2; c++)
      {
        const int          sps = sps_cases[c];
        const size_t       n   = 40;
        wfm_synth_state_t *sh  = wfm_synth_create (
            WFM_SYNTH_SYMBOLS, 1e6, 0.0, 100.0, 1, 1, sps, 7, 0, 0, 0.0);
        DP_REQUIRE_MSG (sh != NULL, "rrc/truth: create");
        DP_CHECK (wfm_synth_set_symbols (sh, syms, 4) == 0);
        DP_CHECK (wfm_synth_set_rrc (sh, taps, ntaps) == 0);
        /* the branch actually under test, asserted so a change in set_rrc's
           selection rule cannot quietly collapse both cases onto one path */
        if (c == 0)
          DP_CHECK (sh->shaper != NULL && sh->fir == NULL);
        else
          DP_CHECK (sh->fir != NULL && sh->shaper == NULL);
        float complex *y = malloc (n * sizeof *y);
        DP_REQUIRE_MSG (y != NULL, "rrc/truth: alloc");
        wfm_synth_steps (sh, y, n);
        const double scale = sqrt ((double)sps);
        int          ok    = 1;
        double       worst = 0.0;
        for (size_t i = 0; i < n; i++)
          {
            double complex want = 0.0;
            for (size_t k = 0; k < ntaps; k++)
              {
                if (k > i)
                  break;
                size_t j = i - k; /* impulse-train index */
                if (j % (size_t)sps)
                  continue; /* structural zero between symbols */
                want += (double)taps[k] * scale
                        * (double complex)syms[(j / (size_t)sps) % 4];
              }
            double err = cabs ((double complex)y[i] - want);
            if (err > worst)
              worst = err;
            if (err > 2e-6)
              ok = 0;
          }
        DP_CHECK_MSG (ok, "shaped output == scaled taps * upsampled symbols");
        if (!ok)
          fprintf (stderr, "  sps=%d worst |err| = %.3g\n", sps, worst);
        free (y);
        wfm_synth_destroy (sh);
      }
  }

  /* the serialization sections above also count via CHECK — fail if any
   * tripped (the early _fails gate only covered the pre-state sections). */
  DP_TEST_END ("test_wfm_synth_core");
}
