/**
 * test_f32_buffer_core.c — the element-typed face of the f32 ring.
 *
 * The ring's own behaviour (wrap, close, peek, reset, wait_status, threads) is
 * pinned in test_buffer_core.c against the SCALAR face. What is pinned here is
 * the one thing DECLARE_DP_BUFFER_VIEW adds: that the element face addresses
 * the same samples, in samples -- a count that is off by the factor of two
 * between the faces is exactly the defect a cast can hide.
 */
#include "f32_buffer/f32_buffer_core.h"

#define JM_TEST_NAME "test_f32_buffer_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

enum
{
  N = 8
};

int
main (void)
{
  f32_buffer_state_t *ab = dp_f32_create (1024);
  REQUIRE (ab != NULL);

  float _Complex x[N];
  for (int k = 0; k < N; k++)
    x[k] = (float _Complex) (k + 1);

  /* Not yet: the element face says so exactly as the scalar face does. */
  CHECK (dp_f32_peek_view (ab, N) == NULL);

  /* N ELEMENTS in is N samples, not N scalars. */
  CHECK (dp_f32_write_some_view (ab, x, N) == N);
  CHECK (dp_f32_available (ab) == N);

  /* Same memory, both faces: the view is a cast, not a copy. */
  float _Complex *v   = dp_f32_peek_view (ab, N);
  float          *raw = dp_f32_peek (ab, N);
  REQUIRE (v != NULL);
  CHECK ((void *)v == (void *)raw);
  for (int k = 0; k < N; k++)
    {
      CHECK (v[k] == (float _Complex) (k + 1));
      CHECK (raw[2 * k] == (float)(k + 1) && raw[2 * k + 1] == 0.0f);
    }

  /* wait_view is peek_view when the samples are already there. */
  CHECK (dp_f32_wait_view (ab, N) == v);
  dp_f32_consume (ab, N);

  /* write_view is all-or-nothing, in elements: capacity fits, one more
     does not -- and a refusal counts the ELEMENTS it refused. */
  size_t          cap = ab->capacity;
  float _Complex *big = calloc (cap + 1, sizeof *big);
  REQUIRE (big != NULL);
  CHECK (!dp_f32_write_view (ab, big, cap + 1));
  CHECK (ab->dropped == cap + 1);
  CHECK (dp_f32_write_view (ab, big, cap));
  CHECK (dp_f32_space (ab) == 0);
  free (big);

  CHECK (f32_buffer_get_capacity (ab) == cap);
  CHECK (f32_buffer_get_available (ab) == cap);
  CHECK (f32_buffer_get_space (ab) == 0);
  CHECK (f32_buffer_get_dropped (ab) == cap + 1);
  CHECK (!f32_buffer_get_closed (ab));
  dp_f32_close (ab);
  CHECK (f32_buffer_get_closed (ab));

  dp_f32_destroy (ab);
  JM_TEST_EPILOGUE ();
}
