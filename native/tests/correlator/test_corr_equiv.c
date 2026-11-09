/* test_corr_equiv.c -- the 1-D correlator is the 2-D one at one row, byte
 * for byte (#2752 step 3.2, docs/design/correlator.md section 2.1).
 *
 * Step 3 retires `Corr` by making it the single-row flavor of the one
 * correlator: `dp_correlator_create_1d (ref, n, ...)` becomes `Corr2D`'s
 * single-row path at ny = 1. That is a refactor only if the two compute the
 * same bits, so this test drives both through identical calls and compares
 * everything a caller can see:
 *
 *   - every call's return: 0 between dumps, n_out on a dump, SIZE_MAX on a
 *     refused frame;
 *   - every output buffer, by memcmp, never by tolerance (a 1-ulp
 *     difference is a different object), past the dump's end too;
 *   - the running state: the count, and the state blob, which is one
 *     layout for both and differs only in its first field, the flavor (a
 *     reject key, so a 1-D blob never restores into a 2-D correlator);
 *   - a dump into a short `out`, a new reference mid-dwell, a reset
 *     mid-dwell and a refused frame, each once a stream.
 *
 * The grid is the design page's prototype, committed: n = 31, 64, 100 and
 * 1023 (prime, a power of two, smooth, and 2^k - 1), dwells 1 and 3, and lag
 * pads 1 and 2 (the native inverse, and the decoupled one that zero-pads the
 * spectrum). The 2-D side runs serial and on every core: at one row its pool
 * must not change a bit.
 *
 * It ran first against Corr's and Corr2D's own code (dp_corr_* and
 * dp_corr2d_*, in the commit "test(corr): Corr is Corr2D at one row, byte
 * for byte"; cited by subject because a rebase changes its hash), where the
 * 1-D path was a copy, and was proven red by sabotage there. Against the
 * flavors it asserts the same things, so the copy's deletion changed no
 * caller's bits.
 *
 * Today it is circular for the arithmetic: dp_correlator_create_1d IS
 * dp_correlator_create_2d at ny = 1, so both sides run the same code and
 * cannot disagree. It is kept for what still differs between them, the
 * flavor field and the blob's flavor reject, and for the layout. The
 * arithmetic is held by test_correlator_1d.c's analytic cases, which assert
 * peak values against closed forms, including at n_out > n.
 */
#include "doppler/correlator/correlator_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define N_LEN 4
#define N_DWELL 2
#define N_PAD 2
static const size_t lens[N_LEN]     = { 31, 64, 100, 1023 };
static const size_t dwells[N_DWELL] = { 1, 3 };
static const size_t pads[N_PAD]     = { 1, 2 };

/* Dumps a stream runs to. Dump 0 is plain; dump 1 goes into a short output;
   dump 2's dwell starts over on a new reference; dump 3's on a reset. */
#define N_DUMPS 4

/* The two objects' blobs: equal size; each names its own flavor first, and
   the rest of the payload is equal. */
static int
check_blobs (const dp_correlator_state_t *a, dp_correlator_state_t *b)
{
  const size_t na = dp_correlator_state_bytes (a);
  const size_t nb = dp_correlator_state_bytes (b);
  DP_REQUIRE (na == nb);
  unsigned char *ba = malloc (na);
  unsigned char *bb = malloc (nb);
  DP_REQUIRE (ba && bb);
  dp_correlator_get_state (a, ba);
  dp_correlator_get_state (b, bb);
  const size_t h = sizeof (dp_state_hdr_t);
  uint64_t     fa, fb;
  memcpy (&fa, ba + h, sizeof fa);
  memcpy (&fb, bb + h, sizeof fb);
  DP_CHECK (fa == DP_CORRELATOR_1D && fb == DP_CORRELATOR_2D);
  const size_t k = h + sizeof fa;
  DP_CHECK (memcmp (ba + k, bb + k, na - k) == 0);
  /* The flavor refuses: a 1-D blob does not restore into the 2-D twin. */
  DP_CHECK (dp_correlator_set_state (b, ba) == DP_ERR_INVALID);
  free (ba);
  free (bb);
  return 0;
}

/* One stream through both objects. Returns 0, or 1 when a REQUIRE failed;
 *dumps receives the dumps compared. */
