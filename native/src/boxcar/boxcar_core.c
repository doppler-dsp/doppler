#include "boxcar/boxcar_core.h"

#include <stdlib.h>
#include <string.h>

void
boxcar_init (boxcar_state_t *s, size_t len, double gain)
{
  if (len == 0)
    len = 1;
  if (len > BOXCAR_MAX_LEN)
    len = BOXCAR_MAX_LEN;
  s->len     = len;
  s->inv_len = 1.0 / (double)len;
  boxcar_set_gain (s, gain); /* sets gain + cached scale = gain / len */
  boxcar_reset (s);
}

boxcar_state_t *
boxcar_create (size_t len, double gain)
{
  if (len == 0 || len > BOXCAR_MAX_LEN)
    return NULL; /* window must fit the fixed in-struct ring */
  boxcar_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  boxcar_init (s, len, gain);
  return s;
}

void
boxcar_destroy (boxcar_state_t *s)
{
  free (s);
}

void
boxcar_reset (boxcar_state_t *s)
{
  /* Clear the whole fixed ring (not just the active window) so the
   * pointer-free POD snapshot is deterministic regardless of how the struct
   * was allocated. */
  s->pos = 0;
  s->acc = 0.0f;
  memset (s->ring, 0, sizeof (s->ring));
}

void
boxcar_steps (boxcar_state_t *s, const float complex *input,
              float complex *output, size_t n)
{
  for (size_t i = 0; i < n; i++)
    output[i] = boxcar_step (s, input[i]);
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (boxcar, boxcar_state_t, BOXCAR_STATE_MAGIC,
                     BOXCAR_STATE_VERSION)
