#include "scene.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#include "doppler/awgn/awgn_core.h"
#include "doppler/lo/lo_core.h"

uint8_t
scene_rtl_code (float v)
{
  float c = floorf (128.0f + 128.0f * v);
  return (uint8_t)(c < 0.0f ? 0.0f : c > 255.0f ? 255.0f : c);
}

uint8_t *
scene_cu8 (double fs, double offset, size_t n)
{
  float complex   *x   = malloc (n * sizeof *x);
  float complex   *w   = malloc (n * sizeof *w);
  uint8_t         *cu8 = malloc (2 * n);
  dp_lo_state_t   *lo  = dp_lo_create (offset / fs);
  dp_awgn_state_t *ns  = dp_awgn_create (SCENE_SEED, (float)SCENE_NOISE_SIGMA);
  if (x && w && cu8 && lo && ns)
    {
      dp_lo_steps (lo, n, x, n);
      dp_awgn_generate (ns, n, w, n);
      for (size_t i = 0; i < n; i++)
        {
          float complex v = (float)SCENE_TONE_AMP * x[i] + w[i];
          cu8[2 * i]      = scene_rtl_code (crealf (v));
          cu8[2 * i + 1]  = scene_rtl_code (cimagf (v));
        }
    }
  else
    {
      free (cu8);
      cu8 = NULL;
    }
  dp_lo_destroy (lo);
  dp_awgn_destroy (ns);
  free (x);
  free (w);
  return cu8;
}
