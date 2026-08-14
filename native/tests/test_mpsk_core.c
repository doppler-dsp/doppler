/*
 * test_mpsk_core.c — the M-PSK constellation: labelling and the decision.
 *
 * `mpsk_slice` is the library's ONE hard-decision rule. It is not a leaf:
 * `mpsk_demap`, `mpsk_diff_demap`, `mpsk_receiver_core.c` and
 * `mpsk_rx_loops.h` all decide through it, so every M-PSK consumer's
 * correctness routes through this file. Before it existed the primitive had
 * zero C assertions — its only coverage was Python, which cannot reach the
 * six inline helpers at all, and a private O(M) copy of the decision rule in
 * test_carrier_mpsk_core.c that scored the carrier loop against itself.
 *
 * Built inside-out, and every section is a property rather than a spot
 * value: Gray labelling, then the map, then the slicer against an EXTERNAL
 * truth, then the array functions on top of those, then differential mode.
 * Each was proven by sabotage — breaking the implementation in the specific
 * way the section describes and watching this file go red.
 *
 * The design is docs/design/mpsk.md §9.
 */
#include "dp_rng_test.h"
#include "dp_test.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

static const int M_ALL[] = { 2, 4, 8 };
enum
{
  NM = (int)(sizeof M_ALL / sizeof M_ALL[0])
};

/* Bit count of a label difference. Small and local: dp_test.h offers no
 * popcount and a table lookup would be more code than the loop. */
static int
bits_set (unsigned v)
{
  int n = 0;
  for (; v; v >>= 1)
    n += (int)(v & 1u);
  return n;
}

/*
 * The EXTERNAL truth for the slicer: nearest constellation point by
 * EUCLIDEAN distance, found by exhaustive search over every index.
 *
 * Deliberately a different computation from the implementation's, which
 * rounds a scaled phase. On the unit circle the two orderings coincide, so
 * this is an independent check and not a consistency test — the failure mode
 * validation.md warns about is a test that compares two paths sharing a
 * defect. It also builds its points from cos/sin directly rather than
 * calling mpsk_constellation, so a broken constellation cannot excuse a
 * broken slicer.
 */
static unsigned
nearest_index_by_distance (float complex y, int m)
{
  double   phi0 = mpsk_phi0 (m);
  double   best = 1e300;
  unsigned bi   = 0;
  for (int k = 0; k < m; k++)
    {
      double th = 2.0 * MPSK_PI * (double)k / (double)m + phi0;
      double dr = (double)crealf (y) - cos (th);
      double di = (double)cimagf (y) - sin (th);
      double d2 = dr * dr + di * di;
      if (d2 < best)
        {
          best = d2;
          bi   = (unsigned)k;
        }
    }
  return bi;
}

