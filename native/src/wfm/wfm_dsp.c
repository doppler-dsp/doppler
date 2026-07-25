/*
 * wfm_dsp.c — DSSS spreading + root-raised-cosine taps (Phase B).
 */
#include "wfm/wfm_dsp.h"

#include "dp_crc16.h"

#include <complex.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void
wfm_rrc_taps (double beta, int sps, int span, float *taps)
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
wfm_polyphase_bank (const float *proto, size_t proto_len, size_t num_phases,
                    size_t num_taps, float *bank)
{
  /* Deal the prototype into `num_phases` phases: phase p selects the taps that
     land on outputs of residue p, i.e. bank[p][t] = proto[t*num_phases + p].
     Zero-pad the final partial tap when num_phases*num_taps > proto_len. This
     is exactly the decomposition resamp's own Kaiser bank uses, so the bank
     drops straight into resamp_create_custom(num_phases, num_taps, bank, ...).
   */
  for (size_t p = 0; p < num_phases; p++)
    for (size_t t = 0; t < num_taps; t++)
      {
        size_t idx             = t * num_phases + p;
        bank[p * num_taps + t] = (idx < proto_len) ? proto[idx] : 0.0f;
      }
}

void
wfm_rrc_polyphase_bank (double beta, int sps, int span, float *bank)
{
  size_t proto_len = wfm_rrc_ntaps (sps, span); /* 2*span*sps + 1 */

  /* Unit-energy prototype, then the sqrt(sps) unit-average-power scale that
     wfm_synth_set_rrc applies to the dense taps — folded in here so the
     polyphase shaper matches the dense FIR amplitude. */
  float *proto = dp_xmalloc (proto_len * sizeof (float));
  wfm_rrc_taps (beta, sps, span, proto);
  float scale = (float)sqrt ((double)sps);
  for (size_t i = 0; i < proto_len; i++)
    proto[i] *= scale;

  wfm_polyphase_bank (proto, proto_len, (size_t)sps, wfm_rrc_bank_ntaps (span),
                      bank);
  free (proto);
}

void
wfm_dsss_spread (const float _Complex *syms, size_t n_sym, const uint8_t *code,
                 size_t sf, float _Complex *out)
{
  for (size_t i = 0; i < n_sym; i++)
    {
      float _Complex s = syms[i];
      for (size_t j = 0; j < sf; j++)
        out[i * sf + j] = (code[j] & 1u) ? -s : s;
    }
}

size_t
wfm_cont_dsss_chips (const uint8_t *code, size_t code_len, const uint8_t *data,
                     size_t n_data, double chips_per_symbol, size_t n_chips,
                     uint8_t *out)
{
  if (!code || code_len == 0 || !data || n_data == 0
      || !(chips_per_symbol > 0.0) || n_chips == 0)
    return 0;
  /* Both clocks advance off the same chip index, independently: the code by
     integer modulo, the data by a floor of a FRACTIONAL quotient.  That floor
     is the whole point -- it is what puts symbol boundaries inside code
     epochs and makes consecutive symbols span different chip counts. */
  for (size_t i = 0; i < n_chips; i++)
    {
      size_t  ci = i % code_len;
      size_t  si = (size_t)((double)i / chips_per_symbol);
      uint8_t b  = data[si % n_data] & 1u;
      out[i]     = (uint8_t)((code[ci] & 1u) ^ b);
    }
  return n_chips;
}

size_t
wfm_frame_dsss_nchips (size_t acq_len, size_t acq_reps, size_t data_len,
                       size_t sync_len, size_t payload_len, int crc)
{
  size_t pre = acq_len * acq_reps;
  /* The CRC trailer protects the payload; with no payload there is nothing
     to protect, so it is dropped rather than emitting crc16(∅). */
  size_t nbits = sync_len + payload_len + ((crc && payload_len) ? 16u : 0u);
  if (nbits && data_len == 0)
    return 0; /* frame bits with no spreading code */
  size_t n = pre + nbits * data_len;
  return n; /* 0 when there is nothing at all to transmit */
}

size_t
wfm_frame_dsss_chips (const uint8_t *acq_code, size_t acq_len, size_t acq_reps,
                      const uint8_t *data_code, size_t data_len,
                      const uint8_t *sync, size_t sync_len,
                      const uint8_t *payload, size_t payload_len, int crc,
                      uint8_t *out)
{
  size_t total = wfm_frame_dsss_nchips (acq_len, acq_reps, data_len, sync_len,
                                        payload_len, crc);
  if (total == 0)
    return 0;
  size_t w = 0;
  /* Unmodulated repeated preamble: the acquisition code, verbatim. */
  for (size_t r = 0; r < acq_reps; r++)
    for (size_t i = 0; i < acq_len; i++)
      out[w++] = acq_code[i] & 1u;
  /* Frame symbols (sync | payload | crc), each XOR-spread by the data code:
     a 0 bit transmits the code as-is, a 1 bit transmits it inverted. */
  for (size_t i = 0; i < sync_len; i++)
    {
      uint8_t b = sync[i] & 1u;
      for (size_t j = 0; j < data_len; j++)
        out[w++] = b ^ (data_code[j] & 1u);
    }
  for (size_t i = 0; i < payload_len; i++)
    {
      uint8_t b = payload[i] & 1u;
      for (size_t j = 0; j < data_len; j++)
        out[w++] = b ^ (data_code[j] & 1u);
    }
  if (crc && payload_len)
    {
      uint16_t c = dp_crc16_ccitt (payload, payload_len);
      for (size_t i = 0; i < 16; i++)
        {
          uint8_t b = (uint8_t)((c >> (15 - i)) & 1u); /* MSB-first */
          for (size_t j = 0; j < data_len; j++)
            out[w++] = b ^ (data_code[j] & 1u);
        }
    }
  return w;
}
