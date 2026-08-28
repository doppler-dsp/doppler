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

#include "ccsds_tm/ccsds_tm_frame.h"

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

  if (seq_fill (&obj->f.preamble, &obj->own[WFM_FRAME_FIELD_PREAMBLE],
                preamble_kind, preamble, preamble_len, preamble_nbits,
                preamble_poly, preamble_seed, preamble_reg_bits, preamble_lfsr,
                preamble_taps_a, preamble_seed_a, preamble_taps_b,
                preamble_seed_b)
          != 0
      || seq_fill (&obj->f.sync, &obj->own[WFM_FRAME_FIELD_SYNC], sync_kind,
                   sync, sync_len, sync_nbits, sync_poly, sync_seed,
                   sync_reg_bits, sync_lfsr, sync_taps_a, sync_seed_a,
                   sync_taps_b, sync_seed_b)
             != 0
      || seq_fill (&obj->f.payload, &obj->own[WFM_FRAME_FIELD_PAYLOAD],
                   payload_kind, payload, payload_len, payload_nbits,
                   payload_poly, payload_seed, payload_reg_bits, payload_lfsr,
                   payload_taps_a, payload_seed_a, payload_taps_b,
                   payload_seed_b)
             != 0)
    {
      frame_destroy (obj);
      return NULL;
    }
  obj->f.preamble_reps = preamble_reps;
  obj->f.crc           = crc;

  /* The four fields ARE a description, so this path and the builder converge
     here and every method below reads only `d`. The seq structs carry the
     `bits` pointers seq_fill already aimed at the owned copies, so nothing
     needs repointing. */
  wfm_frame_describe (&obj->f, &obj->d);
  obj->named = 1;

  /* Geometry once, from the one implementation. */
  wfm_frame_layout (&obj->f, &obj->l);
  wfm_frame_desc_layout (&obj->d, &obj->dl);
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
      || wfm_frame_assemble (&obj->d, NULL, obj->one, obj->nbits)
             != obj->nbits)
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
  for (unsigned i = 0; i < WFM_FRAME_MAX_FIELDS; i++)
    free (state->own[i]);
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
  return wfm_frame_desc_crc_ok (&state->d, rx_bits);
}

/* ── the builder ──────────────────────────────────────────────────────
 *
 * The other way in. frame_create() above takes the four fields wfm_frame_t
 * names; these take one field at a time, so a caller can describe a frame
 * that fixed list cannot hold. Both fill the same `d`, and every method above
 * reads only that -- which is the whole reason the two can share them.
 */

frame_state_t *
frame_create_desc (
    int preamble_kind, const uint8_t *preamble, size_t preamble_len,
    size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly,
    uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr,
    uint64_t preamble_taps_a, uint64_t preamble_seed_a,
    uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind,
    const uint8_t *sync, size_t sync_len, size_t sync_nbits,
    uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits,
    int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a,
    uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind,
    const uint8_t *payload, size_t payload_len, size_t payload_nbits,
    uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits,
    int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a,
    uint64_t payload_taps_b, uint64_t payload_seed_b, int crc)
{
  frame_state_t *obj = (frame_state_t *)calloc (1, sizeof (frame_state_t));
  if (!obj)
    return NULL;

  /* The SAME arguments as frame_create, and that is the flavor: this one
     stops before materialising, so the four fields are a STARTING POINT a
     caller extends rather than a finished frame. Pass empty arrays for all
     three to begin from nothing.

     An empty description is therefore legal here and refused there. The
     difference is where completeness can be judged: frame_create()'s
     description is complete when it returns, and this one is not complete
     until frame_build() is called. */
  if (seq_fill (&obj->f.preamble, &obj->own[WFM_FRAME_FIELD_PREAMBLE],
                preamble_kind, preamble, preamble_len, preamble_nbits,
                preamble_poly, preamble_seed, preamble_reg_bits, preamble_lfsr,
                preamble_taps_a, preamble_seed_a, preamble_taps_b,
                preamble_seed_b)
          != 0
      || seq_fill (&obj->f.sync, &obj->own[WFM_FRAME_FIELD_SYNC], sync_kind,
                   sync, sync_len, sync_nbits, sync_poly, sync_seed,
                   sync_reg_bits, sync_lfsr, sync_taps_a, sync_seed_a,
                   sync_taps_b, sync_seed_b)
             != 0
      || seq_fill (&obj->f.payload, &obj->own[WFM_FRAME_FIELD_PAYLOAD],
                   payload_kind, payload, payload_len, payload_nbits,
                   payload_poly, payload_seed, payload_reg_bits, payload_lfsr,
                   payload_taps_a, payload_seed_a, payload_taps_b,
                   payload_seed_b)
             != 0)
    {
      frame_destroy (obj);
      return NULL;
    }
  obj->f.preamble_reps = preamble_reps;
  obj->f.crc           = crc;

  /* An empty geometry starts an EMPTY description rather than four
     zero-length fields. wfm_frame_describe always emits its four, which is
     right there -- it is what keeps `crc_off` at the end of the payload when
     the trailer is absent -- and wrong here, where a placeholder field would
     take an index, push the caller's first real field to 4, and leave a
     description whose field count does not match what anyone wrote. */
  if (wfm_frame_nbits (&obj->f) != 0)
    wfm_frame_describe (&obj->f, &obj->d);

  /* `named` stays 0 on purpose: layout()'s NAMED view would go stale the
     moment a fifth field is appended, and a stale offset is worse than an
     absent one. A description is read through the indexed accessors. */
  return obj;
}

