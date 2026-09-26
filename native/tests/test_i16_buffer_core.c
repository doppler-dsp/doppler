/**
 * test_i16_buffer_core.c — the element-typed face of the i16 ring.
 *
 * The ring's own behaviour (wrap, close, peek, reset, wait_status, threads) is
 * pinned in test_buffer_core.c against the SCALAR face. What is pinned here is
 * the one thing DECLARE_DP_BUFFER_VIEW adds: that the element face addresses
 * the same samples, in samples -- a count that is off by the factor of two
 * between the faces is exactly the defect a cast can hide.
 */
#include "doppler/i16_buffer/i16_buffer_core.h"

#include "dp_test.h"
#include <stdlib.h>

enum
{
  N = 8
};

int
main (void)
{
  dp_i16_buffer_state_t *ab = dp_i16_create (1024);
  DP_REQUIRE (ab != NULL);

  dp_iq16_t x[N];
  for (int k = 0; k < N; k++)
    x[k] = (dp_iq16_t){ (int16_t)(k + 1), (int16_t)-(k + 1) };

  /* Not yet: the element face says so exactly as the scalar face does. */
  DP_CHECK (dp_i16_peek_view (ab, N) == NULL);

  /* N ELEMENTS in is N samples, not N scalars. */
  DP_CHECK (dp_i16_write_some_view (ab, x, N) == N);
  DP_CHECK (dp_i16_available (ab) == N);

  /* Same memory, both faces: the view is a cast, not a copy. */
  dp_iq16_t *v   = dp_i16_peek_view (ab, N);
  int16_t   *raw = dp_i16_peek (ab, N);
  DP_REQUIRE (v != NULL);
  DP_CHECK ((void *)v == (void *)raw);
  for (int k = 0; k < N; k++)
    {
      DP_CHECK (v[k].i == (int16_t)(k + 1) && v[k].q == (int16_t)-(k + 1));
      DP_CHECK (raw[2 * k] == (int16_t)(k + 1)
                && raw[2 * k + 1] == (int16_t)-(k + 1));
    }

  /* wait_view is peek_view when the samples are already there. */
  DP_CHECK (dp_i16_wait_view (ab, N) == v);
  dp_i16_consume (ab, N);

  /* write_view is all-or-nothing, in elements: capacity fits, one more
     does not -- and a refusal counts the ELEMENTS it refused. */
  size_t     cap = ab->capacity;
  dp_iq16_t *big = calloc (cap + 1, sizeof *big);
  DP_REQUIRE (big != NULL);
  DP_CHECK (!dp_i16_write_view (ab, big, cap + 1));
  DP_CHECK (ab->dropped == cap + 1);
  DP_CHECK (dp_i16_write_view (ab, big, cap));
  DP_CHECK (dp_i16_space (ab) == 0);
  free (big);

  DP_CHECK (dp_i16_buffer_get_capacity (ab) == cap);
  DP_CHECK (dp_i16_buffer_get_available (ab) == cap);
  DP_CHECK (dp_i16_buffer_get_space (ab) == 0);
  DP_CHECK (dp_i16_buffer_get_dropped (ab) == cap + 1);
  DP_CHECK (!dp_i16_buffer_get_closed (ab));
  dp_i16_close (ab);
  DP_CHECK (dp_i16_buffer_get_closed (ab));

  dp_i16_destroy (ab);
  DP_TEST_END ("test_i16_buffer_core");
}
