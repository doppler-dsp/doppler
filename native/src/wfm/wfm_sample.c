/*
 * wfm_sample.c — shared interleaved-I/Q wire-format converter (see header).
 *
 * Lifted verbatim from wfm_reader.c's former static helpers so the reader and
 * the headerless IqFile share one bit-faithful decoder. The ELEM/BPS/SCALE
 * tables mirror wfm_writer's quantisation exactly.
 */
#include "wfm/wfm_sample.h"

/* per sample_type (0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8) — mirror wfm_writer
 */
static const size_t ELEM[5] = { 4, 8, 4, 2, 1 }; /* bytes per I or Q */
static const size_t BPS[5] = { 8, 16, 8, 4, 2 }; /* bytes per complex sample */
static const double SCALE[5] = { 0, 0, 2147483647.0, 32767.0, 127.0 };

/* Copy sz bytes of *src into *dst, reversing on big-endian so the host (LE on
   both wheel targets) sees a native value. Inverse of wfm_writer's put(). */
static void
swab_copy (void *dst, const uint8_t *src, size_t sz, int be)
{
  uint8_t *d = dst;
  for (size_t k = 0; k < sz; k++)
    d[k] = be ? src[sz - 1 - k] : src[k];
}

void
wfm_convert_pair (const uint8_t *p, int sample_type, int endian, float *re,
                  float *im)
{
  int be = endian ? 1 : 0;
  switch (sample_type)
    {
    case 0:
      {
        float a, b;
        swab_copy (&a, p, 4, be);
        swab_copy (&b, p + 4, 4, be);
        *re = a;
        *im = b;
        break;
      }
    case 1:
      {
        double a, b;
        swab_copy (&a, p, 8, be);
        swab_copy (&b, p + 8, 8, be);
        *re = (float)a;
        *im = (float)b;
        break;
      }
    case 2:
      {
        int32_t a, b;
        swab_copy (&a, p, 4, be);
        swab_copy (&b, p + 4, 4, be);
        *re = (float)(a / SCALE[2]);
        *im = (float)(b / SCALE[2]);
        break;
      }
    case 3:
      {
        int16_t a, b;
        swab_copy (&a, p, 2, be);
        swab_copy (&b, p + 2, 2, be);
        *re = (float)(a / SCALE[3]);
        *im = (float)(b / SCALE[3]);
        break;
      }
    default:
      {
        int8_t a = (int8_t)p[0], b = (int8_t)p[1];
        *re = (float)(a / SCALE[4]);
        *im = (float)(b / SCALE[4]);
        break;
      }
    }
}

size_t
wfm_bytes_per_sample (int sample_type)
{
  if (sample_type < 0 || sample_type > 4)
    return 0;
  return BPS[sample_type];
}
