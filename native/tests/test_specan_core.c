#include "doppler/dp_complex.h"
#include "doppler/specan/specan_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

/* Fill buf with a unit-rate complex exponential of normalised frequency fn
 * (cycles/sample) and amplitude amp. */
static void
gen_tone (float _Complex *buf, size_t len, double fn, double amp)
{
  for (size_t k = 0; k < len; k++)
    buf[k] = (float)(amp * cos (2.0 * PI * fn * (double)k))
             + (float)(amp * sin (2.0 * PI * fn * (double)k)) * I;
}

/* Drive obj with `total` input samples in `chunk`-sized blocks; return the
 * first emitted frame in out (capacity cap), or 0 if none appeared. */
static size_t
drive_first_frame (dp_specan_state_t *obj, const float _Complex *tone,
                   size_t total, size_t chunk, float *out, size_t cap)
{
  for (size_t i = 0; i < total; i += chunk)
    {
      size_t m  = (i + chunk <= total) ? chunk : total - i;
      size_t no = dp_specan_execute (obj, tone + i, m, out, cap);
      if (no)
        return no;
    }
  return 0;
}

static size_t
argmax (const float *a, size_t n)
{
  size_t mi = 0;
  for (size_t i = 1; i < n; i++)
    if (a[i] > a[mi])
      mi = i;
  return mi;
}

