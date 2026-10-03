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

/* Display capacity: rbw = 250 Hz at fs_out = 256 kHz is nfft 2048, 1601
 * bins; the narrowest in the sweep. */
#define OUTCAP 4096u

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
  float          *out   = malloc (OUTCAP * sizeof *out);
  DP_CHECK (tone && out);
  if (!tone || !out)
    return 1;

  /* 1. Invalid arguments are rejected (no opaque NULL surprises). 0 is
   * auto for span and rbw (rule 1, rule 3), so only negatives are refused. */
  DP_CHECK (dp_specan_create (0.0, span, rbw, 0, 0, 0, 1.0, 0, 1)
            == NULL); /* fs   */
  DP_CHECK (dp_specan_create (fs, -1.0, rbw, 0, 0, 0, 1.0, 0, 1)
            == NULL); /* span */
  DP_CHECK (dp_specan_create (fs, span, -1.0, 0, 0, 0, 1.0, 0, 1)
            == NULL); /* rbw  */
  DP_CHECK (dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 0)
            == NULL); /* navg */

  /* 2. The three rules of docs/design/specan.md, as the state reports them.
   * fs = 1 MHz and span = 200 kHz give fs_out = 256 kHz. The sweep runs
   * from a 2048-point window (250 Hz) down to the 16-point minimum (60 kHz),
   * so it covers both an unpadded transform and one padded 32x to the
   * 512-point floor; 250, 500, 1000 and 2000 Hz are fs_out / 2^k -- the RBWs
   * the old rule turned into a rectangle. */
  static const double rbws[]
      = { 250.0,  333.0,  500.0,  700.0,   1000.0, 1024.0,
          1500.0, 2000.0, 5000.0, 20000.0, 60000.0 };
  const size_t NR = sizeof rbws / sizeof rbws[0];
  for (size_t r = 0; r < NR; r++)
    {
      dp_specan_state_t *g
          = dp_specan_create (fs, span, rbws[r], 0, 0, 0, 1.0, 0, 1);
      DP_CHECK (g != NULL);
      if (!g)
        continue;
      /* Rule 1: span sets the rate, and its edges are bins. */
      DP_CHECK (g->fs_out == span * 1.28);
      DP_CHECK (g->span == span);
      DP_CHECK (g->disp_n == 2 * (size_t)lround ((double)g->nfft / 2.56) + 1);
      DP_CHECK (g->nfft % 256u == 0); /* k = nfft / 2.56 is an integer */
      DP_CHECK (dp_near ((double)(g->disp_n / 2) * g->fs_out / (double)g->nfft,
                         span / 2.0, 1e-6)); /* edge bin IS span/2 */
      DP_CHECK (g->disp_lo + g->disp_n <= g->nfft);
      /* Rule 2: the window is a power of two >= 16, the SMALLEST whose
       * narrowest RBW (2 bins) fits the request -- half of it would not --
       * and the transform pads it to 512 only when it is shorter. */
      DP_CHECK ((g->n & (g->n - 1)) == 0 && g->n >= 16u);
      DP_CHECK (g->nfft == (g->n < 512u ? 512u : g->n));
      double base = 2.0 * g->fs_out / (double)g->n;
      DP_CHECK (base <= rbws[r] * (1.0 + 1e-12));
      DP_CHECK (base * 2.0 > rbws[r]);
      /* Rule 3: beta widens the ENBW (2..4 bins) to meet the request, so it
       * is never below the beta of 2 bins, and the RBW realised -- read from
       * the window the PSD core actually built -- is within the fit's
       * 0.03%, called 0.1%. */
      DP_CHECK (g->psd->enbw >= 2.0 * (1.0 - 0.0025));
      DP_CHECK (g->psd->enbw <= 4.0 * (1.0 + 0.0025));
      DP_CHECK (g->beta > 11.5);
      double realised = g->psd->enbw * g->fs_out / (double)g->n;
      DP_CHECK (dp_near (g->rbw, realised, realised * 1e-12));
      if (!dp_near (realised, rbws[r], rbws[r] * 0.001))
        printf ("rbw %.0f: realised %.2f (%+.3f%%)\n", rbws[r], realised,
                100.0 * (realised / rbws[r] - 1.0));
      DP_CHECK (dp_near (realised, rbws[r], rbws[r] * 0.001));
      dp_specan_destroy (g);
    }

  /* 2a. Auto and clamps. rbw = 0 is span / 100; a request wider than 4 bins
   * of the 16-point window is clamped to fs_out / 4; span = 0, or one the
   * input cannot supply, is fs / 1.28. */
  {
    dp_specan_state_t *a0
        = dp_specan_create (fs, span, 0.0, 0, 0, 0, 1.0, 0, 1);
    dp_specan_state_t *aw
        = dp_specan_create (fs, span, 100e3, 0, 0, 0, 1.0, 0, 1);
    dp_specan_state_t *s0
        = dp_specan_create (fs, 0.0, 0.0, 0, 0, 0, 1.0, 0, 1);
    dp_specan_state_t *sw
        = dp_specan_create (fs, 0.9 * fs, 0.0, 0, 0, 0, 1.0, 0, 1);
    DP_CHECK (a0 && aw && s0 && sw);
    if (a0 && aw && s0 && sw)
      {
        DP_CHECK (dp_near (a0->rbw, span / 100.0, span / 100.0 * 0.001));
        DP_CHECK (aw->n == 16u);
        DP_CHECK (dp_near (aw->rbw, aw->fs_out / 4.0, aw->fs_out * 2.5e-4));
        DP_CHECK (s0->span == fs / 1.28 && s0->fs_out == fs);
        DP_CHECK (sw->span == fs / 1.28 && sw->fs_out == fs);
        DP_CHECK (dp_near (s0->rbw, s0->span / 100.0, s0->span * 1e-5));
      }
    dp_specan_destroy (a0);
    dp_specan_destroy (aw);
    dp_specan_destroy (s0);
    dp_specan_destroy (sw);
  }

  /* 2b. Every RBW gets the same skirt. Under the old rule an RBW of
   * fs_out/2^k got beta = 0 -- a rectangle -- and painted -13 dB sidelobes
   * round every tone. A noiseless tone must sit >= 80 dB above every bin
   * outside its main lobe. At ENBW 4 bins (beta ~50) the first null is ~16
   * bins of the window out, so 20 window bins -- 20 * nfft / n display bins
   * once padded -- clears it; where that lobe would fill the display (the
   * 20 and 60 kHz windows), there is no skirt left to measure. */
  for (size_t r = 0; r < NR; r++)
    {
      dp_specan_state_t *sk
          = dp_specan_create (fs, span, rbws[r], 0, 0, 0, 1.0, 0, 1);
      DP_CHECK (sk != NULL);
      if (!sk)
        continue;
      size_t guard = 20 * sk->nfft / sk->n;
      if (4 * guard >= sk->disp_n)
        {
          dp_specan_destroy (sk);
          continue;
        }
      gen_tone (tone, NTONE, 30e3 / fs, 1.0);
      size_t nf = drive_first_frame (sk, tone, NTONE, 4096, out, OUTCAP);
      DP_CHECK (nf == sk->disp_n);
      size_t pkk   = argmax (out, nf);
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

  /* 2c. Integrated noise power confirms the ENBW per bin. Complex white noise
   * of unit variance has density N0 = 1/fs, so every display bin must read
   * N0 * rbw = rbw / fs: the RBW measured from the OUTPUT against the RBW
   * REQUESTED, not read back from the window's own enbw field. Averaged over
   * 256 frames of 512 points or more and the central half of the display
   * (clear of the DDC passband edge by more than the widest RBW); the scatter
   * is then a few hundredths of a dB, so 0.1 dB is a bias, not noise. This
   * checks calibration, not the skirt: a rectangle meets its RBW too, which is
   * why 2b exists. */
  {
    float _Complex nz[4096];
    double *acc = malloc (OUTCAP * sizeof *acc);
    DP_CHECK (acc != NULL);
    for (size_t r = 0; acc && r < NR; r++)
      {
        dp_specan_state_t *sn
            = dp_specan_create (fs, span, rbws[r], 0, 0, 0, 1.0, 0, 1);
        DP_CHECK (sn != NULL);
        if (!sn)
          continue;
        uint32_t st     = 0x5eed0u + (uint32_t)r;
        size_t   frames = 0, m = 0;
        memset (acc, 0, OUTCAP * sizeof *acc);
        /* A short window sees few samples per frame: give every RBW the
         * same total noise (256 frames' worth at 512 points). */
        size_t want = 256 * (sn->n < 512u ? 512u / sn->n : 1u);
        while (frames < want)
          {
            for (size_t i = 0; i < 4096; i++)
              nz[i] = dp_cgauss (&st); /* E|z|^2 = 1 */
            size_t no = dp_specan_execute (sn, nz, 4096, out, OUTCAP);
            if (!no)
              continue;
            m = no;
            for (size_t i = 0; i < no; i++)
              acc[i] += pow (10.0, (double)out[i] / 10.0);
            frames++;
          }
        double sum = 0.0;
        size_t lo = m / 4, hi = m - m / 4;
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

  /* The fixture section 3 onward uses. */
  dp_specan_state_t *sa = dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1);
  DP_CHECK (sa != NULL);
  if (!sa)
    return 1;

  /* 3. A unit tone at +30 kHz lands at +30 kHz in the display, near 0 dB. */
  const double f_off = 30e3;
  gen_tone (tone, NTONE, f_off / fs, 1.0);
  size_t nfr = drive_first_frame (sa, tone, NTONE, 4096, out, OUTCAP);
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
  size_t nfr2 = drive_first_frame (sa, tone, NTONE, 4096, out, OUTCAP);
  DP_CHECK (nfr2 == sa->disp_n);
  size_t pk2 = argmax (out, nfr2);
  DP_CHECK (llabs ((long long)pk2 - (long long)dc_bin) <= 2);
  dp_specan_destroy (sa);

  /* 5. navg buffers a full averaging window before emitting a frame. */
  dp_specan_state_t *sb = dp_specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 2);
  DP_CHECK (sb != NULL);
  if (sb)
    {
      /* One window length of input is far short of n*navg decimated. */
      DP_CHECK (dp_specan_execute (sb, tone, sb->n, out, OUTCAP) == 0);
      size_t nfr3 = drive_first_frame (sb, tone, NTONE, 4096, out, OUTCAP);
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
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 2);
    DP_CHECK (g != NULL);
    if (g)
      {
        size_t need = g->n * g->navg, worst = 0;
        for (int k = 0; k < 64; k++)
          {
            (void)dp_specan_execute (g, big, 4096, sout, OUTCAP);
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
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 2);
    dp_specan_state_t *b
        = dp_specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 2);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_specan_execute (a, in, 4096, out, OUTCAP);
    DP_STATE_ROUNDTRIP_TEST (dp_specan, a, b);
    DP_CHECK (b->pend_len == a->pend_len);
    dp_specan_destroy (a);
    dp_specan_destroy (b);
  }

  DP_TEST_END ("test_specan_core");
}
