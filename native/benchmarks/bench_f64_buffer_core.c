/* bench_f64_buffer_core.c — the element-typed face of the f64 ring.
 *
 * The ring's headline numbers (write+wait, write_some+peek, the wrap, the
 * streaming loop) are bench_buffer_core.c's, on the scalar face. This times
 * the same frame through the ELEMENT face, so a cast that stopped being free
 * -- a copy, a length fix-up -- would show here as a difference between the
 * two files rather than nowhere. */
#include "doppler/f64_buffer/f64_buffer_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FRAME 1024
#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  dp_f64_buffer_state_t *ab     = dp_f64_create (16 * FRAME);
  jm_bench_t             _bench = { 0 };
  static double _Complex x[FRAME];
  static double times[ITERATIONS];
  size_t        sink = 0;

  if (!ab)
    return 1;
  printf ("=== f64_buffer benchmark ===\n");
  printf ("frame = %d samples,  %d iterations\n\n", FRAME, ITERATIONS);

  for (int r = 0; r < ITERATIONS; r++)
    {
      uint64_t t0 = jm_bench_now_ns ();
      for (int k = 0; k < BENCH_N / FRAME; k++)
        {
          dp_f64_write_some_view (ab, x, FRAME);
          double _Complex *v = dp_f64_peek_view (ab, FRAME);
          sink += (v != NULL);
          dp_f64_consume (ab, FRAME);
        }
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "write_some_view+peek_view", times, ITERATIONS,
                BENCH_N);

  jm_bench_write_json (&_bench, "f64_buffer");
  dp_f64_destroy (ab);
  return sink == 0;
}