int
main (void)
{
  /* ── 1. bits per symbol, both faces ────────────────────────────────────
   *
   * The header promises log2(M) for M in {2,4,8} and *0 for anything else*.
   * The zero is the half nobody writes a test for, and it is the half a
   * caller uses to reject an unsupported M. */
  DP_CHECK (mpsk_bps (2) == 1);
  DP_CHECK (mpsk_bps (4) == 2);
  DP_CHECK (mpsk_bps (8) == 3);
  {
    static const int BAD[] = { -8, -1, 0, 1, 3, 5, 6, 7, 9, 16, 32, 1000 };
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++)
      DP_CHECK (mpsk_bps (BAD[i]) == 0);
    /* The out-of-line wrapper is a second face of one claim, not a second
     * implementation: it must agree everywhere, including on the rejects. */
    for (int m = -8; m <= 32; m++)
      DP_CHECK (mpsk_bits_per_symbol (m) == mpsk_bps (m));
  }

  /* ── 2. the constellation offset ───────────────────────────────────────
   *
   * pi/4 for QPSK and exactly zero otherwise. QPSK's exception is what makes
   * the constellation axis-separable (design §9.2); the zeros are exact, so
   * they are checked exactly rather than with a tolerance. */
  DP_CHECK (mpsk_phi0 (2) == 0.0);
  DP_CHECK (mpsk_phi0 (8) == 0.0);
  DP_CHECK_NEAR (mpsk_phi0 (4), MPSK_PI / 4.0, 1e-15);

  /* ── 3. Gray labelling ─────────────────────────────────────────────────
   *
   * encode is k ^ k>>1; decode inverts it. Checked as a bijection over the
   * full 32-entry range rather than at the three M values, because both are
   * width-independent and a decode that only works below 8 would pass a
   * narrower sweep. */
  for (unsigned k = 0; k < 32u; k++)
    {
      DP_CHECK (mpsk_gray_encode (k) == (k ^ (k >> 1)));
      DP_CHECK (mpsk_gray_decode (mpsk_gray_encode (k)) == k);
    }

  /*
   * The property the whole labelling exists for, and the one that decides
   * whether a symbol error costs one bit or several: adjacent constellation
   * points differ in exactly one bit. CYCLIC — the 0 <-> M-1 seam is where a
   * near-miss labelling breaks, and it is the transition a noisy symbol is
   * most likely to make.
   */
  for (int mi = 0; mi < NM; mi++)
    {
      int m = M_ALL[mi];
      for (int k = 0; k < m; k++)
        {
          unsigned a = mpsk_gray_encode ((unsigned)k);
          unsigned b = mpsk_gray_encode ((unsigned)((k + 1) % m));
          DP_CHECK (bits_set (a ^ b) == 1);
        }
    }

  /* ── 4. the map: geometry, unit amplitude, masking ─────────────────────
   *
   * Exact points per M (design §9.2), against literals rather than against
   * the formula the implementation uses. */
  {
    const float r2 = 0.70710678118654752f;
    DP_CHECK_NEAR (crealf (mpsk_constellation (0, 2)), 1.0, 1e-6);
    DP_CHECK_NEAR (cimagf (mpsk_constellation (0, 2)), 0.0, 1e-6);
    DP_CHECK_NEAR (crealf (mpsk_constellation (1, 2)), -1.0, 1e-6);
    DP_CHECK_NEAR (cimagf (mpsk_constellation (1, 2)), 0.0, 1e-6);
    /* QPSK: every point on a diagonal, i.e. |I| == |Q| == 1/sqrt(2). That is
     * axis-separability stated as a measurement. */
    for (unsigned g = 0; g < 4u; g++)
      {
        float complex p = mpsk_constellation (g, 4);
        DP_CHECK_NEAR (fabs ((double)crealf (p)), (double)r2, 1e-6);
        DP_CHECK_NEAR (fabs ((double)cimagf (p)), (double)r2, 1e-6);
      }
    /* 8PSK label 0 is on the real axis, as BPSK's is. */
    DP_CHECK_NEAR (crealf (mpsk_constellation (0, 8)), 1.0, 1e-6);
    DP_CHECK_NEAR (cimagf (mpsk_constellation (0, 8)), 0.0, 1e-6);
  }

  for (int mi = 0; mi < NM; mi++)
    {
      int m = M_ALL[mi];
      for (unsigned g = 0; g < (unsigned)m; g++)
        {
          float complex p = mpsk_constellation (g, m);
          /* Unit amplitude — the premise every downstream claim rests on:
           * ahat is a valid carrier error only because |ahat| == 1. */
          DP_CHECK_NEAR (cabsf (p), 1.0, 1e-6);
          /* The label is Gray: the point sits at index gray_decode(g), not
           * at g. Measured as an angle against an independently computed
           * one, so a map that skipped the decode fails here. */
          double want
              = 2.0 * MPSK_PI * (double)mpsk_gray_decode (g) / (double)m
                + mpsk_phi0 (m);
          double got = atan2 ((double)cimagf (p), (double)crealf (p));
          double d   = fmod (got - want + 4.0 * MPSK_PI, 2.0 * MPSK_PI);
          if (d > MPSK_PI)
            d -= 2.0 * MPSK_PI;
          DP_CHECK_NEAR (d, 0.0, 1e-6);
          /* The header says g is masked to the low log2(M) bits, so a label
           * carrying high junk maps to the same point. Nothing in the
           * library relies on this today; it is a documented promise, and an
           * unexercised promise is how an assert-free mask gets deleted. */
          DP_CHECK (mpsk_constellation (g + (unsigned)m, m) == p);
          DP_CHECK (mpsk_constellation (g + 8u * (unsigned)m, m) == p);
        }
    }

  /* ── 5. the slicer ─────────────────────────────────────────────────────
   *
   * (a) Exact at the constellation points: slice o constellation == identity
   * on labels, and the returned ahat is the point itself. This is the
   * "mutual inverse" claim both headers make about each other. */
  for (int mi = 0; mi < NM; mi++)
    {
      int m = M_ALL[mi];
      for (unsigned g = 0; g < (unsigned)m; g++)
        {
          float complex ahat;
          float complex p = mpsk_constellation (g, m);
          DP_CHECK (mpsk_slice (p, m, &ahat) == g);
          DP_CHECK_NEAR (cabsf (ahat - p), 0.0, 1e-6);
        }
    }

  /*
   * (b) Nearest-in-phase, swept, against the external Euclidean truth. 512
   * angles per M walks every decision region and both sides of every
   * boundary. Ties are excluded rather than asserted: at an exact midpoint
   * either neighbour is correct, so the sweep steps off the boundary by a
   * half-step and the tie case is checked separately below.
   */
  for (int mi = 0; mi < NM; mi++)
    {
      int m = M_ALL[mi];
      for (int i = 0; i < 512; i++)
        {
          double        th = 2.0 * MPSK_PI * ((double)i + 0.25) / 512.0;
          float complex y  = (float)cos (th) + (float)sin (th) * I;
          float complex ahat;
          unsigned      g  = mpsk_slice (y, m, &ahat);
          unsigned      ki = mpsk_gray_decode (g);
          DP_CHECK (ki == nearest_index_by_distance (y, m));
          /* ahat is that nearest point, at unit amplitude. */
          DP_CHECK_NEAR (cabsf (ahat), 1.0, 1e-6);
          DP_CHECK_NEAR (cabsf (ahat - mpsk_constellation (g, m)), 0.0, 1e-6);
        }
    }

  /*
   * (c) At an exact decision boundary the answer must still be ONE OF the
   * two neighbours — never a third index, and never out of range. Which one
   * is a rounding detail the header does not promise, so it is not pinned.
   */
  for (int mi = 0; mi < NM; mi++)
    {
      int m = M_ALL[mi];
      for (int k = 0; k < m; k++)
        {
          double th
              = mpsk_phi0 (m) + 2.0 * MPSK_PI * ((double)k + 0.5) / (double)m;
          float complex y = (float)cos (th) + (float)sin (th) * I;
          float complex ahat;
          unsigned      ki = mpsk_gray_decode (mpsk_slice (y, m, &ahat));
          DP_CHECK (ki == (unsigned)k || ki == (unsigned)((k + 1) % m));
        }
    }

  /*
   * (d) Amplitude invariance: "any amplitude; only the phase is used". Swept
   * over nine decades, including a magnitude small enough that a decision
   * made on raw I/Q thresholds rather than on phase would collapse.
   */
  {
    static const float SCALE[]
        = { 1e-6f, 1e-3f, 0.1f, 0.5f, 1.0f, 2.0f, 100.0f, 1e4f, 1e6f };
    uint32_t rs = 12345u;
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        for (int t = 0; t < 64; t++)
          {
            double        th = dp_uni (&rs) * 2.0 * MPSK_PI;
            float complex u  = (float)cos (th) + (float)sin (th) * I;
            float complex a0;
            unsigned      g0 = mpsk_slice (u, m, &a0);
            for (size_t s = 0; s < sizeof SCALE / sizeof SCALE[0]; s++)
              {
                float complex ahat;
                DP_CHECK (mpsk_slice (SCALE[s] * u, m, &ahat) == g0);
                /* The decision does not inherit the input's scale. */
                DP_CHECK_NEAR (cabsf (ahat), 1.0, 1e-6);
              }
          }
      }
  }

  /*
   * (e) ahat as a carrier error signal (design §9.4). For a symbol sitting
   * exactly on a point the decision-directed error Im(y * conj(ahat)) is
   * zero; rotate the symbol by a small angle inside the decision region and
   * the error takes that angle's sign and grows with it. This is the
   * property mpsk_rx_loops.h actually depends on, and nothing else asserts
   * it.
   */
  for (int mi = 0; mi < NM; mi++)
    {
      int    m   = M_ALL[mi];
      double lim = MPSK_PI / (double)m; /* half a decision region */
      for (unsigned g = 0; g < (unsigned)m; g++)
        {
          float complex p = mpsk_constellation (g, m);
          float complex ahat;
          mpsk_slice (p, m, &ahat);
          DP_CHECK_NEAR (cimagf (p * conjf (ahat)), 0.0, 1e-6);

          double prev = -1e300;
          for (int e = 1; e <= 4; e++)
            {
              double        eps = 0.5 * lim * (double)e / 4.0;
              float complex rot = (float)cos (eps) + (float)sin (eps) * I;
              float complex yp  = p * rot;
              float complex ap;
              mpsk_slice (yp, m, &ap);
              double err = (double)cimagf (yp * conjf (ap));
              DP_CHECK (err > 0.0);  /* sign follows the rotation      */
              DP_CHECK (err > prev); /* and grows monotonically with it */
              prev = err;
              /* The mirror rotation gives the opposite sign. */
              float complex rn = (float)cos (-eps) + (float)sin (-eps) * I;
              float complex yn = p * rn;
              float complex an;
              mpsk_slice (yn, m, &an);
              DP_CHECK ((double)cimagf (yn * conjf (an)) < 0.0);
            }
        }
    }

  /* (f) The header's own C example, which nothing executed before now. */
  {
    float complex ahat;
    DP_CHECK (mpsk_slice ((1.0f + 1.0f * I) * 0.70710678f, 4, &ahat) == 0);
  }

  /* ── 6. map/demap over an array ────────────────────────────────────────
   *
   * Round-trip across every label, then the two structural claims the array
   * functions add over the per-symbol ones: MEMORYLESS, and no write past
   * the caller's buffer.
   */
  {
    uint8_t       sym[8], back[8];
    float complex pts[8];
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        for (int g = 0; g < m; g++)
          sym[g] = (uint8_t)g;
        mpsk_map (sym, (size_t)m, pts, m);
        mpsk_demap (pts, (size_t)m, back, m);
        for (int g = 0; g < m; g++)
          DP_CHECK (back[g] == sym[g]);
      }
  }

  /*
   * Memorylessness, stated as a property a stateful implementation fails:
   * reversing the input reverses the output exactly. An accumulator hidden
   * in the loop (the differential functions' shape) breaks this immediately,
   * which is what makes it the right dual to section 7's sequential check.
   */
  {
    enum
    {
      N = 64
    };
    uint8_t       fwd[N], rev[N];
    float complex pf[N], pr[N];
    uint32_t      rs = 777u;
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        for (int i = 0; i < N; i++)
          fwd[i] = (uint8_t)(dp_xs32 (&rs) % (uint32_t)m);
        for (int i = 0; i < N; i++)
          rev[i] = fwd[N - 1 - i];
        mpsk_map (fwd, N, pf, m);
        mpsk_map (rev, N, pr, m);
        for (int i = 0; i < N; i++)
          DP_CHECK (pf[i] == pr[N - 1 - i]);

        /* demap is memoryless too, and amplitude-invariant elementwise. */
        uint8_t       df[N], dr[N];
        float complex sf[N];
        for (int i = 0; i < N; i++)
          sf[i] = pf[i] * (float)(0.01 + 100.0 * dp_uni (&rs));
        mpsk_demap (sf, N, df, m);
        mpsk_demap (pf, N, dr, m);
        for (int i = 0; i < N; i++)
          {
            DP_CHECK (df[i] == fwd[i]);
            DP_CHECK (dr[i] == fwd[i]);
            /* Only the low log2(M) bits are ever set. */
            DP_CHECK (df[i] < (uint8_t)m);
          }
      }
  }

  /*
   * "out must hold sym_len points" — a length claim, so it is checked by
   * canary rather than believed. A one-past-the-end write is invisible to
   * every other assertion in this file.
   */
  {
    enum
    {
      N = 16
    };
    uint8_t       sym[N];
    float complex pts[N + 2];
    uint8_t       out[N + 2];
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        for (int i = 0; i < N; i++)
          sym[i] = (uint8_t)(i % m);
        pts[N] = pts[N + 1] = 12345.0f + 6789.0f * I;
        mpsk_map (sym, N, pts, m);
        DP_CHECK (pts[N] == (float complex) (12345.0f + 6789.0f * I));
        DP_CHECK (pts[N + 1] == (float complex) (12345.0f + 6789.0f * I));

        out[N] = out[N + 1] = 0xABu;
        mpsk_demap (pts, N, out, m);
        DP_CHECK (out[N] == 0xABu && out[N + 1] == 0xABu);

        /* Zero length must touch nothing at all. */
        out[0] = 0xCDu;
        mpsk_demap (pts, 0, out, m);
        DP_CHECK (out[0] == 0xCDu);
      }
  }

  /* ── 7. differential mode ──────────────────────────────────────────────
   *
   * (a) Exact round-trip, and (b) the accumulator claim: the running index
   * is the running SUM of the decoded labels, mod M, from an implicit zero
   * start. The expected index is accumulated here independently of the
   * implementation's loop.
   */
  {
    enum
    {
      N = 300
    };
    uint8_t       sym[N], back[N];
    float complex pts[N];
    uint32_t      rs = 4242u;
    for (int mi = 0; mi < NM; mi++)
      {
        int      m    = M_ALL[mi];
        unsigned mask = (unsigned)(m - 1);
        for (int i = 0; i < N; i++)
          sym[i] = (uint8_t)(dp_xs32 (&rs) % (uint32_t)m);

        mpsk_diff_map (sym, N, pts, m);
        mpsk_diff_demap (pts, N, back, m);
        for (int i = 0; i < N; i++)
          DP_CHECK (back[i] == sym[i]);

        unsigned acc = 0;
        for (int i = 0; i < N; i++)
          {
            acc = (acc + mpsk_gray_decode (sym[i])) & mask;
            float complex want
                = mpsk_constellation (mpsk_gray_encode (acc), m);
            DP_CHECK_NEAR (cabsf (pts[i] - want), 0.0, 1e-6);
          }
        /* The first symbol references the implicit zero-phase start, so it
         * is the coherent point for its own label — the one place the two
         * maps agree. */
        DP_CHECK_NEAR (cabsf (pts[0] - mpsk_constellation (sym[0], m)), 0.0,
                       1e-6);
      }
  }

  /*
   * (c) Invariance to an unknown CONSTANT carrier phase — the reason
   * differential mode exists. Stronger than the M-fold ambiguity: any
   * constant offset shifts every sliced index equally and cancels in the
   * difference, so arbitrary angles are swept alongside the M discrete
   * rotations. Symbol 0 is excluded because it references the implicit zero
   * start, which the rotation genuinely moves (design §9.5).
   */
  {
    enum
    {
      N = 200
    };
    uint8_t       sym[N], back[N];
    float complex pts[N], rot[N];
    uint32_t      rs = 99u;
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        for (int i = 0; i < N; i++)
          sym[i] = (uint8_t)(dp_xs32 (&rs) % (uint32_t)m);
        mpsk_diff_map (sym, N, pts, m);

        for (int j = 0; j < m + 5; j++)
          {
            /* the m constellation rotations, then five arbitrary angles */
            double phi      = (j < m) ? (2.0 * MPSK_PI * (double)j / (double)m)
                                      : (0.137 + 0.911 * (double)(j - m));
            float complex r = (float)cos (phi) + (float)sin (phi) * I;
            for (int i = 0; i < N; i++)
              rot[i] = pts[i] * r;
            mpsk_diff_demap (rot, N, back, m);
            for (int i = 1; i < N; i++)
              DP_CHECK (back[i] == sym[i]);
          }
      }
  }

  /*
   * (d) The dual of section 6's memorylessness check: differential mode is
   * SEQUENTIAL, so reversing the input must NOT simply reverse the output.
   * Asserted as a disagreement, with the precondition that the labels really
   * do vary — a constant sequence would satisfy it vacuously, which is the
   * reject-test trap validation.md names.
   */
  {
    enum
    {
      N = 32
    };
    uint8_t       fwd[N], rev[N];
    float complex pf[N], pr[N];
    uint32_t      rs = 31337u;
    for (int mi = 0; mi < NM; mi++)
      {
        int m = M_ALL[mi];
        if (m == 2)
          continue; /* BPSK: too few phases for the check to mean much */
        int varied = 0;
        for (int i = 0; i < N; i++)
          fwd[i] = (uint8_t)(dp_xs32 (&rs) % (uint32_t)m);
        for (int i = 1; i < N; i++)
          if (fwd[i] != fwd[0])
            varied = 1;
        DP_CHECK (varied); /* precondition, not decoration */
        for (int i = 0; i < N; i++)
          rev[i] = fwd[N - 1 - i];
        mpsk_diff_map (fwd, N, pf, m);
        mpsk_diff_map (rev, N, pr, m);
        int differs = 0;
        for (int i = 0; i < N; i++)
          if (cabsf (pf[i] - pr[N - 1 - i]) > 1e-6f)
            differs = 1;
        DP_CHECK (differs);
      }
  }

  DP_TEST_END ("test_mpsk_core");
}
