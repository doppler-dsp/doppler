/**
 * test_i16_buffer_core.c — the element-typed face of the i16 ring.
 *
 * The ring's own behaviour (wrap, close, peek, reset, wait_status, threads) is
 * pinned in test_buffer_core.c against the SCALAR face. What is pinned here is
 * the one thing DECLARE_DP_BUFFER_VIEW adds: that the element face addresses
 * the same samples, in samples -- a count that is off by the factor of two
 * between the faces is exactly the defect a cast can hide.
 */
#include "i16_buffer/i16_buffer_core.h"

#define JM_TEST_NAME "test_i16_buffer_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

enum
{
  N = 8
};

int
main (void)
{
  i16_buffer_state_t *ab = dp_i16_create (1024);
  REQUIRE (ab != NULL);

  dp_iq16_t x[N];
  for (int k = 0; k < N; k++)
    x[k] = (dp_iq16_t){ (int16_t)(k + 1), (int16_t)-(k + 1) };

  /* Not yet: the element face says so exactly as the scalar face does. */
  CHECK (dp_i16_peek_view (ab, N) == NULL);

  /* N ELEMENTS in is N samples, not N scalars. */
  CHECK (dp_i16_write_some_view (ab, x, N) == N);
  CHECK (dp_i16_available (ab) == N);

  /* Same memory, both faces: the view is a cast, not a copy. */
  dp_iq16_t *v   = dp_i16_peek_view (ab, N);
  int16_t   *raw = dp_i16_peek (ab, N);
  REQUIRE (v != NULL);
  CHECK ((void *)v == (void *)raw);
  for (int k = 0; k < N; k++)
    {
      CHECK (v[k].i == (int16_t)(k + 1) && v[k].q == (int16_t)-(k + 1));
      CHECK (raw[2 * k] == (int16_t)(k + 1)
             && raw[2 * k + 1] == (int16_t)-(k + 1));
    }

  /* wait_view is peek_view when the samples are already there. */
  CHECK (dp_i16_wait_view (ab, N) == v);
  dp_i16_consume (ab, N);

  /* write_view is all-or-nothing, in elements: capacity fits, one more
     does not -- and a refusal counts the ELEMENTS it refused. */
  size_t     cap = ab->capacity;
  dp_iq16_t *big = calloc (cap + 1, sizeof *big);
  REQUIRE (big != NULL);
  CHECK (!dp_i16_write_view (ab, big, cap + 1));
  CHECK (ab->dropped == cap + 1);
  CHECK (dp_i16_write_view (ab, big, cap));
  CHECK (dp_i16_space (ab) == 0);
  free (big);

  CHECK (i16_buffer_get_capacity (ab) == cap);
  CHECK (i16_buffer_get_available (ab) == cap);
  CHECK (i16_buffer_get_space (ab) == 0);
  CHECK (i16_buffer_get_dropped (ab) == cap + 1);
  CHECK (!i16_buffer_get_closed (ab));
  dp_i16_close (ab);
  CHECK (i16_buffer_get_closed (ab));

  dp_i16_destroy (ab);
  JM_TEST_EPILOGUE ();
}