int
main (void)
{
  const double    fs = 1.0e6, span = 200e3, rbw = 1500.0;
  const size_t    NTONE = 1u << 14; /* 16384 input samples per drive */
  float _Complex *tone  = malloc (NTONE * sizeof *tone);
  float          *out   = malloc (2048 * sizeof *out);
  DP_CHECK (tone && out);
  if (!tone || !out)
    return 1;

  /* 1. Invalid arguments are rejected (no opaque NULL surprises). */
  DP_CHECK (dp_specan_create (0.0, span, rbw, 0, 0, 0, 1.0, 0, 1, 1)
            == NULL); /* fs   */
  DP_CHECK (dp_specan_create (fs, 0.0, rbw, 0, 0, 0, 1.0, 0, 1, 1)
            == NULL); /* span */
  DP_CHECK (dp_specan_create (fs, span, 0.0, 0, 0, 0, 1.0, 0, 1, 1)
            == NULL); /* rbw  */
  DP_CHECK (dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 0)
            == NULL); /* navg */

  /* 2. The natural params derive a sane DSP grid. */
  dp_specan_state_t *sa
      = dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 1);
  DP_CHECK (sa != NULL);
  if (!sa)
    return 1;
  DP_CHECK (dp_near (sa->fs_out, span * 1.28, 1.0)); /* span -> decim rate */
  DP_CHECK ((sa->nfft & (sa->nfft - 1)) == 0);       /* FFT is a power of 2 */
  DP_CHECK (sa->nfft >= 2 * sa->n && sa->nfft < 4 * sa->n); /* pad >= 2    */
  DP_CHECK (sa->disp_n % 2 == 1); /* odd, DC-centred     */
  DP_CHECK (sa->disp_lo + sa->disp_n <= sa->nfft);
  double realized_rbw = sa->psd->enbw * sa->fs_out / (double)sa->n;
  DP_CHECK (dp_near (realized_rbw, rbw, rbw * 0.05)); /* RBW met within 5% */
  DP_CHECK (sa->beta > 0.0);                          /* Kaiser actually used*/

  /* 2b. Every RBW gets the same skirt. The window length used to be
   * next_pow_two(fs_out/rbw), leaving the Kaiser ENBW target anywhere in [1,
   * 2) bins; an RBW of fs_out/2^k asked for exactly 1 bin, got beta = 0 -- a
   * rectangle -- and painted -13 dB sidelobes around every tone. 1000, 2000
   * and 4000 Hz are exactly that case here (fs_out = 256 kHz). For each, a
   * noiseless tone must sit >= 80 dB above every bin outside its main lobe
   * (Kaiser beta ~12: null at ~4 bins of n, so 5 bins of n in display bins
   * of nfft clears it). */
  {
    const double rbws[] = { 1000.0, 2000.0, 4000.0, 1500.0, 1999.0, 3000.0 };
    for (size_t r = 0; r < sizeof rbws / sizeof rbws[0]; r++)
      {
        dp_specan_state_t *sk
            = dp_specan_create (fs, span, rbws[r], 0, 0, 0, 1.0, 0, 1, 1);
        DP_CHECK (sk != NULL);
        if (!sk)
          continue;
        DP_CHECK (sk->psd->enbw >= 2.0 - 1e-3); /* never below 2 bins */
        DP_CHECK (dp_near (sk->psd->enbw * sk->fs_out / (double)sk->n, rbws[r],
                           rbws[r] * 0.01)); /* RBW exact      */
        gen_tone (tone, NTONE, 30e3 / fs, 1.0);
        size_t nf = drive_first_frame (sk, tone, NTONE, 4096, out, 2048);
        DP_CHECK (nf == sk->disp_n);
        size_t pkk   = argmax (out, nf);
        size_t guard = 5 * sk->nfft / sk->n;
        float  worst = -1e30f;
        for (size_t i = 0; i < nf; i++)
          if ((i + guard < pkk || i > pkk + guard) && out[i] > worst)
            worst = out[i];
        if (out[pkk] - worst < 80.0f)
          printf ("rbw %.0f: skirt only %.1f dB below the tone (beta %.2f)\n",
                  rbws[r], (double)(out[pkk] - worst), sk->beta);
        DP_CHECK (out[pkk] - worst >= 80.0f);
        dp_specan_destroy (sk);
      }
  }

  /* 2c. Integrated noise power confirms the ENBW per bin. Complex white noise
   * of unit variance has density N0 = 1/fs, so every display bin must read
   * N0 * rbw = rbw / fs -- the RBW measured from the OUTPUT, not read back
   * from the window's own enbw field. Averaged over 64+ frames and the
   * central 80% of the display (the DDC passband edge is clear of it); the
   * scatter is then ~0.01 dB, so 0.1 dB is a bias, not noise. This checks
   * calibration, not the skirt: a rectangle meets its RBW too, which is why
   * 2b exists. */
  {
    const double rbws[] = { 1000.0, 2000.0, 4000.0, 1500.0, 3000.0 };
    float _Complex nz[4096];
    double *acc = malloc (2048 * sizeof *acc);
    DP_CHECK (acc != NULL);
    for (size_t r = 0; acc && r < sizeof rbws / sizeof rbws[0]; r++)
      {
        dp_specan_state_t *sn
            = dp_specan_create (fs, span, rbws[r], 0, 0, 0, 1.0, 0, 1, 16);
        DP_CHECK (sn != NULL);
        if (!sn)
          continue;
        uint32_t st     = 0x5eed0u + (uint32_t)r;
        size_t   frames = 0, m = 0;
        memset (acc, 0, 2048 * sizeof *acc);
        while (frames < 64)
          {
            for (size_t i = 0; i < 4096; i++)
              nz[i] = dp_cgauss (&st); /* E|z|^2 = 1 */
            size_t no = dp_specan_execute (sn, nz, 4096, out, 2048);
            if (!no)
              continue;
            m = no;
            for (size_t i = 0; i < no; i++)
              acc[i] += pow (10.0, (double)out[i] / 10.0);
            frames++;
          }
        double sum = 0.0;
        size_t lo = m / 10, hi = m - m / 10;
        for (size_t i = lo; i < hi; i++)
          sum += acc[i] / (double)frames;
        double per_bin = sum / (double)(hi - lo);
        double err_db  = 10.0 * log10 (per_bin * fs / rbws[r]);
        if (fabs (err_db) > 0.1)
          printf ("rbw %.0f: noise reads %+.3f dB off N0*RBW\n", rbws[r],
                  err_db);
        DP_CHECK (fabs (err_db) <= 0.1);
        dp_specan_destroy (sn);
      }
    free (acc);
  }

  /* 3. A unit tone at +30 kHz lands at +30 kHz in the display, near 0 dB. */
  const double f_off = 30e3;
  gen_tone (tone, NTONE, f_off / fs, 1.0);
  size_t nfr = drive_first_frame (sa, tone, NTONE, 4096, out, 2048);
  DP_CHECK (nfr == sa->disp_n);
  size_t pk     = argmax (out, nfr);
  size_t dc_bin = sa->disp_n / 2; /* odd length -> exact DC-centred index */
  double bin_hz = sa->fs_out / (double)sa->nfft;
  double pk_hz  = ((double)pk - (double)dc_bin) * bin_hz;
  DP_CHECK (dp_near (pk_hz, f_off, bin_hz)); /* within one display bin  */
  DP_CHECK (out[pk] > -3.0);                 /* ~0 dBFS for amplitude 1 */
  DP_CHECK (out[pk] - out[5] > 30.0);        /* tone clears far bins     */

  /* 4. Retuning to the tone moves it to DC (cheap LO retune, no rebuild). */
  dp_specan_retune (sa, f_off);
  size_t nfr2 = drive_first_frame (sa, tone, NTONE, 4096, out, 2048);
  DP_CHECK (nfr2 == sa->disp_n);
  size_t pk2 = argmax (out, nfr2);
  DP_CHECK (llabs ((long long)pk2 - (long long)dc_bin) <= 2);
  dp_specan_destroy (sa);

  /* 5. navg buffers a full averaging window before emitting a frame. */
  dp_specan_state_t *sb
      = dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 2);
  DP_CHECK (sb != NULL);
  if (sb)
    {
      /* One window length of input is far short of n*navg decimated. */
      DP_CHECK (dp_specan_execute (sb, tone, sb->n, out, 2048) == 0);
      size_t nfr3 = drive_first_frame (sb, tone, NTONE, 4096, out, 2048);
      DP_CHECK (nfr3 == sb->disp_n);
      dp_specan_destroy (sb);
    }

  free (tone);
  free (out);

  /* The pending buffer must stay bounded by one window, however much input
   * arrives per call. It did not: one call emitted one spectrum and kept the
   * rest of a large block buffered, so `pend_len` grew by the leftover every
   * call and never came back down. That leaked memory, staled the displayed
   * frame, and -- because dp_specan_state_bytes reserves exactly n*navg
   * samples for `pend` -- made dp_specan_get_state write past the end of the
   * caller's blob. Driven here well past the point where the old code had
   * already overrun that reservation (268 by the first call, 2146 by the
   * eighth). */
  {
    float _Complex big[4096];
    float sout[2048];
    for (int i = 0; i < 4096; i++)
      big[i] = (float)(i % 7) - 3.0f + 0.2f * I;
    dp_specan_state_t *g
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 1, 2);
    DP_CHECK (g != NULL);
    if (g)
      {
        size_t need = g->n * g->navg, worst = 0;
        for (int k = 0; k < 64; k++)
          {
            (void)dp_specan_execute (g, big, 4096, sout, 2048);
            if (g->pend_len > worst)
              worst = g->pend_len;
          }
        DP_CHECK (worst < need);
        /* and the blob the fixed-size reservation promises still fits */
        DP_CHECK (g->pend_len * sizeof (float _Complex)
                  <= need * sizeof (float _Complex));
        dp_specan_destroy (g);
      }
  }

  /* serializable state — ddc + psd children + pending samples resume. */
  {
    float _Complex in[4096];
    float out[2048];
    for (int i = 0; i < 4096; i++)
      in[i] = (float)(i % 7) - 3.0f + 0.2f * I;
    dp_specan_state_t *a
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 1, 2);
    dp_specan_state_t *b
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 1, 2);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_specan_execute (a, in, 4096, out, 2048);
    DP_STATE_ROUNDTRIP_TEST (dp_specan, a, b);
    DP_CHECK (b->pend_len == a->pend_len);
    dp_specan_destroy (a);
    dp_specan_destroy (b);
  }

  DP_TEST_END ("test_specan_core");
}