int
frame_add_field (frame_state_t *state, const uint8_t *lit, size_t lit_len,
                 int kind, size_t gen_len, size_t reps, uint64_t poly,
                 uint64_t seed, uint32_t reg_bits, int lfsr, uint64_t taps_a,
                 uint64_t seed_a, uint64_t taps_b, uint64_t seed_b,
                 uint32_t derived_by, size_t derived_bits)
{
  if (!state || state->one != NULL
      || state->d.n_fields >= WFM_FRAME_MAX_FIELDS)
    return -1;

  const unsigned i = state->d.n_fields;
  wfm_field_t   *f = &state->d.field[i];
  memset (f, 0, sizeof *f);

  if (derived_by)
    {
      /* A derived field carries no sequence: its bits are the stage's
         output, and its length is what that stage will write. */
      f->derived_by = derived_by;
      f->bits       = derived_bits;
    }
  else if (seq_fill (&f->seq, &state->own[i], kind, lit, lit_len, gen_len,
                     poly, seed, reg_bits, lfsr, taps_a, seed_a, taps_b,
                     seed_b)
           != 0)
    return -1;

  f->reps = reps;
  state->d.n_fields++;
  return (int)i;
}

int
frame_add_stage (frame_state_t *state, int kind, uint32_t first_field,
                 uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                 uint32_t emit_den, uint32_t unit_bits)
{
  if (!state || state->one != NULL
      || state->d.n_stages >= WFM_FRAME_MAX_STAGES)
    return -1;

  const unsigned i = state->d.n_stages;
  wfm_stage_t   *s = &state->d.stage[i];
  memset (s, 0, sizeof *s);
  s->kind        = (wfm_stage_kind_t)kind;
  s->first_field = first_field;
  s->n_fields    = n_fields;
  s->depth       = depth;
  s->emit_num    = emit_num;
  s->emit_den    = emit_den;
  s->unit_bits   = unit_bits;
  state->d.n_stages++;
  return (int)i;
}

int
frame_build (frame_state_t *state)
{
  if (!state || state->one != NULL)
    return -1;
  if (wfm_frame_desc_layout (&state->d, &state->dl) != 0)
    return -1;

  state->nbits = state->dl.out_bits;
  if (state->nbits == 0)
    return -1;

  /* Materialised here for the reason frame_create() materialises in its own
     body: a description that cannot produce its own bits is not a frame, and
     the refusal belongs at the point the caller can still do something about
     it. This is also where a stage naming a kernel nothing here carries is
     refused -- wfm_frame_assemble returns 0 rather than skipping it.

     The CCSDS kernels are what make the coded stages reachable from Python at
     all: ccsds_tm has no binding of its own and is not getting one, so this
     object is where a caller meets the outer code, the randomiser and the
     inner code. The dependency runs frame -> ccsds_tm -> wfm_frame, which is
     the direction ccsds_tm's own CMakeLists anticipated ("BOTH ends want it:
     wfmgen encodes, and frame/ber_meter will decode").

     NULL for the inner encoder's state: a description describes ONE frame, so
     each build starts from the all-zero register. A stream of CADUs sharing
     one register is a transmitter's job, and ccsds_tm_frame_encode is where
     that lives. */
  wfm_frame_ops_t ops;
  ccsds_tm_frame_ops (&ops, NULL);

  state->one = (uint8_t *)malloc (state->nbits);
  if (!state->one
      || wfm_frame_assemble (&state->d, &ops, state->one, state->nbits)
             != state->nbits)
    {
      free (state->one);
      state->one   = NULL;
      state->nbits = 0;
      return -1;
    }
  return 0;
}

