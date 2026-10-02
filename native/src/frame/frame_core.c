/*
 * frame_core.c — the frame descriptor as an object. The contract, and the
 * reasoning behind each boundary, live on the declarations in
 * frame/frame_core.h.
 *
 * Everything here is lifecycle: copy the literal arrays, describe them, and
 * delegate. No offset, no CRC position and no bit order is
 * computed in this file — those live once, in wfm_frame.c, which is what lets
 * a receiver and a generator hold the same descriptor and agree.
 */
#include "doppler/frame/frame_core.h"

#include "doppler/ccsds_tm/ccsds_tm_frame.h"
#include "doppler/cvt/cvt_core.h"

#include <stdlib.h>
#include <string.h>

/* One field's bits, copied so the description outlives the call. Every
   element must be 0 or 1: a byte of 101 is not a bit, and masking it (what
   dp_wfm_seq_bits would do) turns a caller's mistake -- a digit string passed
   where bits belong -- into a valid-looking field. An empty array is an absent
   field: `s` stays zero-length and nothing is allocated. A length with no
   array is a contradiction, not an absence, and is refused. Returns 0, or -1
   on a non-bit element or a length with no array. */
static int
literal_fill (wfm_seq_t *s, uint8_t **own, const uint8_t *bits, size_t len)
{
  memset (s, 0, sizeof *s);
  s->kind = WFM_SEQ_LITERAL;
  if (len == 0)
    return 0;
  if (!bits)
    return -1;
  for (size_t i = 0; i < len; i++)
    if (bits[i] > 1u)
      return -1;
  /* Caller-sized, but the caller's array of the same size is already in
     memory, so only a genuine OOM fails this -- the abort-on-OOM helper. */
  *own = dp_xmalloc (len);
  memcpy (*own, bits, len);
  s->bits = *own;
  s->len  = len;
  return 0;
}

/* The three fields and the CRC, shared by both constructors so they cannot
   disagree about what an argument means. They are copied, then described by
   the ONE fixed-layout builder, dp_wfm_frame_fixed -- so a Frame and a
   wfmgen `--sync`/`--crc` frame are the same description, not two layouts
   that happen to agree.

   A preamble has no repetition count of its own: a repeated one is repeated
   in its bits (`field_bits("pn:31:5*4")`), so a supplied one is one
   repetition of itself. Each copy is owned in the slot of the field it
   landed in, which is how dp_frame_add_field owns its copies too. */
static int
frame_init (dp_frame_state_t *obj, const uint8_t *preamble,
            size_t preamble_len, const uint8_t *sync, size_t sync_len,
            const uint8_t *payload, size_t payload_len, int crc)
{
  wfm_seq_t pre, syn, pay;
  uint8_t  *own[3] = { NULL, NULL, NULL };
  if (literal_fill (&pre, &own[0], preamble, preamble_len) != 0
      || literal_fill (&syn, &own[1], sync, sync_len) != 0
      || literal_fill (&pay, &own[2], payload, payload_len) != 0
      || dp_wfm_frame_fixed (&obj->d, &pre, 1u, &syn, &pay, crc) != 0)
    {
      free (own[0]);
      free (own[1]);
      free (own[2]);
      return -1;
    }
  static const char *const name[3] = { "preamble", "sync", "payload" };
  for (int k = 0; k < 3; k++)
    {
      const int i = dp_wfm_frame_field_index (&obj->d, name[k]);
      if (i >= 0)
        obj->own[i] = own[k];
      else
        free (own[k]); /* absent, so nothing was allocated -- free (NULL) */
    }
  return 0;
}

