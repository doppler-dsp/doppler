/*
 * wfm_dsp.c — DSSS spreading + root-raised-cosine taps (Phase B).
 */
#include "doppler/wfm/wfm_dsp.h"

#include "doppler/wfm/wfm_frame.h"

#include "doppler/dp_crc16.h"

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void
dp_wfm_rrc_taps (double beta, int sps, int span, float *taps)
{
  size_t n      = wfm_rrc_ntaps (sps, span);
  double center = (double)(span * sps);
  double sumsq  = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      /* t in symbol periods (T = 1). The formula itself lives once, in
         wfm_rrc_h() (wfm_dsp.h) — this walks it on the uniform 1/sps grid;
         a receiver's matched-filter bank samples the same evaluator at
         non-uniform instants. */
      double t = ((double)i - center) / (double)sps;
      double h = wfm_rrc_h (t, beta);
      taps[i]  = (float)h;
      sumsq += h * h;
    }
  /* normalise to unit energy */
  double norm = (sumsq > 0.0) ? 1.0 / sqrt (sumsq) : 1.0;
  for (size_t i = 0; i < n; i++)
    taps[i] = (float)(taps[i] * norm);
}

void
dp_wfm_polyphase_bank (const float *proto, size_t proto_len, size_t num_phases,
                       size_t num_taps, float *bank)
{
  /* Deal the prototype into `num_phases` phases: phase p selects the taps that
     land on outputs of residue p, i.e. bank[p][t] = proto[t*num_phases + p].
     Zero-pad the final partial tap when num_phases*num_taps > proto_len. This
     is exactly the decomposition resamp's own Kaiser bank uses, so the bank
     drops straight into dp_resamp_create_custom(num_phases, num_taps, bank,
     ...).
   */
  for (size_t p = 0; p < num_phases; p++)
    for (size_t t = 0; t < num_taps; t++)
      {
        size_t idx             = t * num_phases + p;
        bank[p * num_taps + t] = (idx < proto_len) ? proto[idx] : 0.0f;
      }
}

void
dp_wfm_rrc_polyphase_bank (double beta, int sps, int span, float *bank)
{
  size_t proto_len = wfm_rrc_ntaps (sps, span); /* 2*span*sps + 1 */

  /* Unit-energy prototype, then the sqrt(sps) unit-average-power scale that
     dp_wfm_synth_set_rrc applies to the dense taps — folded in here so the
     polyphase shaper matches the dense FIR amplitude. */
  float *proto = dp_xmalloc (proto_len * sizeof (float));
  dp_wfm_rrc_taps (beta, sps, span, proto);
  float scale = (float)sqrt ((double)sps);
  for (size_t i = 0; i < proto_len; i++)
    proto[i] *= scale;

  dp_wfm_polyphase_bank (proto, proto_len, (size_t)sps,
                         wfm_rrc_bank_ntaps (span), bank);
  free (proto);
}

void
dp_wfm_dsss_spread (const float _Complex *syms, size_t n_sym,
                    const uint8_t *code, size_t sf, float _Complex *out)
{
  for (size_t i = 0; i < n_sym; i++)
    {
      float _Complex s = syms[i];
      for (size_t j = 0; j < sf; j++)
        out[i * sf + j] = (code[j] & 1u) ? -s : s;
    }
}

/* The ONE continuous-DSSS symbol clock (wfm_dsp.h says why it is here and
   exact). Fast math is off for this function alone -- GCC by an optimize
   pragma around it, Clang (and clang-cl) by a block-scope float_control.
   Measured at -O3 -ffast-math, symbol 51 at 6138000 / 2 / 2700 opens on
   chip 57971 instead of 57970 under either compiler without them, and
   GCC's narrower "no-reciprocal-math" alone does NOT restore it: some other
   fast-math rewrite of the quotient comparison does the same damage. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("no-fast-math")
#endif
uint64_t
dp_wfm_dsss_cont_edge (uint64_t k, double chips_per_symbol)
{
#if defined(__clang__)
#pragma float_control(precise, on)
#endif
  /* ceil(k * cps) is the edge in exact arithmetic and a chip either side
     of it in floating point, so it only starts the search; the quotient
     decides. cps >= 1 keeps each step one chip. */
  uint64_t n = (uint64_t)ceil ((double)k * chips_per_symbol);
  while (n && (uint64_t)((double)(n - 1u) / chips_per_symbol) >= k)
    n--;
  while ((uint64_t)((double)n / chips_per_symbol) < k)
    n++;
  return n;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

size_t
dp_wfm_cont_dsss_chips (const uint8_t *code, size_t code_len,
                        const uint8_t *data, size_t n_data,
                        double chips_per_symbol, size_t n_chips, uint8_t *out)
{
  if (!code || code_len == 0 || !data || n_data == 0
      || !(chips_per_symbol > 0.0) || n_chips == 0)
    return 0;
  /* Both clocks advance off the same chip index, independently: the code by
     integer modulo, the data by the one symbol clock's edges -- a floor of a
     FRACTIONAL quotient, which is what puts symbol boundaries inside code
     epochs and makes consecutive symbols span different chip counts. */
  size_t   si   = 0;
  uint64_t next = dp_wfm_dsss_cont_edge (1, chips_per_symbol);
  for (size_t i = 0; i < n_chips; i++)
    {
      if (i >= next)
        next = dp_wfm_dsss_cont_edge (++si + 1u, chips_per_symbol);
      const uint8_t b = data[si % n_data] & 1u;
      out[i]          = (uint8_t)((code[i % code_len] & 1u) ^ b);
    }
  return n_chips;
}
