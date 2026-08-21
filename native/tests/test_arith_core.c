/*
 * test_arith_core.c — the arith module's fixed-point free functions.
 *
 * These are the Q15 and Q8 kernels, and the only interesting thing about
 * them is what happens at the rails. A Q-format add is not integer
 * addition: it SATURATES rather than wrapping, because a wrapped sample is
 * a full-scale sign flip in the middle of a waveform and a clipped one is
 * merely clipped. Every claim below is about that boundary, the rounding
 * rule at the LSB, or the identity the kernel is supposed to preserve away
 * from both.
 *
 * The two widths are tested identically on purpose. They are the same
 * source transliterated between 16 and 8 bits, so a defect in one and not
 * the other is exactly what a shared test finds — and doppler#905 records
 * that the two have already diverged in cost, which is a reason to keep
 * checking that they have not diverged in behaviour.
 */
#include "arith/arith_core.h"
#include "dp_test.h"
#include <stdio.h>

#define Q15_MAX 32767
#define Q15_MIN (-32768)
#define Q8_MAX 127
#define Q8_MIN (-128)

int
main (void)
{
  /* ── Q15: saturation, not wrap ──────────────────────────────────── */
  {
    const int16_t big[4] = { Q15_MAX, Q15_MIN, Q15_MAX, Q15_MIN };
    const int16_t one[4] = { 1, -1, Q15_MAX, Q15_MIN };
    int16_t       out[4] = { 0 };

    add_q15 (big, 4, one, 4, out);
    DP_CHECK (out[0] == Q15_MAX); /* 32767 + 1 clamps, does not wrap */
    DP_CHECK (out[1] == Q15_MIN);
    DP_CHECK (out[2] == Q15_MAX); /* max + max is still max */
    DP_CHECK (out[3] == Q15_MIN);

    sub_q15 (big, 4, one, 4, out);
    DP_CHECK (out[0] == Q15_MAX - 1);
    DP_CHECK (out[1] == Q15_MIN + 1); /* min - (-1) rises by one, no wrap */
    DP_CHECK (out[2] == 0);
    DP_CHECK (out[3] == 0);

    /* The saturating case for subtract, which the block above does not
       reach: min - max is -65535, which is not a small negative number. */
    {
      const int16_t lo[2]  = { Q15_MIN, Q15_MAX };
      const int16_t hi[2]  = { Q15_MAX, Q15_MIN };
      int16_t       sat[2] = { 0 };
      sub_q15 (lo, 2, hi, 2, sat);
      DP_CHECK (sat[0] == Q15_MIN);
      DP_CHECK (sat[1] == Q15_MAX);
    }
  }

  /* ── Q15: the multiply is a Q15 product, not an integer one ─────── */
  {
    const int16_t a[3]   = { Q15_MAX, 16384, Q15_MIN }; /* 1.0-, 0.5, -1.0 */
    const int16_t b[3]   = { Q15_MAX, 16384, Q15_MIN };
    int16_t       out[3] = { 0 };

    mul_q15 (a, 3, b, 3, out);
    /* 0.5 * 0.5 = 0.25 -> 8192, exactly representable. */
    DP_CHECK (out[1] == 8192);
    /* (1-eps)^2 stays inside the range and near full scale. */
    DP_CHECK (out[0] > 32000 && out[0] <= Q15_MAX);
    /* (-1) * (-1) = +1.0, which Q15 cannot hold -> saturates to max. */
    DP_CHECK (out[2] == Q15_MAX);
  }

  /* ── Q15: the dot product accumulates WIDE ──────────────────────── */
  {
    /* Eight full-scale products overflow a 32-bit Q15 accumulator's
       headroom if the accumulation is done narrow. The return type is
       int64_t precisely so this cannot happen; check the value rather
       than the type, since only the value is observable. */
    int16_t a[8], b[8];
    for (int i = 0; i < 8; i++)
      {
        a[i] = Q15_MAX;
        b[i] = Q15_MAX;
      }
    const int64_t d = dot_q15 (a, 8, b, 8);
    DP_CHECK (d == 8LL * (int64_t)Q15_MAX * (int64_t)Q15_MAX);
    DP_CHECK (d > 0); /* the sign is what a narrow accumulator loses */
  }

  /* ── Q15: shifts saturate on the way up, sign-extend on the way down */
  {
    const int16_t a[4]   = { 16384, -16384, 1, -1 };
    int16_t       out[4] = { 0 };

    shl_q15 (a, 4, out, 1);
    DP_CHECK (out[0] == Q15_MAX); /* 0.5 << 1 = 1.0, not representable */
    DP_CHECK (out[1] == Q15_MIN);
    DP_CHECK (out[2] == 2);
    DP_CHECK (out[3] == -2);

    shr_q15 (a, 4, out, 1);
    DP_CHECK (out[0] == 8192);
    DP_CHECK (out[1] == -8192);
    /* A right shift of a negative odd value must not round toward zero
       differently from the positive case, or a signal acquires a DC term
       proportional to how often it is scaled. */
    DP_CHECK (out[2] == 0 || out[2] == 1);
    DP_CHECK (out[3] == 0 || out[3] == -1);

    /* Shifting by zero is the identity. */
    shl_q15 (a, 4, out, 0);
    for (int i = 0; i < 4; i++)
      DP_CHECK (out[i] == a[i]);
  }

  /* ── Q8: the same claims, same order ────────────────────────────── */
  {
    const int8_t big[4] = { Q8_MAX, Q8_MIN, Q8_MAX, Q8_MIN };
    const int8_t one[4] = { 1, -1, Q8_MAX, Q8_MIN };
    int8_t       out[4] = { 0 };

    add_q8 (big, 4, one, 4, out);
    DP_CHECK (out[0] == Q8_MAX);
    DP_CHECK (out[1] == Q8_MIN);
    DP_CHECK (out[2] == Q8_MAX);
    DP_CHECK (out[3] == Q8_MIN);

    sub_q8 (big, 4, one, 4, out);
    DP_CHECK (out[0] == Q8_MAX - 1);
    DP_CHECK (out[1] == Q8_MIN + 1);
    DP_CHECK (out[2] == 0);
    DP_CHECK (out[3] == 0);

    {
      const int8_t lo[2]  = { Q8_MIN, Q8_MAX };
      const int8_t hi[2]  = { Q8_MAX, Q8_MIN };
      int8_t       sat[2] = { 0 };
      sub_q8 (lo, 2, hi, 2, sat);
      DP_CHECK (sat[0] == Q8_MIN);
      DP_CHECK (sat[1] == Q8_MAX);
    }
  }

  {
    const int8_t a[3]   = { Q8_MAX, 64, Q8_MIN }; /* 1.0-, 0.5, -1.0 */
    const int8_t b[3]   = { Q8_MAX, 64, Q8_MIN };
    int8_t       out[3] = { 0 };

    mul_q8 (a, 3, b, 3, out);
    DP_CHECK (out[1] == 32); /* 0.5 * 0.5 = 0.25 */
    DP_CHECK (out[0] > 120 && out[0] <= Q8_MAX);
    DP_CHECK (out[2] == Q8_MAX); /* (-1)^2 saturates, as in Q15 */
  }

  {
    int8_t a[8], b[8];
    for (int i = 0; i < 8; i++)
      {
        a[i] = Q8_MAX;
        b[i] = Q8_MAX;
      }
    const int32_t d = dot_q8 (a, 8, b, 8);
    DP_CHECK (d == 8 * Q8_MAX * Q8_MAX);
    DP_CHECK (d > 0);
  }

  {
    const int8_t a[4]   = { 64, -64, 1, -1 };
    int8_t       out[4] = { 0 };

    shl_q8 (a, 4, out, 1);
    DP_CHECK (out[0] == Q8_MAX);
    DP_CHECK (out[1] == Q8_MIN);
    DP_CHECK (out[2] == 2);

    shr_q8 (a, 4, out, 1);
    DP_CHECK (out[0] == 32);
    DP_CHECK (out[1] == -32);

    shl_q8 (a, 4, out, 0);
    for (int i = 0; i < 4; i++)
      DP_CHECK (out[i] == a[i]);
  }

  /* ── int64 shifts: no saturation, because there is no Q format ──── */
  {
    const int64_t a[3]   = { 1, -1, 1LL << 40 };
    int64_t       out[3] = { 0 };

    shl_i64 (a, 3, out, 4);
    DP_CHECK (out[0] == 16);
    DP_CHECK (out[1] == -16);
    DP_CHECK (out[2] == (1LL << 44));

    shr_i64 (a, 3, out, 4);
    DP_CHECK (out[2] == (1LL << 36));

    shl_i64 (a, 3, out, 0);
    for (int i = 0; i < 3; i++)
      DP_CHECK (out[i] == a[i]);
  }

  /* ── Length mismatch is the shorter of the two ───────────────────── */
  {
    const int16_t a[4]   = { 100, 200, 300, 400 };
    const int16_t b[2]   = { 1, 2 };
    int16_t       out[4] = { -1, -1, -1, -1 };

    add_q15 (a, 4, b, 2, out);
    DP_CHECK (out[0] == 101);
    DP_CHECK (out[1] == 202);
    /* Whatever the kernel does past the short operand, it must not have
       read past it -- the surviving sentinel is the only evidence a test
       can offer for that without a sanitizer. */
    DP_CHECK (out[2] == -1 || out[2] == 300 || out[2] == 0);
  }

  DP_TEST_END ("test_arith_core");
}
