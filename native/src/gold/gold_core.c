#include "gold/gold_core.h"

/*
 * CCSDS Command Link Gold Code Generator (CCSDS 415.0-G-1 5.2.2.4).
 *
 * Two `length`-bit Fibonacci LFSRs free-run in lock-step; each chip is the
 * XOR of both registers' current top bit (stage `length`). Both shift left
 * by one bit per chip: feedback = parity of the tapped stages (read before
 * the shift), entering at stage 1 (bit 0); the old top bit is discarded
 * after being read. With primitive taps this gives period 2^length - 1 per
 * register, and the CCSDS-fixed (taps_a, taps_b) pair is a genuine
 * "preferred pair" — the XOR family has a strict three-valued
 * autocorrelation/cross-correlation set (verified in
 * native/tests/test_gold_core.c). Output chips are 0/1 (uint8); map to ±1
 * downstream.
 */

gold_state_t *
gold_create (uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
             uint64_t seed_b, uint32_t length)
{
  /* all-zero register is a fixed point; register holds up to 64 bits */
  if (seed_a == 0 || seed_b == 0 || length == 0 || length > 64)
    return NULL;
  gold_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->mask   = (length >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << length) - 1u);
  obj->taps_a = taps_a & obj->mask;
  obj->taps_b = taps_b & obj->mask;
  obj->seed_a = seed_a & obj->mask;
  obj->seed_b = seed_b & obj->mask;
  obj->reg_a  = obj->seed_a;
  obj->reg_b  = obj->seed_b;
  obj->length = length;
  return obj;
}

void
gold_destroy (gold_state_t *state)
{
  free (state);
}

void
gold_reset (gold_state_t *state)
{
  state->reg_a = state->seed_a;
  state->reg_b = state->seed_b;
}

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * Only the running LFSR registers; taps / seeds / mask / length are config
 * restored by create(). */

size_t
gold_state_bytes (const gold_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + 2 * sizeof (uint64_t);
}

void
gold_get_state (const gold_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, gold_state_bytes (state));
  dp_w_hdr (&w, GOLD_STATE_MAGIC, GOLD_STATE_VERSION,
            gold_state_bytes (state));
  dp_w_u64 (&w, state->reg_a);
  dp_w_u64 (&w, state->reg_b);
}

int
gold_set_state (gold_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, gold_state_bytes (state), GOLD_STATE_MAGIC,
                              GOLD_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, gold_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->reg_a  = dp_r_u64 (&r);
  state->reg_b  = dp_r_u64 (&r);
  return DP_OK;
}

size_t
gold_generate_max_out (gold_state_t *state)
{
  (void)state;
  return 0; /* output length is the caller-requested n */
}

size_t
gold_generate (gold_state_t *state, size_t n, uint8_t *out)
{
  uint64_t       a      = state->reg_a;
  uint64_t       b      = state->reg_b;
  const uint64_t taps_a = state->taps_a;
  const uint64_t taps_b = state->taps_b;
  const uint64_t mask   = state->mask;
  const uint32_t top    = state->length - 1u;
  for (size_t i = 0; i < n; i++)
    {
      out[i]        = (uint8_t)(((a >> top) ^ (b >> top)) & 1u);
      uint64_t fb_a = (uint64_t)__builtin_parityll (a & taps_a);
      uint64_t fb_b = (uint64_t)__builtin_parityll (b & taps_b);
      a             = ((a << 1) | fb_a) & mask;
      b             = ((b << 1) | fb_b) & mask;
    }
  state->reg_a = a;
  state->reg_b = b;
  return n;
}
