/*
 * main.c — a minimal downstream consumer of the doppler C library.
 *
 * Runs the local oscillator at a quarter of the sample rate, where its output
 * is known exactly: the phasor turns 90 degrees per sample, cycling
 * 1, j, -1, -j. Exit status 0 means the library linked, ran and produced
 * those samples -- not merely that it linked.
 *
 * It then does the three things a header-only reader of the API would not
 * expect to matter to a LINK, and do (doppler#1450). Most of doppler's C
 * surface is inline -- every `step()`, the thread wrappers, the whole ring
 * buffer -- so the calls those bodies make land in THIS object file, and the
 * linker resolves them from what the consumer was handed, never from
 * libdoppler's own dependencies:
 *
 *   - an inline `agc_step()`, whose body calls libm;
 *   - a `dp_thread.h` thread, which is pthread on POSIX;
 *   - a ring buffer, which maps its own pages.
 *
 * With only `-ldoppler` on the line the first fails everywhere and the second
 * on any glibc older than 2.34. `lo` alone never showed it: its kernel is
 * out of line, so nothing of libm's is referenced from here.
 *
 * It uses only the pure-C core, so it builds and runs on every platform
 * doppler publishes for, Windows included, and links with exactly what
 * find_package or pkg-config hand out -- nothing added by hand. It writes no
 * file.
 *
 * Builds against either link mode; see CMakeLists.txt (find_package) and the
 * pkg-config commands in docs/install/c.md.
 */
#include "doppler/dp_complex.h"
#include "doppler/dp_thread.h"
#include <doppler/agc/agc_core.h>
#include <doppler/buffer/buffer.h>
#include <doppler/lo/lo_core.h>
#include <math.h>
#include <stdio.h>

#define N 8

/* Runs on its own thread and reports back through the argument, so the join
 * is observable rather than assumed. */
DP_THREAD_FN (set_flag, arg)
{
  *(int *)arg = 1;
  DP_THREAD_RETURN;
}

/* The inline surface. `n` comes from argc so the compiler cannot fold the
 * loop away and with it the libm references the test exists to create. */
static int
inline_surface (int n)
{
  int bad = 0;

  /* An AGC driven by a constant converges on its target level: the inline
   * step() ran, and its libm calls resolved. */
  agc_state_t *agc = agc_create (0.0, 0.0025, 0.05);
  if (!agc)
    return 1;
  float _Complex y = 0.0f;
  for (int i = 0; i < 4000 * n; i++)
    y = agc_step (agc, 0.1f);
  agc_destroy (agc);
  if (fabsf (cabsf (y) - 1.0f) > 0.05f)
    {
      fprintf (stderr, "agc settled at %f, want 1.0\n", (double)cabsf (y));
      bad++;
    }

  int         flag = 0;
  dp_thread_t t;
  if (dp_thread_create (&t, set_flag, &flag) != 0)
    return bad + 1;
  dp_thread_join (t);
  if (!flag)
    {
      fprintf (stderr, "the joined thread never ran\n");
      bad++;
    }

  dp_f32_t *ring = dp_f32_create (1000);
  if (!ring)
    {
      fprintf (stderr, "dp_f32_create failed\n");
      return bad + 1;
    }
  dp_f32_destroy (ring);
  return bad;
}

int
main (int argc, char **argv)
{
  (void)argv;
  static const float want_re[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
  static const float want_im[4] = { 0.0f, 1.0f, 0.0f, -1.0f };

  lo_state_t *lo = lo_create (0.25);
  if (!lo)
    {
      fprintf (stderr, "lo_create failed\n");
      return 1;
    }
  float _Complex x[N];
  lo_steps (lo, N, x, N);
  lo_destroy (lo);

  int bad = 0;
  for (int i = 0; i < N; i++)
    if (fabsf (crealf (x[i]) - want_re[i % 4]) > 1e-5f
        || fabsf (cimagf (x[i]) - want_im[i % 4]) > 1e-5f)
      bad++;
  printf ("lo at fs/4: %d of %d samples off the 1, j, -1, -j cycle\n", bad, N);

  int inl = inline_surface (argc);
  printf ("inline surface (agc step, thread, ring): %d failure(s)\n", inl);
  return bad != 0 || inl != 0;
}