dp_frame_state_t *
dp_frame_create (const uint8_t *preamble, size_t preamble_len,
                 const uint8_t *sync, size_t sync_len, const uint8_t *payload,
                 size_t payload_len, int crc)
{
  dp_frame_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  if (frame_init (obj, preamble, preamble_len, sync, sync_len, payload,
                  payload_len, crc)
          != 0
      || dp_wfm_frame_desc_layout (&obj->d, &obj->dl) != 0
      || (obj->nbits = obj->dl.frame_bits) == 0)
    {
      dp_frame_destroy (obj);
      return NULL;
    }

  /* Materialise now: a descriptor that cannot produce its own bits is not a
     frame, and finding out here is what lets the caller be told at the point
     the mistake was made rather than three calls later. The buffer is then
     what `bits()` repeats. */
  obj->one = dp_xmalloc (obj->nbits);
  if (dp_wfm_frame_assemble (&obj->d, NULL, obj->one, obj->nbits)
      != obj->nbits)
    {
      dp_frame_destroy (obj);
      return NULL;
    }
  obj->built = 1;
  return obj;
}

void
dp_frame_destroy (dp_frame_state_t *state)
{
  if (!state)
    return;
  for (unsigned i = 0; i < WFM_FRAME_MAX_FIELDS; i++)
    free (state->own[i]);
  free (state->one);
  free (state);
}

size_t
dp_frame_bits_max_out (dp_frame_state_t *state, size_t n)
{
  return state ? n * state->nbits : 0;
}

