/**
 * @file test_dp_state.c
 * @brief `dp_state_test.h`'s self-test — and what its one macro does NOT
 * check.
 *
 * `DP_STATE_ROUNDTRIP_TEST` is 12 lines and **31 test files call it**. It is
 * the highest leverage per line in the `dp_*_test.h` family, and it is the
 * only evidence most serializable objects have that their state interface
 * works at all. Nothing tested it.
 *
 * ## Testing a macro that takes a PREFIX
 *
 * The macro pastes `pfx##_state_bytes` / `_get_state` / `_set_state`, so it
 * cannot be tested against a real object without also testing that object.
 * Instead this file defines a **fake** one over the real `dp_state.h`
 * envelope — real header, real `dp_state_validate` — whose `set_state` can be
 * switched between a correct implementation and three broken ones. The macro
 * is then run against each, and the question asked of it is: *did it notice?*
 *
 * Whether it noticed is read from `dp_test.h`'s counters rather than from an
 * exit status, the same trick `test_dp_test.c` uses, so a deliberate failure
 * never reaches this file's own verdict.
 *
 * ## The finding: it is an ENVELOPE test, not a FIDELITY test
 *
 * The macro asserts `set_state` returns `DP_OK` on a good blob and
 * `DP_ERR_INVALID` on a clobbered one. It never asks whether the restored
 * object CARRIES the state it was given.
 *
 * So a `set_state` that validates the envelope, returns `DP_OK`, and restores
 * **nothing** passes at all 31 call sites. That is demonstrated below rather
 * than argued: mode `NOOP_OK` leaves the macro completely silent, while `b`
 * demonstrably still holds its initial state.
 *
 * That gap matters because the project's claim is bit-exact resume — the
 * contributor guide asks each object for "a mid-stream split that resumes
 * bit-for-bit". Every object that meets that does so in its OWN test, by hand.
 * The shared macro, which is what a new object reaches for first, only proved
 * the envelope.
 *
 * A generic fidelity check turns out to be three lines and needs no knowledge
 * of the object: **restore into `b`, re-serialize `b`, and compare the bytes
 * to `a`'s blob.** The standard already guarantees this is well defined —
 * a blob carries only the RUNNING fields, config is restored by `create()`,
 * and `b` is required to be "a fresh object of the same config" — so two
 * objects in the same state must serialize identically. The macro now does
 * that, and this file proves the addition is what catches `NOOP_OK`.
 */
#include "dp_state_test.h"
#include "dp_test.h"

#include "dp_state.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ── A fake serializable object, over the real envelope ─────────────────── */

#define FAKE_MAGIC DP_FOURCC ('F', 'A', 'K', 'E')
#define FAKE_VERSION 1u

typedef struct
{
  uint32_t phase; /* a running field, the kind a blob carries */
  int32_t  acc;   /* another                                  */
} fake_state_t;

/** @brief How `fake_set_state` should misbehave. */
typedef enum
{
  FAKE_CORRECT = 0,    /**< validates, restores — what an object must do    */
  FAKE_ALWAYS_OK,      /**< returns DP_OK without validating the envelope   */
  FAKE_ALWAYS_INVALID, /**< refuses everything, including a good blob       */
  FAKE_NOOP_OK         /**< validates, returns DP_OK, restores NOTHING      */
} fake_mode_t;

static fake_mode_t fake_mode = FAKE_CORRECT;

static size_t
fake_state_bytes (const fake_state_t *s)
{
  (void)s;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t) + sizeof (int32_t);
}

static void
fake_get_state (const fake_state_t *s, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, fake_state_bytes (s));
  dp_w_hdr (&w, FAKE_MAGIC, FAKE_VERSION, fake_state_bytes (s));
  dp_w_bytes (&w, &s->phase, sizeof s->phase);
  dp_w_bytes (&w, &s->acc, sizeof s->acc);
}

static int
fake_set_state (fake_state_t *s, const void *blob)
{
  int rc;

  if (fake_mode == FAKE_ALWAYS_OK)
    return DP_OK;
  if (fake_mode == FAKE_ALWAYS_INVALID)
    return DP_ERR_INVALID;

  rc = dp_state_validate (blob, fake_state_bytes (s), FAKE_MAGIC,
                          FAKE_VERSION);
  if (rc != DP_OK)
    return rc;
  if (fake_mode == FAKE_NOOP_OK)
    return DP_OK; /* the envelope was fine; the payload is dropped */

  {
    dp_reader_t r = dp_reader_init (blob, fake_state_bytes (s));
    r.off         = sizeof (dp_state_hdr_t);
    dp_r_bytes (&r, &s->phase, sizeof s->phase);
    dp_r_bytes (&r, &s->acc, sizeof s->acc);
  }
  return DP_OK;
}

/* ── Observing the macro without being scored by it ─────────────────────── */

static FILE *cap_file  = NULL;
static int   cap_saved = -1;

static void
cap_begin (void)
{
  fflush (stderr);
  cap_file  = tmpfile ();
  cap_saved = dup (fileno (stderr));
  if (cap_file && cap_saved >= 0)
    dup2 (fileno (cap_file), fileno (stderr));
}

