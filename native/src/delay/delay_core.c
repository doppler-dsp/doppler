#include "delay/delay_core.h"
#include <string.h>

/* Round n up to the next power of two (returns 1 for n==0). */
static size_t
next_pow2 (size_t n)
{
  size_t c = 1;
  while (c < n)
    c <<= 1;
  return c;
}

delay_state_t *
delay_create (size_t num_taps)
{
  delay_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;

  state->num_taps = num_taps;
  state->capacity = next_pow2 (num_taps > 0 ? num_taps : 1);
  state->mask = state->capacity - 1;
  state->head = 0;

  /* Two copies so any num_taps-length window is always contiguous. */
  state->buf = calloc (2 * state->capacity, sizeof (double _Complex));
  if (!state->buf)
    {
      free (state);
      return NULL;
    }
  return state;
}

void
delay_destroy (delay_state_t *state)
{
  if (!state)
    return;
  free (state->buf);
  free (state);
}

void
delay_reset (delay_state_t *state)
{
  memset (state->buf, 0, 2 * state->capacity * sizeof (double _Complex));
  state->head = 0;
}

void
delay_push (delay_state_t *state, double complex x)
{
  /* Decrement head (wrapping), then write to both halves so the
   * window starting at head is always a contiguous run. */
  state->head = (state->head - 1) & state->mask;
  state->buf[state->head] = x;
  state->buf[state->head + state->capacity] = x;
}

size_t
delay_ptr_max_out (delay_state_t *state)
{
  return state->num_taps;
}

size_t
delay_ptr (delay_state_t *state, size_t n, double complex *out)
{
  size_t actual = n < state->num_taps ? n : state->num_taps;
  memcpy (out, &state->buf[state->head], actual * sizeof (double _Complex));
  return actual;
}

size_t
delay_push_ptr_max_out (delay_state_t *state)
{
  return state->num_taps;
}

size_t
delay_push_ptr (delay_state_t *state, double complex x, double complex *out)
{
  delay_push (state, x);
  memcpy (out, &state->buf[state->head],
          state->num_taps * sizeof (double _Complex));
  return state->num_taps;
}

void
delay_write (delay_state_t *state, double complex x)
{
  delay_push (state, x);
}