size_t
dp_frame_bits (dp_frame_state_t *state, size_t n, uint8_t *out, size_t max_out)
{
  /* No single frame to repeat: a description with a data field is built and
     checkable but has no bits of its own, and zeros in its place would make
     this a fabricated frame. */
  if (!state || !out || !state->one)
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

int
dp_frame_crc_ok (dp_frame_state_t *state, const uint8_t *rx_bits,
                 size_t rx_bits_len)
{
  if (!state || !rx_bits || rx_bits_len < state->nbits)
    return -1;
  return dp_wfm_frame_desc_crc_ok (&state->d, rx_bits);
}

/* ── the builder ──────────────────────────────────────────────────────
 *
 * The other way in. dp_frame_create() above takes the common frame's three
 * fields and its CRC; these take one field at a time, so a caller can describe
 * a frame that fixed list cannot hold. Both fill the same `d`, and every
 * method above reads only that -- which is the whole reason the two can share
 * them.
 */

dp_frame_state_t *
dp_frame_create_desc (const uint8_t *preamble, size_t preamble_len,
                      const uint8_t *sync, size_t sync_len,
                      const uint8_t *payload, size_t payload_len, int crc)
{
  dp_frame_state_t *obj = dp_xcalloc (1, sizeof (*obj));

  /* The SAME arguments as dp_frame_create, and that is the flavor: this one
     stops before materialising, so the fields are a STARTING POINT a caller
     extends rather than a finished frame. Omit all three to begin from
     nothing -- an EMPTY description rather than an empty payload field,
     which would take index 0 and push the caller's first real field to 1.

     An empty description is therefore legal here and refused there. The
     difference is where completeness can be judged: dp_frame_create()'s
     description is complete when it returns, and this one is not complete
     until dp_frame_build() is called. */
  if (preamble_len + sync_len + payload_len != 0
      && frame_init (obj, preamble, preamble_len, sync, sync_len, payload,
                     payload_len, crc)
             != 0)
    {
      dp_frame_destroy (obj);
      return NULL;
    }
  return obj;
}

int
dp_frame_add_field (dp_frame_state_t *state, const char *name,
                    const uint8_t *bits, size_t bits_len)
{
  if (!state || state->built || !bits || bits_len == 0
      || state->d.n_fields >= WFM_FRAME_MAX_FIELDS)
    return -1;

  wfm_seq_t seq;
  uint8_t  *own = NULL;
  if (literal_fill (&seq, &own, bits, bits_len) != 0)
    return -1;
  /* The general layer appends, names and refuses a duplicate name; this
     object only owns the copy it points at. */
  const int i = dp_wfm_frame_add_field (&state->d, name, &seq, 0u);
  if (i < 0)
    {
      free (own);
      return -1;
    }
  state->own[i] = own;
  return i;
}

int
dp_frame_add_stage (dp_frame_state_t *state, int kind, uint32_t first_field,
                    uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                    uint32_t emit_den, uint32_t unit_bits)
{
  if (!state || state->built)
    return -1;
  /* The general layer appends, and wires a derived last field's producer by
     the same rule the by-name form uses; this object only adds the
     parameters that form does not take. */
  const int i = dp_wfm_frame_add_stage_at (&state->d, (uint32_t)kind,
                                           first_field, n_fields);
  if (i < 0)
    return -1;
  wfm_stage_t *s = &state->d.stage[i];
  s->depth       = depth;
  s->emit_num    = emit_num;
  s->emit_den    = emit_den;
  s->unit_bits   = unit_bits;
  return i;
}

int
dp_frame_build (dp_frame_state_t *state)
{
  if (!state || state->built)
    return -1;
  if (dp_wfm_frame_desc_layout (&state->d, &state->dl) != 0)
    return -1;

  state->nbits = state->dl.out_bits;
  if (state->nbits == 0)
    return -1;

  /* Materialised here for the reason dp_frame_create() materialises in its own
     body: a description that cannot produce its own bits is not a frame, and
     the refusal belongs at the point the caller can still do something about
     it. This is also where a stage naming a kernel nothing here carries is
     refused -- dp_wfm_frame_assemble returns 0 rather than skipping it.

     The CCSDS kernels are what make the coded stages reachable from Python at
     all: ccsds_tm has no binding of its own and is not getting one, so this
     object is where a caller meets the outer code, the randomiser and the
     inner code. The dependency runs frame -> ccsds_tm -> wfm_frame, which is
     the direction ccsds_tm's own CMakeLists anticipated ("BOTH ends want it:
     wfmgen encodes, and frame/ber_meter will decode").

     NULL for the inner encoder's state: a description describes ONE frame, so
     each build starts from the all-zero register. A stream of CADUs sharing
     one register is a transmitter's job, and dp_ccsds_tm_frame_encode is where
     that lives. */
  wfm_frame_ops_t ops;
  dp_ccsds_tm_frame_ops (&ops, NULL);

  /* A data field has no bits until a source draws them, so such a
     description is laid out from lengths (all a receiver needs) and its
     stages are proved runnable over a SCRATCH chunk that is thrown away:
     the same check as below, with nothing kept that could pass for the
     frame. bits() then has no single frame to repeat. */
  int has_data = 0;
  for (unsigned i = 0; i < state->d.n_fields; i++)
    if (!state->d.field[i].derived_by
        && state->d.field[i].seq.kind == WFM_SEQ_DATA
        && state->dl.field_bits[i])
      has_data = 1;

  uint8_t     *frame   = dp_xmalloc (state->nbits);
  uint8_t     *scratch = has_data ? dp_xcalloc (state->nbits, 1) : NULL;
  const size_t n = dp_wfm_frame_assemble_data (&state->d, &ops, scratch, frame,
                                               state->nbits);
  free (scratch);
  if (n != state->nbits)
    {
      free (frame);
      state->nbits = 0;
      return -1;
    }
  if (has_data)
    free (frame);
  else
    state->one = frame;
  state->built = 1;
  return 0;
}

size_t
dp_frame_n_fields (dp_frame_state_t *state)
{
  return state ? state->d.n_fields : 0u;
}

size_t
dp_frame_n_stages (dp_frame_state_t *state)
{
  return state ? state->d.n_stages : 0u;
}

size_t
dp_frame_field_off (dp_frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_fields) ? state->dl.field_off[i] : 0u;
}

size_t
dp_frame_field_bits (dp_frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_fields) ? state->dl.field_bits[i] : 0u;
}

