#define DP_TEST_VERBOSE 1
#include "doppler/awgn/awgn_core.h"
#include "doppler/dp_complex.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_STAT 65536 /* samples for statistical checks */
#define N_SMALL 256

/* ------------------------------------------------------------------
 * test_lifecycle: create, destroy, NULL safety.
 * ------------------------------------------------------------------ */
static void
test_lifecycle (void)
{
  printf ("\n-- Lifecycle --\n");
  dp_awgn_state_t *g = dp_awgn_create (0, 1.0f);
  DP_CHECK (g != NULL);
  dp_awgn_destroy (g);
  dp_awgn_destroy (NULL); /* must be a no-op */
  DP_CHECK (1);           /* no crash */
}

/* ------------------------------------------------------------------
 * test_amplitude_property: get/set without disturbing RNG.
 * ------------------------------------------------------------------ */
static void
test_amplitude_property (void)
{
  printf ("\n-- Amplitude property --\n");
  dp_awgn_state_t *g = dp_awgn_create (1, 2.0f);
  DP_CHECK (dp_awgn_get_amplitude (g) == 2.0f);
  dp_awgn_set_amplitude (g, 0.5f);
  DP_CHECK (dp_awgn_get_amplitude (g) == 0.5f);
  dp_awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_zero_amplitude: all outputs must be exactly 0+0j.
 * ------------------------------------------------------------------ */
static void
test_zero_amplitude (void)
{
  printf ("\n-- Zero amplitude --\n");
  dp_awgn_state_t *g = dp_awgn_create (0, 0.0f);
  float _Complex buf[N_SMALL];
  dp_awgn_generate (g, N_SMALL, buf, N_SMALL);
  int all_zero = 1;
  for (int i = 0; i < N_SMALL; i++)
    if (buf[i] != 0.0f + 0.0f * I)
      all_zero = 0;
  DP_CHECK (all_zero);
  dp_awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_reset_reproducible: reset reproduces identical output.
 * ------------------------------------------------------------------ */
static void
test_reset_reproducible (void)
{
  printf ("\n-- Reset reproducible --\n");
  dp_awgn_state_t *g = dp_awgn_create (42, 1.0f);
  float _Complex a[N_SMALL], b[N_SMALL];

  dp_awgn_generate (g, N_SMALL, a, N_SMALL);
  dp_awgn_reset (g);
  dp_awgn_generate (g, N_SMALL, b, N_SMALL);

  DP_CHECK (memcmp (a, b, N_SMALL * sizeof *a) == 0);
  dp_awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_reseed: different seeds produce different streams.
 * ------------------------------------------------------------------ */
static void
test_reseed (void)
{
  printf ("\n-- Reseed --\n");
  dp_awgn_state_t *g = dp_awgn_create (1, 1.0f);
  float _Complex a[N_SMALL], b[N_SMALL];

  dp_awgn_generate (g, N_SMALL, a, N_SMALL);
  dp_awgn_reseed (g, 2);
  dp_awgn_generate (g, N_SMALL, b, N_SMALL);

  int differs = 0;
  for (int i = 0; i < N_SMALL; i++)
    if (a[i] != b[i])
      differs = 1;
  DP_CHECK (differs);

  /* reseed back to 1 should reproduce stream a */
  dp_awgn_reseed (g, 1);
  float _Complex c[N_SMALL];
  dp_awgn_generate (g, N_SMALL, c, N_SMALL);
  DP_CHECK (memcmp (a, c, N_SMALL * sizeof *a) == 0);
  dp_awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_statistics: mean ≈ 0, variance ≈ amplitude² per component.
 * ------------------------------------------------------------------ */
static void
test_statistics (void)
{
  printf ("\n-- Statistics (N=%d) --\n", N_STAT);
  const float      amp = 2.0f;
  dp_awgn_state_t *g   = dp_awgn_create (7, amp);

  float _Complex *buf = malloc (N_STAT * sizeof *buf);
  dp_awgn_generate (g, N_STAT, buf, N_STAT);

  double sum_re = 0, sum_im = 0;
  double sum_re2 = 0, sum_im2 = 0;
  for (int i = 0; i < N_STAT; i++)
    {
      double re = (double)crealf (buf[i]);
      double im = (double)cimagf (buf[i]);
      sum_re += re;
      sum_im += im;
      sum_re2 += re * re;
      sum_im2 += im * im;
    }
  double mean_re = sum_re / N_STAT;
  double mean_im = sum_im / N_STAT;
  double var_re  = sum_re2 / N_STAT - mean_re * mean_re;
  double var_im  = sum_im2 / N_STAT - mean_im * mean_im;

  /* Mean within ±3σ/√N of 0 (3*amp/√65536 ≈ 0.023 for amp=2) */
  double mean_tol = 3.0 * amp / sqrt ((double)N_STAT);
  DP_CHECK (fabs (mean_re) < mean_tol);
  DP_CHECK (fabs (mean_im) < mean_tol);

  /* Variance within 2% of amp² */
  double var_tol = 0.02 * amp * amp;
  DP_CHECK (fabs (var_re - amp * amp) < var_tol);
  DP_CHECK (fabs (var_im - amp * amp) < var_tol);

  free (buf);
  dp_awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_split_block: split into two calls == one contiguous call.
 * ------------------------------------------------------------------ */
static void
test_split_block (void)
{
  printf ("\n-- Split-block identity --\n");
  float _Complex full[N_SMALL], part[N_SMALL];

  /* Full block */
  dp_awgn_state_t *g = dp_awgn_create (99, 1.0f);
  dp_awgn_generate (g, N_SMALL, full, N_SMALL);
  dp_awgn_destroy (g);

  /* Two halves */
  g           = dp_awgn_create (99, 1.0f);
  size_t half = N_SMALL / 2;
  dp_awgn_generate (g, half, part, half);
  dp_awgn_generate (g, half, part + half, half);
  dp_awgn_destroy (g);

  DP_CHECK (memcmp (full, part, N_SMALL * sizeof *full) == 0);
}

/* ------------------------------------------------------------------
 * test_oneshot: dp_awgn() matches dp_awgn_create+generate+destroy.
 * ------------------------------------------------------------------ */
static void
test_oneshot (void)
{
  printf ("\n-- One-shot dp_awgn() --\n");

  float _Complex ref[N_SMALL];
  dp_awgn_state_t *g = dp_awgn_create (42, 0.7f);
  DP_CHECK (g != NULL);
  dp_awgn_generate (g, N_SMALL, ref, N_SMALL);
  dp_awgn_destroy (g);

  float _Complex out[N_SMALL];
  DP_CHECK (dp_awgn (42, 0.7f, N_SMALL, out) == 0);
  DP_CHECK (memcmp (ref, out, N_SMALL * sizeof *out) == 0);
}

/* Advance the RNG, serialize, restore into a fresh generator, and the noise
 * stream continues bit-for-bit; a clobbered envelope rejects. */
static void
test_state_roundtrip (void)
{
  printf ("\n-- Serializable state round-trip --\n");
  enum
  {
    M = 64
  };
  float _Complex ref[M], got[M];

  dp_awgn_state_t *a = dp_awgn_create (123, 1.0f);
  dp_awgn_generate (a, M, ref, M); /* advance past the seed state */
  size_t sb   = dp_awgn_state_bytes (a);
  void  *blob = malloc (sb);
  dp_awgn_get_state (a, blob);
  dp_awgn_generate (a, M, ref, M); /* reference continuation */

  dp_awgn_state_t *b = dp_awgn_create (123, 1.0f);
  DP_CHECK (dp_awgn_set_state (b, blob) == DP_OK);
  ((char *)blob)[0] ^= (char)0xFF;
  DP_CHECK (dp_awgn_set_state (b, blob) == DP_ERR_INVALID);
  dp_awgn_generate (b, M, got, M);
  DP_CHECK (memcmp (ref, got, sizeof ref) == 0);

  dp_awgn_destroy (a);
  dp_awgn_destroy (b);
  free (blob);

  /* The amplitude travels (#2084): set after create, it is a mutator's value
     and so state. A target created at another amplitude resumes at the
     source's, and continues with the same output. */
  dp_awgn_state_t *src = dp_awgn_create (7, 1.0f);
  dp_awgn_state_t *dst = dp_awgn_create (7, 1.0f);
  dp_awgn_generate (src, M, ref, M);
  dp_awgn_set_amplitude (src, 2.5f);
  dp_awgn_set_amplitude (dst, 0.25f); /* another value: must not survive */
  void *b2 = malloc (dp_awgn_state_bytes (src));
  dp_awgn_get_state (src, b2);
  DP_CHECK (dp_awgn_set_state (dst, b2) == DP_OK);
  DP_CHECK (dp_awgn_get_amplitude (dst) == 2.5f);
  dp_awgn_generate (src, M, ref, M);
  dp_awgn_generate (dst, M, got, M);
  DP_CHECK (memcmp (ref, got, sizeof ref) == 0);
  dp_awgn_destroy (src);
  dp_awgn_destroy (dst);
  free (b2);

  /* The seed travels too (#2084): reseed() writes it and reset() reseeds
     from it, so it is a mutator's value. Restored, then reset, the target
     restarts the source's stream, not its own constructor seed's. */
  src = dp_awgn_create (0, 1.0f);
  dst = dp_awgn_create (0, 1.0f);
  dp_awgn_reseed (src, 9);
  dp_awgn_generate (src, M, ref, M);
  void *b3 = malloc (dp_awgn_state_bytes (src));
  dp_awgn_get_state (src, b3);
  DP_CHECK (dp_awgn_set_state (dst, b3) == DP_OK);
  dp_awgn_reset (src);
  dp_awgn_reset (dst);
  dp_awgn_generate (src, M, ref, M);
  dp_awgn_generate (dst, M, got, M);
  DP_CHECK (memcmp (ref, got, sizeof ref) == 0);

  /* The version is checked on its own. A v1 blob is also a different size,
     so the size check refused it first and pinned nothing: a blob of
     today's size that claims version 1 must be refused too. */
  dp_state_hdr_t hdr;
  memcpy (&hdr, b3, sizeof hdr);
  hdr.version = 1;
  memcpy (b3, &hdr, sizeof hdr);
  DP_CHECK (dp_awgn_set_state (dst, b3) == DP_ERR_INVALID);
  dp_awgn_destroy (src);
  dp_awgn_destroy (dst);
  free (b3);
}

/* ------------------------------------------------------------------
 * test_forged_state_refused: a bad amplitude or an all-zero RNG state in a
 * live blob is refused, and the target is left byte-identical (#2084).
 *
 * set_state() restored the amplitude with no check, so a NaN or negative
 * sigma came back as the generator's amplitude, and an all-zero s[4] is
 * xoshiro256++'s fixed point, which emits zeros forever. The create and the
 * setter take the same amplitude predicate: create refuses it, and the setter
 * ignores it. The blob layout is [hdr][u64 s[4]][u64 seed][f32 amplitude];
 * each case forges one field and compares the whole state, not one value.
 * ------------------------------------------------------------------ */
static void
test_forged_state_refused (void)
{
  printf ("\n-- Forged state refused --\n");
  const float    nanf = NAN, negf = -1.0f, inff = INFINITY;
  const uint64_t zero4[4] = { 0, 0, 0, 0 };

  /* The create and the setter take the same predicate. */
  DP_CHECK (dp_awgn_create (1, nanf) == NULL);
  DP_CHECK (dp_awgn_create (1, inff) == NULL);
  DP_CHECK (dp_awgn_create (1, negf) == NULL);
  dp_awgn_state_t *g = dp_awgn_create (1, 2.0f);
  DP_CHECK (g != NULL);
  if (!g)
    return;
  dp_awgn_set_amplitude (g, nanf);
  dp_awgn_set_amplitude (g, inff);
  dp_awgn_set_amplitude (g, negf);
  DP_CHECK (dp_awgn_get_amplitude (g) == 2.0f); /* ignored, not taken */

  dp_awgn_state_t *src    = dp_awgn_create (5, 1.0f);
  dp_awgn_state_t *dst    = dp_awgn_create (5, 1.0f);
  const size_t     sb     = dp_awgn_state_bytes (src);
  unsigned char   *blob   = malloc (sb);
  unsigned char   *before = malloc (sb);
  unsigned char   *after  = malloc (sb);
  dp_awgn_get_state (src, blob);
  const size_t base = sizeof (dp_state_hdr_t);
  const size_t amp  = base + 4 * sizeof (uint64_t) + sizeof (uint64_t);

  struct
  {
    size_t      off, len;
    const void *val;
    const char *what;
  } forge[] = {
    { amp, sizeof nanf, &nanf, "a NaN amplitude" },
    { amp, sizeof inff, &inff, "an infinite amplitude" },
    { amp, sizeof negf, &negf, "a negative amplitude" },
    { base, sizeof zero4, zero4, "an all-zero RNG state" },
  };
  for (size_t k = 0; k < sizeof forge / sizeof *forge; k++)
    {
      dp_awgn_get_state (dst, before);
      unsigned char *bad = malloc (sb);
      memcpy (bad, blob, sb);
      memcpy (bad + forge[k].off, forge[k].val, forge[k].len);
      const int rc = dp_awgn_set_state (dst, bad);
      dp_awgn_get_state (dst, after);
      if (rc != DP_ERR_INVALID)
        fprintf (stderr, "  forged blob accepted: %s\n", forge[k].what);
      DP_CHECK (rc == DP_ERR_INVALID);
      DP_CHECK (memcmp (after, before, sb) == 0);
      free (bad);
    }

  /* The unforged blob still restores, so the refusals are not the whole
     path failing. */
  DP_CHECK (dp_awgn_set_state (dst, blob) == DP_OK);

  dp_awgn_destroy (g);
  dp_awgn_destroy (src);
  dp_awgn_destroy (dst);
  free (blob);
  free (before);
  free (after);
}

/* ------------------------------------------------------------------
 * test_stream_pinned: the sequence is PINNED, not merely self-consistent.
 *
 * gh-690: this generator shipped with two implementations selected at run
 * time, and they produced different noise from the same seed — the scalar
 * state s[0..3] and the eight AVX-512 streams vs[0..3][0..7] come from
 * different SplitMix64 draws. Every AWGN-derived number was therefore
 * platform-dependent, silently.
 *
 * Nothing here caught it, and test_reset_reproducible() is why: it compares
 * a stream to ITSELF after dp_awgn_reset(), on one path, on one machine. That
 * passes identically under either implementation. Self-consistency is not
 * reproducibility, and only an external reference can tell them apart.
 *
 * So: reference values, recorded from the scalar path, which is the one
 * every shipped Linux build has always run. A future re-vectorisation is
 * welcome and has to reproduce these.
 *
 * The RNG and the LUT are integer and table lookups, so the real and
 * imaginary parts are exactly reproducible; the tolerance is for logf, which
 * libm does not guarantee to the last ulp across platforms. It is far below
 * the ~0.3 spacing between consecutive samples, so a WRONG stream cannot
 * hide inside it — which is the property that matters, and the reason a
 * tolerance is honest here rather than a loophole.
 * ------------------------------------------------------------------ */
static void
test_stream_pinned (void)
{
  printf ("\n-- Stream pinned (gh-690) --\n");
  static const float want_re[]
      = { -0.268593192f, -0.054472364f, -0.578596532f, -1.609373569f };
  static const float want_im[]
      = { 0.581977606f, -0.171774969f, -0.357516527f, -1.250267267f };

  dp_awgn_state_t *g = dp_awgn_create (42, 1.0f);
  float _Complex out[4];
  dp_awgn_generate (g, 4, out, 4);
  for (size_t i = 0; i < 4; i++)
    {
      DP_CHECK_NEAR (crealf (out[i]), want_re[i], 1e-5);
      DP_CHECK_NEAR (cimagf (out[i]), want_im[i], 1e-5);
    }
  dp_awgn_destroy (g);

  /* The head being right does not mean the state update is. */
  dp_awgn_state_t *h = dp_awgn_create (12345, 1.0f);
  float _Complex buf[4096];
  double acc_re = 0.0, acc_im = 0.0;
  for (int r = 0; r < 64; r++)
    {
      dp_awgn_generate (h, 4096, buf, 4096);
      for (size_t i = 0; i < 4096; i++)
        {
          acc_re += (double)crealf (buf[i]);
          acc_im += (double)cimagf (buf[i]);
        }
    }
  DP_CHECK_NEAR (acc_re, 17.390397, 0.05);
  DP_CHECK_NEAR (acc_im, -47.646037, 0.05);
  dp_awgn_destroy (h);
}

int
main (void)
{
  test_lifecycle ();
  test_amplitude_property ();
  test_zero_amplitude ();
  test_reset_reproducible ();
  test_stream_pinned ();
  test_reseed ();
  test_statistics ();
  test_split_block ();
  test_oneshot ();
  test_state_roundtrip ();
  test_forged_state_refused ();

  printf ("\n");
  DP_TEST_END ("test_awgn_core");
}
