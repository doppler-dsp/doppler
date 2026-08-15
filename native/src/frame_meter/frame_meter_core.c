/*
 * frame_meter_core.c — frame outcomes accumulated across a record. The
 * contract, and why each rule is the way it is, live on the declarations in
 * frame_meter/frame_meter_core.h.
 */
#include "frame_meter/frame_meter_core.h"

#include <stdlib.h>

frame_meter_state_t *
frame_meter_create (size_t target_errors, double conf)
{
  if (conf < 0.0 || conf >= 1.0)
    return NULL;
  frame_meter_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;
  s->target_errors = target_errors ? target_errors : BER_TARGET_ERRORS;
  s->conf          = (conf > 0.0) ? conf : BER_CONF;
  return s;
}

void
frame_meter_destroy (frame_meter_state_t *state)
{
  free (state);
}

void
frame_meter_reset (frame_meter_state_t *state)
{
  if (!state)
    return;
  state->frames        = 0;
  state->sync_detected = 0;
  state->crc_passed    = 0;
  state->errors        = 0;
}

void
frame_meter_add (frame_meter_state_t *state, int sync_ok, int crc)
{
  if (!state)
    return;
  state->frames++;
  if (!sync_ok)
    {
      /* Not detected is not delivered. Nothing else about the frame can be
         read, so neither the CRC nor its absence says anything here. */
      state->errors++;
      return;
    }
  state->sync_detected++;
  if (crc > 0)
    state->crc_passed++;
  else if (crc == 0)
    state->errors++; /* found, and wrong */
  /* crc < 0: no check carried, and a detected frame counts as delivered --
     counting it as an error would measure the frame format, not the receiver.
   */
}

size_t
frame_meter_get_frames (const frame_meter_state_t *state)
{
  return state->frames;
}

size_t
frame_meter_get_sync_detected (const frame_meter_state_t *state)
{
  return state->sync_detected;
}

size_t
frame_meter_get_crc_passed (const frame_meter_state_t *state)
{
  return state->crc_passed;
}

size_t
frame_meter_get_errors (const frame_meter_state_t *state)
{
  return state->errors;
}

int
frame_meter_get_enough (const frame_meter_state_t *state)
{
  return state->errors >= state->target_errors;
}

ber_interval_t
frame_meter_fer (const frame_meter_state_t *state)
{
  return ber_confidence (state->errors, state->frames, state->conf);
}

ber_interval_t
frame_meter_sync_miss (const frame_meter_state_t *state)
{
  return ber_confidence (state->frames - state->sync_detected, state->frames,
                         state->conf);
}

/* ── Serializable state — the envelope plus the four running counters. The
 * configuration (target, confidence) is restored by create(), not carried in
 * the blob; see dp_state.h. ─────────────────────────────────────────────── */

size_t
frame_meter_state_bytes (const frame_meter_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + 4u * sizeof (uint64_t);
}

void
frame_meter_get_state (const frame_meter_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, frame_meter_state_bytes (state));
  dp_w_hdr (&w, FRAME_METER_STATE_MAGIC, FRAME_METER_STATE_VERSION,
            frame_meter_state_bytes (state));
  dp_w_u64 (&w, (uint64_t)state->frames);
  dp_w_u64 (&w, (uint64_t)state->sync_detected);
  dp_w_u64 (&w, (uint64_t)state->crc_passed);
  dp_w_u64 (&w, (uint64_t)state->errors);
}

int
frame_meter_set_state (frame_meter_state_t *state, const void *blob)
{
  int rc
      = dp_state_validate (blob, frame_meter_state_bytes (state),
                           FRAME_METER_STATE_MAGIC, FRAME_METER_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, frame_meter_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->frames = (size_t)dp_r_u64 (&r);
  state->sync_detected = (size_t)dp_r_u64 (&r);
  state->crc_passed    = (size_t)dp_r_u64 (&r);
  state->errors        = (size_t)dp_r_u64 (&r);
  return DP_OK;
}
