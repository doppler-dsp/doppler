/*
 * test_burst_acq_core.c — BurstAcquisition thin-forwarder C-level tests.
 *
 * burst_acq_core.c is a pure forwarder onto acq_core.c's shared engine (see
 * burst_acq_core.h's file doc comment) -- the underlying physics are already
 * exhaustively covered by test_acq_core.c against acq_create_burst()
 * directly. This test only proves the forwarding itself is correct: a
 * real construction succeeds, push()/reset()/configure_search_raw() reach
 * the embedded engine, and the state triplet round-trips.
 */
#include "burst_acq/burst_acq_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                          \
  do                                                                         \
    {                                                                        \
      if (!(cond))                                                           \
        {                                                                    \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
          _fails++;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

int
main (void)
{
  int _fails = 0;

  /* NULL/zero-length code is rejected, same as acq_create_burst() itself. */
  CHECK (burst_acq_create (NULL, 0, 1, 4, 1000000.0, 50.0, 0.0, 1e-3, 0.9, 0)
         == NULL);

  const size_t spc   = 2;
  const size_t nx    = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate = 1.0e6;

  burst_acq_state_t *obj
      = burst_acq_create (CODE7, 7, 8, spc, crate, 65.0, 0.0, 1e-2, 0.9, 0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* Noise-free burst at zero Doppler/code-phase -- exercises push()
   * reaching the embedded engine. */
  float complex *burst = malloc (2 * nx * sizeof (float complex));
  CHECK (burst != NULL);
  if (burst)
    {
      for (size_t k = 0; k < 2 * nx; k++)
        {
          uint8_t chip = CODE7[(k / spc) % 7];
          burst[k]     = (chip & 1u) ? -1.0f : 1.0f;
        }
      acq_result_t hits[4];
      size_t       nh = burst_acq_push (obj, burst, 2 * nx, hits, 4);
      CHECK (nh == 1);
      if (nh == 1)
        {
          CHECK (hits[0].doppler_bin == 0);
          CHECK (hits[0].code_phase == 0);
        }
      free (burst);
    }

  burst_acq_reset (obj);

  /* configure_search_raw reaches the embedded engine. */
  CHECK (burst_acq_configure_search_raw (obj, 3, 1) == 0);
  CHECK (burst_acq_configure_search_raw (obj, 0, 1) == -1); /* out of range */

  /* State triplet round-trip. */
  size_t nbytes = burst_acq_state_bytes (obj);
  CHECK (nbytes > 0);
  void *blob = malloc (nbytes);
  CHECK (blob != NULL);
  if (blob)
    {
      burst_acq_get_state (obj, blob);
      CHECK (burst_acq_set_state (obj, blob) == 0);
      ((char *)blob)[0] ^= (char)0xFF; /* clobber the header magic */
      CHECK (burst_acq_set_state (obj, blob) != 0);
      free (blob);
    }

  burst_acq_destroy (obj);
  burst_acq_destroy (NULL); /* must not crash */

  if (_fails)
    {
      fprintf (stderr, "test_burst_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_burst_acq_core PASSED\n");
  return 0;
}
