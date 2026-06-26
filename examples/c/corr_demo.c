/**
 * corr_demo.c — doppler Corr / Corr2D / Detector demonstration.
 *
 * Four self-contained sections:
 *
 *   §1  Corr     — 1-D circular cross-correlation, exact lag recovery.
 *                  A 64-sample BPSK-PN reference is correlated against a
 *                  circularly shifted copy; peak falls at the injected lag.
 *
 *   §2  Corr     — coherent integrate-and-dump (dwell=4).  Shows per-frame
 *                  accumulation and the dump on the final call.
 *
 *   §3  Corr2D   — 2-D peak recovery.  An 8×8 PN reference is shifted by
 *                  (row=3, col=5); the correlation peak maps back to (3, 5).
 *
 *   §4  Detector — streaming push in 32-sample chunks (half a frame).
 *                  Three detections fire as each complete frame is drained
 *                  from the ring buffer.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/c/corr_demo
 */

#include <corr/corr_core.h>
#include <corr2d/corr2d_core.h>
#include <detector/detector_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Deterministic BPSK PN via xorshift32 ─────────────────────────────────
 * Each call advances the state and returns +1.0 or -1.0 (real, zero imag).
 * The sequence is long-period with good autocorrelation properties.        */
static uint32_t s_rng = 0xBADC0FFEu;

static float
_bpsk (void)
{
  s_rng ^= s_rng << 13;
  s_rng ^= s_rng >> 17;
  s_rng ^= s_rng << 5;
  return (s_rng >> 31) ? 1.0f : -1.0f;
}

static void
fill_pn (float complex *buf, size_t n)
{
  for (size_t k = 0; k < n; k++)
    buf[k] = _bpsk (); /* BPSK: ±1 real, zero imaginary */
}

/* 2-D circular shift: dst[i,j] = src[(i−dr+ny)%ny, (j−dc+nx)%nx] */
static void
circ_shift_2d (float complex *dst, const float complex *src, size_t ny,
               size_t nx, size_t dr, size_t dc)
{
  for (size_t i = 0; i < ny; i++)
    for (size_t j = 0; j < nx; j++)
      dst[i * nx + j] = src[((i + ny - dr) % ny) * nx + ((j + nx - dc) % nx)];
}

/* ── §1  Corr: 1-D lag recovery ─────────────────────────────────────────── */
static void
demo_corr_1d (void)
{
  const size_t N   = 64;
  const size_t LAG = 17;

  float complex ref[64], x[64], out[64];
  fill_pn (ref, N);

  /* Input: ref circularly shifted right by LAG samples. */
  for (size_t k = 0; k < N; k++)
    x[k] = ref[(k + N - LAG) % N];

  corr_state_t *c = corr_create (ref, N, 1, 1, 0);
  corr_execute (c, x, N, out);
  corr_destroy (c);

  /* Find peak. */
  size_t peak_bin = 0;
  float  peak_mag = 0.0f;
  for (size_t k = 0; k < N; k++)
    {
      float m = cabsf (out[k]);
      if (m > peak_mag)
        {
          peak_mag = m;
          peak_bin = k;
        }
    }

  /* Mean sidelobe magnitude (all bins except peak). */
  float sl_sum = 0.0f;
  for (size_t k = 0; k < N; k++)
    if (k != peak_bin)
      sl_sum += cabsf (out[k]);
  float mean_sl = sl_sum / (float)(N - 1);

  printf ("--- §1  Corr: 1-D lag recovery (N=%zu, BPSK-PN, LAG=%zu) ---\n", N,
          LAG);
  /* R[τ] = sum_n x[n]·conj(ref[n-τ]) (correlation theorem, unnormalized).
   * For unit-power BPSK: peak = N (energy of the reference), not 1. */
  printf ("  peak bin : %zu  (expected %zu)\n", peak_bin, LAG);
  printf ("  peak |R| : %.3f  (= N = %zu for unit-power ref)\n", peak_mag, N);
  printf ("  mean sidelobe |R| : %.3f\n", mean_sl);
  printf ("  PSLR : %.1f dB\n\n", 20.0f * log10f (peak_mag / mean_sl));
}

/* ── §2  Corr: coherent integrate-and-dump ─────────────────────────────────
 */
