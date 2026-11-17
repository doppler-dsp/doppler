/* main.c -- the C twin of main.cpp, and its oracle (doppler#3108).
 *
 * The same calls on the same data, written the way a C caller writes them.
 * `make cxx-headers-check` builds both against one libdoppler.a and
 * requires identical output, so what the C++ program got through the
 * headers is what C gets. Keep the two in step: a change to one is a
 * change to both.
 */
#include <stdio.h>

#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/lo/lo_core.h"
#include "doppler/util/util_core.h"

#define N 64

int
main (void)
{
  const float taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
  float _Complex x[N], y[N], d[N];

  dp_lo_state_t  *lo  = dp_lo_create (0.0625);
  dp_fir_state_t *fir = dp_fir_create_real (taps, 5);
  dp_cic_state_t *cic = dp_cic_create (4);
  if (!lo || !fir || !cic)
    return 2;

  if (dp_lo_steps (lo, N, x, N) != N)
    return 3;
  if (dp_fir_execute (fir, x, N, y) != N)
    return 4;
  /* C inlines the header's dp_cic_decimate; C++ links the library's copy
     of the same body. */
  size_t nd = dp_cic_decimate (cic, y, N, d, N);
  if (nd != N / 4)
    return 5;

  for (size_t i = 0; i < N; i++)
    {
      float _Complex z = dp_square_clip (y[i], 0.5f);
      printf ("%a %a %a %a\n", (double)crealf (y[i]), (double)cimagf (y[i]),
              (double)crealf (z), (double)cimagf (z));
    }

  for (size_t i = 0; i < nd; i++)
    printf ("%a %a\n", (double)crealf (d[i]), (double)cimagf (d[i]));

  dp_cic_destroy (cic);
  dp_fir_destroy (fir);
  dp_lo_destroy (lo);
  return 0;
}
