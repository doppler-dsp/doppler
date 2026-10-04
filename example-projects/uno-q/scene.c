#include "scene.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#include "doppler/wfm_synth/wfm_synth_core.h"

uint8_t
scene_rtl_code (float v)
{
  float c = floorf (128.0f + 128.0f * v);
  return (uint8_t)(c < 0.0f ? 0.0f : c > 255.0f ? 255.0f : c);
}

uint8_t *
scene_cu8 (double fs, double offset, size_t n)
{
  float complex *x   = malloc (n * sizeof *x);
  uint8_t       *cu8 = malloc (2 * n);

  /* The tone and its noise come from the library's waveform synthesizer —
     the engine behind `wfmgen --type tone --snr` — at the SNR the scene's
     amplitude and noise sigma imply: tone power A^2 over complex noise power
     2*sigma^2, in the full sample-rate band (snr_mode 1). It returns a unit
     tone plus noise at that ratio; scale it to the scene's amplitude. */
  const double snr_db
      = 10.0
        * log10 (SCENE_TONE_AMP * SCENE_TONE_AMP
                 / (2.0 * SCENE_NOISE_SIGMA * SCENE_NOISE_SIGMA));
  dp_wfm_synth_state_t *syn = dp_wfm_synth_create (
      WFM_SYNTH_TONE, fs, offset, snr_db, 1, SCENE_SEED, 1, 8, 0, 0, 0.0);
  if (x && cu8 && syn)
    {
      dp_wfm_synth_steps (syn, x, n);
      for (size_t i = 0; i < n; i++)
        {
          const float complex v = (float)SCENE_TONE_AMP * x[i];
          cu8[2 * i]            = scene_rtl_code (crealf (v));
          cu8[2 * i + 1]        = scene_rtl_code (cimagf (v));
        }
    }
  else
    {
      free (cu8);
      cu8 = NULL;
    }
  dp_wfm_synth_destroy (syn);
  free (x);
  return cu8;
}