size_t
dp_frame_stage_first (dp_frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_stages) ? state->dl.stage[i].first : 0u;
}

size_t
dp_frame_stage_bits (dp_frame_state_t *state, size_t i)
{
  return (state && i < state->dl.n_stages) ? state->dl.stage[i].n : 0u;
}

size_t
dp_frame_deframe_max_out (dp_frame_state_t *state, size_t rx_bits_len)
{
  (void)rx_bits_len; /* the length is the description's, not the input's */
  return state ? state->dl.frame_bits : 0u;
}

size_t
dp_frame_deframe (dp_frame_state_t *state, const uint8_t *rx_bits,
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
  dp_ccsds_tm_frame_ops (&ops, NULL);
  wfm_frame_rx_t rx;
  const int      verdict = dp_wfm_frame_check (&state->d, &ops, out, &rx);

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
dp_frame_check (dp_frame_state_t *state, const uint8_t *rx_bits,
                size_t rx_bits_len)
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
  dp_ccsds_tm_frame_ops (&ops, NULL);
  wfm_frame_rx_t rx;
  const int      verdict = dp_wfm_frame_check (&state->d, &ops, work, &rx);
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

/* ── naming a description's fields ───────────────────────────────────────
 *
 * The Python-facing half of the by-name builder. Each of these delegates to
 * the wfm_frame_* entry point that owns the rule -- the name lookup, the
 * cover resolution, the derived-producer wiring -- so this layer adds the
 * object's own guard (a built frame is frozen) and the STORAGE, and no
 * arithmetic of its own.
 */

int
dp_frame_field_index (dp_frame_state_t *state, const char *name)
{
  return state ? dp_wfm_frame_field_index (&state->d, name) : -1;
}

int
dp_frame_name_field (dp_frame_state_t *state, uint32_t index, const char *name)
{
  if (!state || state->built || index >= state->d.n_fields)
    return -1;
  /* Refuse a duplicate here too: a rename that collided would make
     field_index answer with whichever field it reached first. */
  const int taken = dp_wfm_frame_field_index (&state->d, name);
  if (taken >= 0 && (uint32_t)taken != index)
    return -1;
  wfm_seq_t keep = state->d.field[index].seq;
  (void)keep;
  {
    size_t n = (name && name[0]) ? strlen (name) : 0u;
    if (n >= WFM_FRAME_NAME_MAX)
      n = WFM_FRAME_NAME_MAX - 1u;
    if (n)
      memcpy (state->d.field[index].name, name, n);
    state->d.field[index].name[n] = '\0';
  }
  return 0;
}

int
dp_frame_add_derived (dp_frame_state_t *state, const char *name, size_t bits)
{
  if (!state || state->built)
    return -1;
  return dp_wfm_frame_add_derived (&state->d, name, bits);
}

int
dp_frame_add_data (dp_frame_state_t *state, const char *name, size_t len)
{
  /* The range the Field grammar accepts for `data:LEN`, so this door
     admits exactly what the text door does. */
  if (!state || state->built || len == 0 || len > WFM_FIELD_MAX_BITS)
    return -1;
  /* The field `data:LEN` parses to: a WFM_SEQ_DATA sequence of that
     length, written once, as the parser writes it. Nothing is owned --
     a data field has no bits of its own to copy. */
  const wfm_seq_t seq = { .kind = WFM_SEQ_DATA, .len = len };
  return dp_wfm_frame_add_field (&state->d, name, &seq, 1u);
}

int
dp_frame_add_stage_over (dp_frame_state_t *state, int kind, const char *first,
                         const char *last, uint32_t depth, uint32_t unit_bits)
{
  if (!state || state->built)
    return -1;
  const int s
      = dp_wfm_frame_add_stage (&state->d, (uint32_t)kind, first, last);
  if (s < 0)
    return -1;
  state->d.stage[s].depth     = depth;
  state->d.stage[s].unit_bits = unit_bits;
  return s;
}