static int
run_case (size_t n, size_t dwell, size_t pad, int nthreads, uint32_t seed,
          size_t *dumps)
{
  const size_t    no   = pad * n;
  float _Complex *ref  = malloc (n * sizeof *ref);
  float _Complex *ref2 = malloc (n * sizeof *ref2);
  float _Complex *x    = malloc (n * sizeof *x);
  float _Complex *o1   = malloc (no * sizeof *o1);
  float _Complex *o2   = malloc (no * sizeof *o2);
  DP_REQUIRE (ref && ref2 && x && o1 && o2);
  uint32_t st = seed;
  for (size_t i = 0; i < n; i++)
    ref[i] = dp_cgauss (&st);
  for (size_t i = 0; i < n; i++)
    ref2[i] = dp_cgauss (&st);

  dp_correlator_state_t *a = dp_correlator_create_1d (ref, n, dwell, 1, no);
  dp_correlator_state_t *b
      = dp_correlator_create_2d (ref, 1, n, dwell, nthreads, 0, no, -1);
  DP_REQUIRE (a && b);
  DP_CHECK (b->fast_path == 1); /* one row is always the single-row path */
  DP_CHECK (dp_correlator_execute_max_out (a)
            == dp_correlator_execute_max_out (b));

  int new_ref = 0, was_reset = 0;
  *dumps = 0;
  while (*dumps < N_DUMPS)
    {
      for (size_t i = 0; i < n; i++)
        x[i] = dp_cgauss (&st);

      /* Mid-dwell where the dwell allows it, at the dwell's start where it
         does not: both reset the accumulator and the count. */
      const size_t mid = dwell > 1 ? 1 : 0;
      if (*dumps == 2 && !new_ref && a->count == mid)
        {
          dp_correlator_set_ref (a, ref2);
          DP_CHECK (dp_correlator_set_ref (b, ref2) == 0);
          new_ref = 1;
        }
      if (*dumps == 3 && !was_reset && a->count == mid)
        {
          if (check_blobs (a, b))
            return 1;
          dp_correlator_reset (a);
          dp_correlator_reset (b);
          was_reset = 1;
        }

      const size_t cap = (*dumps == 1) ? no / 2 + 1 : no;

      /* A frame one sample short is refused by both, counted by neither. */
      DP_CHECK (dp_correlator_execute (a, x, n - 1, o1, cap, NULL)
                == SIZE_MAX);
      DP_CHECK (dp_correlator_execute (b, x, n - 1, o2, cap, NULL)
                == SIZE_MAX);

      memset (o1, 0xA5, no * sizeof *o1);
      memset (o2, 0xA5, no * sizeof *o2);
      const size_t r1 = dp_correlator_execute (a, x, n, o1, cap, NULL);
      const size_t r2 = dp_correlator_execute (b, x, n, o2, cap, NULL);
      DP_CHECK (r1 == r2);
      DP_CHECK (a->count == b->count);
      DP_CHECK (memcmp (o1, o2, no * sizeof *o1) == 0);
      if (r1 != 0)
        {
          DP_CHECK (r1 == cap);
          (*dumps)++;
        }
      else if (check_blobs (a, b))
        return 1;
    }
  DP_CHECK (new_ref && was_reset);

  dp_correlator_destroy (a);
  dp_correlator_destroy (b);
  free (ref);
  free (ref2);
  free (x);
  free (o1);
  free (o2);
  return 0;
}

int
main (void)
{
  uint32_t seed  = 0x2752u;
  size_t   cases = 0;
  for (size_t l = 0; l < N_LEN; l++)
    for (size_t d = 0; d < N_DWELL; d++)
      for (size_t p = 0; p < N_PAD; p++)
        for (int t = 0; t < 2; t++)
          {
            /* nthreads 1, then 0: every online core, capped at the row. */
            size_t dumps = 0;
            DP_REQUIRE (run_case (lens[l], dwells[d], pads[p], t ? 0 : 1,
                                  seed++, &dumps)
                        == 0);
            DP_CHECK (dumps == N_DUMPS);
            cases++;
          }
  DP_CHECK (cases == (size_t)N_LEN * N_DWELL * N_PAD * 2);
  DP_TEST_END ("test_corr_equiv");
}
