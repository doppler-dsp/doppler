/*
 * frame_core.c — the frame descriptor as an object. The contract, and the
 * reasoning behind each boundary, live on the declarations in
 * frame/frame_core.h.
 *
 * Everything here is lifecycle: copy the literal arrays, populate a
 * `wfm_frame_t`, and delegate. No offset, no CRC position and no bit order is
 * computed in this file — those live once, in wfm_frame.c, which is what lets
 * a receiver and a generator hold the same descriptor and agree.
 */
#include "frame/frame_core.h"

#include <stdlib.h>
#include <string.h>

/* Fill one `wfm_seq_t` from the flattened arguments, copying a literal array
   so the descriptor outlives the call. `own` receives the copy (NULL for a
   generated kind) and is freed by frame_destroy().

   The two lengths are NOT interchangeable and the choice is the kind's: a
   literal is exactly as long as the array it was given, while a generated
   field emits however many bits it was asked for. Returns 0, or -1 if the
   copy failed. */
static int
seq_fill (wfm_seq_t *s, uint8_t **own, int kind, const uint8_t *lit,
          size_t lit_len, size_t gen_len, uint64_t poly, uint64_t seed,
          uint32_t reg_bits, int lfsr, uint64_t taps_a, uint64_t seed_a,
          uint64_t taps_b, uint64_t seed_b)
{
  s->kind     = (wfm_seq_kind_t)kind;
  s->poly     = poly;
  s->seed     = seed;
  s->reg_bits = reg_bits;
  s->lfsr     = lfsr;
  s->taps_a   = taps_a;
  s->seed_a   = seed_a;
  s->taps_b   = taps_b;
  s->seed_b   = seed_b;

  if (kind == WFM_SEQ_LITERAL)
    {
      s->len = lit_len;
      if (lit && lit_len)
        {
          *own = (uint8_t *)malloc (lit_len);
          if (!*own)
            return -1;
          memcpy (*own, lit, lit_len);
          s->bits = *own;
        }
      /* A literal with a length but no array stays unbuildable on purpose:
         wfm_frame_bits() refuses it, and frame_create() turns that into a
         NULL rather than emitting a frame with a hole in it. */
      return 0;
    }
  s->len  = gen_len;
  s->bits = NULL;
  return 0;
}

frame_state_t *
frame_create (int preamble_kind, const uint8_t *preamble, size_t preamble_len,
              size_t preamble_nbits, size_t preamble_reps,
              uint64_t preamble_poly, uint64_t preamble_seed,
              uint32_t preamble_reg_bits, int preamble_lfsr,
              uint64_t preamble_taps_a, uint64_t preamble_seed_a,
              uint64_t preamble_taps_b, uint64_t preamble_seed_b,
              int sync_kind, const uint8_t *sync, size_t sync_len,
              size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed,
              uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a,
              uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b,
              int payload_kind, const uint8_t *payload, size_t payload_len,
              size_t payload_nbits, uint64_t payload_poly,
              uint64_t payload_seed, uint32_t payload_reg_bits,
              int payload_lfsr, uint64_t payload_taps_a,
              uint64_t payload_seed_a, uint64_t payload_taps_b,
              uint64_t payload_seed_b, int crc)
{
  frame_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  if (seq_fill (&obj->f.preamble, &obj->preamble_own, preamble_kind, preamble,
                preamble_len, preamble_nbits, preamble_poly, preamble_seed,
                preamble_reg_bits, preamble_lfsr, preamble_taps_a,
                preamble_seed_a, preamble_taps_b, preamble_seed_b)
          != 0
      || seq_fill (&obj->f.sync, &obj->sync_own, sync_kind, sync, sync_len,
                   sync_nbits, sync_poly, sync_seed, sync_reg_bits, sync_lfsr,
                   sync_taps_a, sync_seed_a, sync_taps_b, sync_seed_b)
             != 0
      || seq_fill (&obj->f.payload, &obj->payload_own, payload_kind, payload,
                   payload_len, payload_nbits, payload_poly, payload_seed,
                   payload_reg_bits, payload_lfsr, payload_taps_a,
                   payload_seed_a, payload_taps_b, payload_seed_b)
             != 0)
    {
      frame_destroy (obj);
      return NULL;
    }
  obj->f.preamble_reps = preamble_reps;
  obj->f.crc           = crc;

  /* Geometry once, from the one implementation. */
  wfm_frame_layout (&obj->f, &obj->l);
  obj->nbits = obj->l.total_bits;
  if (obj->nbits == 0)
    {
      frame_destroy (obj);
      return NULL;
    }

  /* Materialise now: a descriptor that cannot produce its own bits is not a
     frame, and finding out here is what lets the caller be told at the point
     the mistake was made rather than three calls later. The buffer is then
     what `bits()` repeats, so a generator runs once per frame DESCRIPTION
     instead of once per frame. (Repeats would be identical either way —
     wfm_frame.c builds and destroys its LFSR per call — so this is about
     where the refusal happens and what the repeat costs, not correctness.) */
  obj->one = (uint8_t *)malloc (obj->nbits);
  if (!obj->one
      || wfm_frame_bits (&obj->f, obj->one, obj->nbits) != obj->nbits)
    {
      frame_destroy (obj);
      return NULL;
    }
  return obj;
}

void
frame_destroy (frame_state_t *state)
{
  if (!state)
    return;
  free (state->preamble_own);
  free (state->sync_own);
  free (state->payload_own);
  free (state->one);
  free (state);
}

size_t
frame_bits_max_out (frame_state_t *state, size_t n)
{
  return state ? n * state->nbits : 0;
}

size_t
frame_bits (frame_state_t *state, size_t n, uint8_t *out, size_t max_out)
{
  if (!state || !out)
    return 0;
  /* Whole frames only: half a frame is not a frame, and a caller comparing
     against a capture would silently misalign every subsequent one. */
  size_t fit = max_out / state->nbits;
  if (n > fit)
    n = fit;
  for (size_t i = 0; i < n; i++)
    memcpy (out + i * state->nbits, state->one, state->nbits);
  return n * state->nbits;
}

wfm_frame_layout_t
frame_layout (frame_state_t *state)
{
  return state->l;
}

int
frame_crc_ok (frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len)
{
  if (!state || !rx_bits || rx_bits_len < state->nbits)
    return -1;
  return wfm_frame_crc_ok (&state->f, rx_bits);
}
