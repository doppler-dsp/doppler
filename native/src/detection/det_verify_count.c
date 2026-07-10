#include "detection/detection_core.h"
#include <limits.h>
#include <math.h>

int
det_verify_count (double p_look, double p_target)
{
  /* Consecutive independent looks compound: n looks at per-look
   * probability p reach p^n, so the smallest n with p^n <= p_target is
   * ceil(ln p_target / ln p_look). One function serves both sides of a
   * lock detector — declare (p = per-look pfa, target = false-declare
   * budget) and drop (p = per-look miss rate 1-pd, target = false-drop
   * budget). */
  if (p_target >= p_look)
    return 1; /* a single look already meets the budget */
  if (p_look <= 0.0)
    return 1; /* an impossible look can never compound */
  if (p_look >= 1.0)
    return INT_MAX; /* a certain look never reaches a smaller target */
  /* Nudge below the ratio before ceil so an exact multiple (e.g.
   * p_look = 1e-3, p_target = 1e-9 -> exactly 3) is not pushed to n+1 by
   * a last-ulp excess in the log quotient. */
  double n = ceil (log (p_target) / log (p_look) - 1e-9);
  return n < 1.0 ? 1 : (int)n;
}
