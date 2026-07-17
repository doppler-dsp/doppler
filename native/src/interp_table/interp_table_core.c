#include "interp_table/interp_table_core.h"

#include <math.h>
#include <string.h>

interp_table_state_t *
interp_table_create (const double complex *table, size_t table_len, int method)
{
  if (table_len == 0)
    return NULL;
  interp_table_state_t *obj = malloc (sizeof (*obj));
  if (!obj)
    return NULL;
  obj->table = malloc (table_len * sizeof (double complex));
  if (!obj->table)
    {
      free (obj);
      return NULL;
    }
  memcpy (obj->table, table, table_len * sizeof (double complex));
  obj->n      = table_len;
  obj->method = method;
  return obj;
}

void
interp_table_destroy (interp_table_state_t *state)
{
  if (!state)
    return;
  free (state->table);
  free (state);
}

void
interp_table_reset (interp_table_state_t *state)
{
  (void)state;
}

size_t
interp_table_execute_max_out (interp_table_state_t *state)
{
  (void)state;
  return 0;
}

/* Wraps a floor'd index into [0, n): fmod() alone can return a negative
 * result for a negative dividend, so fold it back into range. */
static JM_FORCEINLINE size_t
wrap_index (double floor_pt, size_t n)
{
  double w = fmod (floor_pt, (double)n);
  if (w < 0.0)
    w += (double)n;
  return (size_t)w;
}

size_t
interp_table_execute (interp_table_state_t *state, const double *in,
                      size_t n_in, double complex *out)
{
  const double complex *table = state->table;
  size_t                n     = state->n;
  for (size_t i = 0; i < n_in; i++)
    {
      double point    = in[i];
      double floor_pt = floor (point);
      size_t lo       = wrap_index (floor_pt, n);

      if (state->method == 0) /* floor */
        {
          out[i] = table[lo];
          continue;
        }

      size_t hi   = (lo + 1 >= n) ? 0 : lo + 1;
      double frac = point - floor_pt;

      if (state->method == 1) /* nearest (ties round up) */
        {
          out[i] = (frac > 0.5) ? table[hi] : table[lo];
          continue;
        }

      /* linear */
      out[i] = table[lo] + frac * (table[hi] - table[lo]);
    }
  return n_in;
}
