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
      /* t in symbol periods (T = 1) */
      double t = ((double)i - center) / (double)sps;
      double h;
      if (fabs (t) < 1e-9)
        {
          /* limit at t = 0 */
          h = 1.0 - beta + 4.0 * beta / M_PI;
        }
      else if (beta > 0.0 && fabs (fabs (t) - 1.0 / (4.0 * beta)) < 1e-9)
        {
          /* limit at t = ±1/(4β) (0/0 in the general form) */
          double a = M_PI / (4.0 * beta);
          h = (beta / sqrt (2.0))
              * ((1.0 + 2.0 / M_PI) * sin (a) + (1.0 - 2.0 / M_PI) * cos (a));
        }
      else
        {
          double pt  = M_PI * t;
          double num = sin (pt * (1.0 - beta))
                       + 4.0 * beta * t * cos (pt * (1.0 + beta));
          double den = pt * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));
          h          = num / den;
        }
      taps[i] = (float)h;
      sumsq += h * h;
    }
  /* normalise to unit energy */
  double norm = (sumsq > 0.0) ? 1.0 / sqrt (sumsq) : 1.0;
  for (size_t i = 0; i < n; i++)
    taps[i] = (float)(taps[i] * norm);
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
