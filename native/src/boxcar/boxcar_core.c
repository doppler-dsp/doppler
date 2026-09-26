#include "doppler/boxcar/boxcar_core.h"

#include <stdlib.h>
#include <string.h>

void
boxcar_init (dp_boxcar_state_t *s, size_t len, double gain)
{
  if (len == 0)
    len = 1;
  if (len > BOXCAR_MAX_LEN)
    len = BOXCAR_MAX_LEN;
  s->len     = len;
  s->inv_len = 1.0 / (double)len;
  dp_boxcar_set_gain (s, gain); /* sets gain + cached scale = gain / len */
  dp_boxcar_reset (s);
}

dp_boxcar_state_t *
dp_boxcar_create (size_t len, double gain)
{
  if (len == 0 || len > BOXCAR_MAX_LEN)
    return NULL; /* window must fit the fixed in-struct ring */
  dp_boxcar_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  boxcar_init (s, len, gain);
  return s;
}

void
dp_boxcar_destroy (dp_boxcar_state_t *s)
{
  free (s);
}

void
dp_boxcar_reset (dp_boxcar_state_t *s)
{
  /* Clear the whole fixed ring (not just the active window) so the
   * pointer-free POD snapshot is deterministic regardless of how the struct
   * was allocated. */
  s->pos = 0;
  s->acc = 0.0f;
  memset (s->ring, 0, sizeof (s->ring));
}

void
dp_boxcar_steps (dp_boxcar_state_t *s, const float _Complex *x,
                 float _Complex *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = dp_boxcar_step (s, x[i]);
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_boxcar, dp_boxcar_state_t, BOXCAR_STATE_MAGIC,
                     BOXCAR_STATE_VERSION)
