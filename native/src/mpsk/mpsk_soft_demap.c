/*
 * mpsk_soft_demap.c — mpsk module-level function.
 *
 * Per-bit log-likelihood ratios by the max-log rule over the constellation.
 * The soft counterpart of mpsk_demap; see mpsk_core.h for the sign convention
 * and docs/design/mpsk.md §9.7 for why this is one general path rather than a
 * closed form per M.
 */
#include "mpsk/mpsk_core.h"

#include <float.h>

void
mpsk_soft_demap (const float complex *x, size_t x_len, float *llr,
                 size_t llr_len, int m, float n0)
{
  const int nb = mpsk_bps (m);
  if (nb == 0 || n0 <= 0.0f || llr_len < x_len * (size_t)nb)
    return;

  /* The constellation ONCE per call, not per symbol. mpsk_constellation is a
     cos/sin pair per point, which is why this function is the array face and
     there is no inline per-symbol twin -- a caller looping symbols would pay
     for the grid on every one of them. */
  float complex pts[8];
  for (int g = 0; g < m; g++)
    pts[g] = mpsk_constellation ((unsigned)g, m);

  for (size_t i = 0; i < x_len; i++)
    {
      /* Nearest point in each bit's 0-subset and 1-subset. Every M has both
         non-empty for every bit -- a Gray label of nb bits takes all 2^nb
         values across the M = 2^nb points -- so neither min stays at the
         sentinel and no guard is needed for one. */
      float d0[3], d1[3];
      for (int b = 0; b < nb; b++)
        d0[b] = d1[b] = FLT_MAX;

      const float yr = crealf (x[i]);
      const float yi = cimagf (x[i]);

      for (int g = 0; g < m; g++)
        {
          const float dr = yr - crealf (pts[g]);
          const float di = yi - cimagf (pts[g]);
          const float d  = dr * dr + di * di;

          for (int b = 0; b < nb; b++)
            {
              /* The label IS the Gray code, and bit b of it is the bit whose
                 LLR this is -- so the subsets are read straight off g with no
                 second mapping to disagree with mpsk_demap about. */
              if (((unsigned)g >> b) & 1u)
                {
                  if (d < d1[b])
                    d1[b] = d;
                }
              else if (d < d0[b])
                d0[b] = d;
            }
        }

      /* L = (d1 - d0)/n0: positive when the nearest 0-point is closer, i.e.
         positive means bit 0, which is what makes `L < 0` the same decision
         mpsk_demap makes. */
      for (int b = 0; b < nb; b++)
        llr[i * (size_t)nb + (size_t)b] = (d1[b] - d0[b]) / n0;
    }
}
