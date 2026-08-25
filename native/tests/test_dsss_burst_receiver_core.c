/**
 * @file test_dsss_burst_receiver_core.c
 * @brief DsssBurstReceiver — lifecycle and argument-validation tests.
 *
 * Phase 4 of the lifecycle spine covers `push()`; this file covers what
 * phase 2/3 built, and deliberately does NOT pretend to cover more. The
 * three stages are unimplemented (see `dsss_burst_receiver_push`), so the
 * only honest claims here are about construction, refusal, derived
 * geometry and reset.
 *
 * Converted off jm's scaffold harness: doppler does not adopt `jm_test.h`
 * (a second assertion harness beside `dp_test.h` is the duplication this
 * repo forbids -- see just-makeit.toml's status_allow note and gh-730), so
 * a scaffolded test is converted to DP_CHECK/DP_REQUIRE on arrival.
 */
#include "dsss_burst_receiver/dsss_burst_receiver_core.h"

#include "dp_test.h"

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD 32u
#define SYNC_LEN 13u

static dsss_burst_receiver_state_t *
make (void)
{
  uint8_t acq[ACQ_SF], data[DATA_SF], sync[SYNC_LEN];
  for (size_t i = 0; i < ACQ_SF; i++)
    acq[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < DATA_SF; i++)
    data[i] = (uint8_t)((i >> 1) & 1u);
  for (size_t i = 0; i < SYNC_LEN; i++)
    sync[i] = (uint8_t)((i * 3u) & 1u);
  return dsss_burst_receiver_create (acq, ACQ_SF, data, DATA_SF, sync,
                                     SYNC_LEN, REPS, SPC, 1.0e6, PAYLOAD, 50.0,
                                     0.0, 1e-3, 0.9, 0.0, 0.0, 10);
}

/* A valid parameter set builds, and the codes are COPIED -- the caller's
 * arrays here are stack locals inside make(), already out of scope by the
 * time this reads back, so a borrowing constructor would show up as garbage
 * geometry rather than as a clean pointer. */
static int
test_create_copies_and_derives (void)
{
  dsss_burst_receiver_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  DP_CHECK (s->acq_code_len == ACQ_SF);
  DP_CHECK (s->data_code_len == DATA_SF);
  DP_CHECK (s->sync_len == SYNC_LEN);
  DP_CHECK (s->acq_code[1] == 1u); /* the copy survived its source */

  /* One preamble repetition, in samples -- the modulus every epoch
   * ambiguity in this object is stated against. */
  DP_CHECK (s->code_period == ACQ_SF * SPC);

  /* preamble + spread (sync | payload | CRC-16) frame. */
  DP_CHECK (s->burst_len
            == (REPS * ACQ_SF + (SYNC_LEN + PAYLOAD + 16u) * DATA_SF) * SPC);

  /* The look-back ring holds detection lag + refine span + the burst
   * itself, rounded to a power of two (§7.1). Checked as ">= the span"
   * rather than "== a literal": pinning the rounded number would pin the
   * rounding, and the CONTRACT is that the span fits. */
  DP_REQUIRE (s->hist != NULL);
  DP_CHECK (s->hist->capacity >= 2u * REPS * s->code_period + s->burst_len);
  DP_CHECK ((s->hist->capacity & (s->hist->capacity - 1u)) == 0u);

  DP_CHECK (s->acq != NULL);
  DP_CHECK (s->demod != NULL);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* Every rejected parameter set is an ARGUMENT error the manifest turns into
 * a ValueError naming the constraint. Each case varies ONE field from the
 * valid set above, so a NULL cannot be attributed to the wrong guard. */
static int
test_refuses_bad_arguments (void)
{
  uint8_t code[ACQ_SF], sync[SYNC_LEN];
  for (size_t i = 0; i < ACQ_SF; i++)
    code[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < SYNC_LEN; i++)
    sync[i] = 0u;

#define MK(ac, acl, dc, dcl, sy, syl, rp, sp, cr, pl, cn, du, fa, pdv)        \
  dsss_burst_receiver_create ((ac), (acl), (dc), (dcl), (sy), (syl), (rp),    \
                              (sp), (cr), (pl), (cn), (du), (fa), (pdv), 0.0, \
                              0.0, 10)

  DP_CHECK (MK (NULL, 0, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                PAYLOAD, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no preamble code */
  DP_CHECK (MK (code, ACQ_SF, NULL, 0, sync, SYNC_LEN, REPS, SPC, 1e6, PAYLOAD,
                50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no data code */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, NULL, 0, REPS, SPC, 1e6, PAYLOAD,
                50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no sync word */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, 0, SPC, 1e6,
                PAYLOAD, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* reps < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, 0, 1e6,
                PAYLOAD, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* spc < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 0.0,
                PAYLOAD, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6, 0,
                50.0, 0.0, 1e-3, 0.9)
            == NULL); /* payload_len < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                PAYLOAD, 50.0, 0.0, 1.0, 0.9)
            == NULL); /* pfa out of (0,1) */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                PAYLOAD, 50.0, 0.0, 1e-3, 0.0)
            == NULL); /* pd out of (0,1) */

  /* The control: the same call with every field valid DOES build, so the
   * nine NULLs above are attributable to the field each varied and not to
   * a constructor that refuses everything. */
  dsss_burst_receiver_state_t *ok
      = MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
            PAYLOAD, 50.0, 0.0, 1e-3, 0.9);
  DP_CHECK (ok != NULL);
  dsss_burst_receiver_destroy (ok);
#undef MK
  return 0;
}

