/*
 * syncword_core.c — the frame-sync detector.
 *
 * Every line of arithmetic here is dp_syncword.h's. What this file owns is
 * the marker's LIFETIME: a searcher copies the pattern it was built from, so
 * `SyncFinder(ccsds_asm_bits())` — a temporary that numpy frees the moment
 * the constructor returns — is a valid searcher rather than a dangling one.
 */
#include "syncword/syncword_core.h"

#include <stdlib.h>

syncword_state_t *
syncword_create (const uint8_t *marker, size_t marker_len)
{
  /* An empty marker matches at every offset with zero errors, which is not
     a degenerate search but a wrong one -- it would report frame sync
     immediately, forever. Refused at construction (jm gh-482 turns this
     NULL into a ValueError naming the reason) rather than at every find. */
  if (!marker || marker_len == 0u)
    return NULL;

  syncword_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  obj->marker = malloc (marker_len);
  if (!obj->marker)
    {
      free (obj);
      return NULL;
    }

  /* Normalised to 0/1 on the way in rather than masked on every comparison:
     the search runs marker_len XORs per offset and the caller's convention
     is already one bit per byte, so this pays once for a hot loop that
     would otherwise pay per bit. */
  for (size_t i = 0; i < marker_len; i++)
    obj->marker[i] = (uint8_t)(marker[i] & 1u);
  obj->nbits = marker_len;
  return obj;
}

void
syncword_destroy (syncword_state_t *state)
{
  if (!state)
    return;
  free (state->marker);
  free (state);
}

syncword_hit_t
syncword_find (syncword_state_t *state, const uint8_t *bits, size_t bits_len,
               uint32_t max_errors)
{
  syncword_hit_t    r = { 0, 0u, 0, 0u };
  dp_syncword_hit_t h;

  if (dp_syncword_find (bits, bits_len, state->marker, state->nbits,
                        (unsigned)max_errors, &h))
    {
      r.found    = 1;
      r.offset   = h.offset;
      r.inverted = h.inverted;
      r.errors   = (uint32_t)h.errors;
    }
  return r;
}

double
syncword_pfa (syncword_state_t *state, uint32_t max_errors)
{
  return dp_syncword_pfa (state->nbits, (unsigned)max_errors);
}

int
syncword_max_errors_for (syncword_state_t *state, size_t window_bits,
                         double pfa)
{
  return dp_syncword_max_errors (state->nbits, window_bits, pfa);
}