static void
cap_end (void)
{
  fflush (stderr);
  if (cap_saved >= 0)
    {
      dup2 (cap_saved, fileno (stderr));
      close (cap_saved);
      cap_saved = -1;
    }
  if (cap_file)
    {
      fclose (cap_file);
      cap_file = NULL;
    }
}

/**
 * @brief Run the macro in @p mode and report how many checks it FAILED.
 *
 * The counters are snapshotted and restored around it, so a deliberate
 * failure inside the macro never reaches this file's verdict — the same
 * mechanism `test_dp_test.c` uses, and for the same reason.
 *
 * @param mode  How `fake_set_state` should misbehave.
 * @param b     Out: the object the macro restored into, for inspection.
 * @return      Failures the macro recorded.
 */
static int
run_macro (fake_mode_t mode, fake_state_t *b)
{
  fake_state_t a  = { 0xDEADBEEFu, -12345 };
  int          c0 = dp_test_checks_, f0 = dp_test_fails_;
  int          fails;

  b->phase  = 0u;
  b->acc    = 0;
  fake_mode = mode;

  cap_begin ();
  DP_STATE_ROUNDTRIP_TEST (fake, &a, b);
  cap_end ();

  fails           = dp_test_fails_ - f0;
  dp_test_checks_ = c0;
  dp_test_fails_  = f0;
  fake_mode       = FAKE_CORRECT;
  return fails;
}

int
main (void)
{
  fake_state_t b;

  printf ("dp_state_test.h self-test — the macro 31 test files call\n");

  /* ── 1. A correct object passes ───────────────────────────────────────── */

  DP_CHECK_MSG (run_macro (FAKE_CORRECT, &b) == 0,
                "a correct state interface passes the macro");
  DP_CHECK_MSG (b.phase == 0xDEADBEEFu && b.acc == -12345,
                "...and the restored object really carries the state");

  /* ── 2. The two halves each catch their own defect ───────────────────────
   */

  /* The reject half. An object that returns DP_OK without validating would
     silently REINTERPRET a foreign blob — which is precisely what the
     envelope exists to prevent, and what the macro's clobber is for. */
  DP_CHECK_MSG (run_macro (FAKE_ALWAYS_OK, &b) > 0,
                "a set_state that accepts a clobbered envelope is caught");

  /* The accept half. Without it, an object that refuses EVERYTHING would pass
     the reject half perfectly — a reject test with no accept beside it is
     satisfied by a function that always fails. */
  DP_CHECK_MSG (run_macro (FAKE_ALWAYS_INVALID, &b) > 0,
                "a set_state that refuses a GOOD blob is caught too, so the "
                "reject half is not vacuous");

  /* ── 3. The gap this file exists to record ────────────────────────────── */

  /* Before the fidelity check below was added to the macro, this was the
     finding: a set_state that validates the envelope, returns DP_OK and
     restores NOTHING passed at all 31 call sites. The macro proved the
     envelope; the bit-exact resume the project actually claims was left
     entirely to each object's own test.

     With the re-serialize comparison in place it is caught, and that is what
     this assertion pins -- if the fidelity check is ever removed, this goes
     red rather than the gap quietly reopening. */
  {
    int fails = run_macro (FAKE_NOOP_OK, &b);
    DP_CHECK_MSG (fails > 0,
                  "a set_state that validates, returns DP_OK and restores "
                  "NOTHING is caught — the macro checks fidelity, not just "
                  "the envelope");
    DP_CHECK_MSG (b.phase == 0u && b.acc == 0,
                  "...and it really did restore nothing, so the assertion "
                  "above is about the macro rather than about the fake");
  }

  /* ── 4. The macro is a single statement ───────────────────────────────── */

  /* `do { } while (0)`, so it composes in an unbraced if/else like every other
     macro in the family. 31 call sites and only one of them has to be inside
     an `if` for this to matter. */
  {
    fake_state_t a2 = { 7u, 9 }, b2 = { 0u, 0 };
    int          taken = 0;
    if (1)
      DP_STATE_ROUNDTRIP_TEST (fake, &a2, &b2);
    else
      taken = 1;
    DP_CHECK_MSG (taken == 0,
                  "the macro is a single statement: an unbraced if/else binds "
                  "its else correctly");
    DP_CHECK (b2.phase == 7u && b2.acc == 9);
  }

  /* ── 5. The clobber really invalidates ────────────────────────────────── */

  /* The macro flips the first byte of the blob, and calls that "the envelope
     magic". Asserted directly, because if the header layout ever moved the
     magic away from offset 0 the clobber would land on a payload byte, the
     envelope would still validate, and the reject half would silently start
     testing nothing at all 31 sites. */
  {
    fake_state_t a3 = { 42u, -7 };
    size_t       cb = fake_state_bytes (&a3);
    char         blob[64];
    DP_REQUIRE (cb <= sizeof blob);
    fake_get_state (&a3, blob);
    DP_CHECK (dp_state_validate (blob, cb, FAKE_MAGIC, FAKE_VERSION) == DP_OK);
    blob[0] ^= (char)0xFF;
    DP_CHECK_MSG (dp_state_validate (blob, cb, FAKE_MAGIC, FAKE_VERSION)
                      == DP_ERR_INVALID,
                  "byte 0 of the blob IS the envelope magic, so the macro's "
                  "clobber invalidates rather than landing on a payload byte");
  }

  DP_TEST_END ("test_dp_state");
}