size_t
frame_n_fields (frame_state_t *state)
{
  return state ? state->d.n_fields : 0u;
}

size_t
frame_n_stages (frame_state_t *state)
{
  return state ? state->d.n_stages : 0u;
}

size_t
frame_field_off (frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_fields) ? state->dl.field_off[i] : 0u;
}

size_t
frame_field_bits (frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_fields) ? state->dl.field_bits[i] : 0u;
}

size_t
frame_stage_first (frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_stages) ? state->dl.stage[i].first : 0u;
}

size_t
frame_stage_bits (frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_stages) ? state->dl.stage[i].n : 0u;
}

size_t
frame_deframe_max_out (frame_state_t *state, size_t rx_bits_len)
{
  (void)rx_bits_len; /* the length is the description's, not the input's */
  return state ? state->dl.frame_bits : 0u;
}

size_t
frame_deframe (frame_state_t *state, const uint8_t *rx_bits,
               size_t rx_bits_len, uint8_t *out, size_t max_out)
{
  if (!state || !rx_bits || !out || state->dl.frame_bits == 0
      || rx_bits_len < state->dl.frame_bits || max_out < state->dl.frame_bits)
    {
      if (state)
        {
          state->rx_ok      = 0;
          state->rx_checked = 0;
          state->rx_units   = 0;
          state->rx_symbols = 0;
        }
      return 0;
    }

  /* The caller's bits are a CAPTURE: copy before the stages correct, so a
     frame can be deframed twice and score the same both times. */
  memcpy (out, rx_bits, state->dl.frame_bits);

  wfm_frame_ops_t ops;
  ccsds_tm_frame_ops (&ops, NULL);
  wfm_frame_rx_t rx;
  const int      verdict = wfm_frame_check (&state->d, &ops, out, &rx);

  state->rx_checked = 0;
  state->rx_units   = 0;
  state->rx_ok      = 0;
  state->rx_symbols = 0;
  if (verdict >= 0)
    {
      state->rx_checked = (int)rx.checked;
      for (unsigned i = 0; i < rx.n_stages; i++)
        {
          state->rx_units += (int)rx.stage[i].units;
          state->rx_ok += (int)rx.stage[i].ok;
          state->rx_symbols += (int)rx.stage[i].symbols;
        }
    }
  return state->dl.frame_bits;
}

frame_check_t
frame_check (frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len)
{
  frame_check_t out;
  memset (&out, 0, sizeof out);
  if (!state || !rx_bits || rx_bits_len < state->dl.frame_bits
      || state->dl.frame_bits == 0)
    return out;

  /* A copy, because the stages CORRECT in place and the caller's buffer is a
     capture -- scoring a frame must not rewrite the evidence. */
  uint8_t *work = (uint8_t *)malloc (state->dl.frame_bits);
  if (!work)
    return out;
  memcpy (work, rx_bits, state->dl.frame_bits);

  wfm_frame_ops_t ops;
  ccsds_tm_frame_ops (&ops, NULL);
  wfm_frame_rx_t rx;
  const int      verdict = wfm_frame_check (&state->d, &ops, work, &rx);
  free (work);
  if (verdict < 0)
    return out; /* no reversible stage: pass = 0, checked = 0 */

  out.passed  = verdict;
  out.stages  = rx.n_stages;
  out.checked = rx.checked;
  for (unsigned k = 0; k < rx.n_stages; k++)
    {
      if (!rx.stage[k].checked)
        continue;
      out.units += rx.stage[k].units;
      out.ok += rx.stage[k].ok;
      out.corrected += rx.stage[k].corrected;
      out.symbols += rx.stage[k].symbols;
    }
  return out;
}
