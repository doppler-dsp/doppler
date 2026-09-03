/*
 * square_clip.c — util module-level function.
 *
 * Provides the external linkage definition of square_clip; the
 * header-only static inline copy is used by all other callers.
 */
#include <complex.h>
#include <math.h>

float _Complex square_clip (float _Complex y, float lin)
{
  float r = fminf (fmaxf (crealf (y), -lin), lin);
  float i = fminf (fmaxf (cimagf (y), -lin), lin);
  return r + i * I;
}