static void
demo_corr_dwell (void)
{
  const size_t N     = 64;
  const size_t DWELL = 4;

  float complex ref[64], out[64];
  fill_pn (ref, N);

  printf ("--- §2  Corr: integrate-and-dump (N=%zu, dwell=%zu) ---\n", N,
          DWELL);
  printf ("  Pushing the reference against itself %zu times.\n", DWELL);
  printf ("  Self-correlation peak at lag 0:"
          " N=%zu per frame; after dwell=%zu: %.1f.\n\n",
          N, DWELL, (float)(DWELL * N));

  corr_state_t *c = corr_create (ref, N, DWELL, 1, 0);
  for (size_t i = 0; i < DWELL; i++)
    {
      size_t n_out = corr_execute (c, ref, N, out);
      if (n_out == 0)
        printf ("  frame %zu/%zu : accumulating  (count=%zu)\n", i + 1, DWELL,
                c->count);
      else
        printf ("  frame %zu/%zu : dump!  "
                "|R[0]| = %.3f  (expected %.1f)\n",
                i + 1, DWELL, cabsf (out[0]), (float)(DWELL * N));
    }
  corr_destroy (c);
  printf ("\n");
}

/* ── §3  Corr2D: 2-D peak recovery ──────────────────────────────────────── */
static void
demo_corr2d (void)
{
  const size_t NY = 8, NX = 8;
  const size_t DR = 3, DC = 5;
  const size_t N = NY * NX;

  float complex ref[64], x[64], out[64];
  fill_pn (ref, N);
  circ_shift_2d (x, ref, NY, NX, DR, DC);

  corr2d_state_t *c = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
  corr2d_execute (c, x, N, out);
  corr2d_destroy (c);

  size_t peak_flat = 0;
  float  peak_mag  = 0.0f;
  for (size_t k = 0; k < N; k++)
    {
      float m = cabsf (out[k]);
      if (m > peak_mag)
        {
          peak_mag  = m;
          peak_flat = k;
        }
    }

  printf ("--- §3  Corr2D: 2-D peak recovery"
          " (NY=%zu NX=%zu, shift=(%zu,%zu)) ---\n",
          NY, NX, DR, DC);
  printf ("  peak (row, col) : (%zu, %zu)  (expected (%zu, %zu))\n",
          peak_flat / NX, peak_flat % NX, DR, DC);
  printf ("  peak |R| : %.6f\n\n", peak_mag);
}

/* ── §4  Detector: streaming push in sub-frame chunks ──────────────────────
 */
static void
demo_detector (void)
{
  const size_t N        = 64;
  const size_t CHUNK    = 32; /* half a frame per push */
  const size_t N_CHUNKS = 6;

  float complex ref[64];
  fill_pn (ref, N);

  printf ("--- §4  Detector: streaming push"
          " (N=%zu, chunk=%zu, dwell=1, threshold=0) ---\n",
          N, CHUNK);
  printf ("  Chunks 1,3,5 buffer the first half of a frame;\n"
          "  chunks 2,4,6 complete it and trigger a detection.\n\n");

  /* threshold=0 → always fire; noise_lo=1 excludes the peak bin (lag=0)
   * from the noise estimate so noise_est reflects the sidelobe floor.     */
  detector_state_t *det
      = detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);

  det_result_t results[4];
  size_t       total = 0;

  for (size_t ch = 0; ch < N_CHUNKS; ch++)
    {
      /* Alternate first and second half of ref so consecutive chunk pairs
       * form one complete N-sample frame that matches the reference.      */
      const float complex *in   = ref + (ch % 2) * CHUNK;
      size_t               ndet = detector_push (det, in, CHUNK, results, 4);

      printf ("  chunk %zu (%zu samples) : ", ch + 1, CHUNK);
      if (ndet == 0)
        {
          printf ("buffering\n");
        }
      else
        {
          for (size_t d = 0; d < ndet; d++)
            printf ("detection  lag=%zu  "
                    "peak=%.4f  noise=%.4f  stat=%.2f\n",
                    results[d].lag, results[d].peak_mag, results[d].noise_est,
                    results[d].test_stat);
        }
      total += ndet;
    }

  printf ("\n  total: %zu detections from %zu chunks\n\n", total, N_CHUNKS);
  detector_destroy (det);
}

int
main (void)
{
  printf ("=== doppler Corr / Corr2D / Detector Demo ===\n\n");
  demo_corr_1d ();
  demo_corr_dwell ();
  demo_corr2d ();
  demo_detector ();
  printf ("Demo complete.\n");
  return 0;
}