/* push() is unimplemented, so the ONLY claim available is that it is a legal
 * no-op: it reports no completed burst for any input and advances the stream
 * position. Pinned so that phase 3 landing a real implementation has to
 * change a test rather than silently repurpose a passing one. */
static int
test_push_is_a_declared_noop_until_phase_3 (void)
{
  dsss_burst_receiver_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  float complex x[64];
  for (size_t i = 0; i < 64; i++)
    x[i] = 0.0f + 0.0f * I;

  uint8_t out[PAYLOAD];
  DP_CHECK (dsss_burst_receiver_push (s, x, 64, out, PAYLOAD) == 0);
  DP_CHECK (s->samples_fed == 64);
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) == 0);
  DP_CHECK (dsss_burst_receiver_get_frame_valid (s) == 0);

  /* The buffer a caller must provide is the payload length, whatever the
   * input block size -- at most one burst is returned per call. */
  DP_CHECK (dsss_burst_receiver_push_max_out (s, 1) == PAYLOAD);
  DP_CHECK (dsss_burst_receiver_push_max_out (s, 1u << 20) == PAYLOAD);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* reset() clears every read-back, and the PRECONDITION is asserted first --
 * otherwise the check passes against state that was already zero, which is
 * the vacuous-reset shape this campaign keeps finding (burst_demod F4).
 * The lifetime counters deliberately survive. */
static int
test_reset_clears_the_event_but_not_the_counters (void)
{
  dsss_burst_receiver_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  /* Stand in for a completed burst. Written directly because push() cannot
   * produce one yet; phase 3 replaces this with a real burst. */
  s->preamble_start = 4096;
  s->frame_valid    = 1;
  s->doppler_hz_est = 1234.5;
  s->est_snr_db     = 12.0;
  s->refine_margin  = 0.75;
  s->pending        = 2;
  s->n_bursts       = 7;
  s->dropped        = 3;
  s->samples_fed    = 99999;

  DP_REQUIRE (s->frame_valid == 1 && s->doppler_hz_est != 0.0);

  dsss_burst_receiver_reset (s);

  DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == 0);
  DP_CHECK (dsss_burst_receiver_get_frame_valid (s) == 0);
  DP_CHECK (dsss_burst_receiver_get_doppler_hz_est (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_est_snr_db (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_refine_margin (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_pending (s) == 0);
  DP_CHECK (s->samples_fed == 0);

  /* Lifetime, on purpose: a reset that zeroed these could hide that the
   * receiver had already lost samples. */
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) == 7);
  DP_CHECK (dsss_burst_receiver_get_dropped (s) == 3);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* destroy(NULL) is a no-op: the create() failure path calls it on a
 * partially built object, so this is the guard that makes `goto fail` safe
 * rather than a double-free waiting for an allocation to fail. */
static int
test_destroy_null_is_safe (void)
{
  dsss_burst_receiver_destroy (NULL);
  DP_CHECK (1);
  return 0;
}

int
main (void)
{
  if (test_create_copies_and_derives ())
    return 1;
  if (test_refuses_bad_arguments ())
    return 1;
  if (test_push_is_a_declared_noop_until_phase_3 ())
    return 1;
  if (test_reset_clears_the_event_but_not_the_counters ())
    return 1;
  if (test_destroy_null_is_safe ())
    return 1;
  DP_TEST_END ("test_dsss_burst_receiver_core");
}
