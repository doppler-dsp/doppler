/**
 * @file wfm_private.h
 * @brief Shared internals of the wfm composer, the Plan and the synth.
 *
 * Not part of the public API: nothing here is exported, and nothing here is
 * promised to a caller (the acq_private.h / det_private.h pattern).
 */
#ifndef WFM_PRIVATE_H
#define WFM_PRIVATE_H

#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define WFM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define WFM_NOINLINE __declspec(noinline)
#else
#define WFM_NOINLINE
#endif

/**
 * @brief `out[i] += gain * in[i]` -- the ONE real-gain accumulate every wfm
 *        path uses to add a scaled stream (a noise stream, a source) into a
 *        sum. (A phase-rotated source, whose gain is complex, is the Plan's
 *        own override and has no compose twin to agree with.)
 *
 * A compose and a Plan must produce the same bits for the same scene. Each
 * adds the same streams, but written inline as `sig * carrier + noise` in one
 * place and `cache + noise` in another they are the same number only where
 * the compiler never fuses a multiply-add; under this tree's `-ffast-math` an
 * FMA target (AArch64, or x86 with `-mfma`) rounds the first once and the
 * second twice (doppler#2789). So the signal is STORED (rounded) first and
 * every path adds through this function.
 *
 * `noinline` keeps it a loop of its own that no caller can fuse with the pass
 * before it. It is a `static` in each translation unit that includes this
 * header (nothing is exported), compiled from this one source under the one
 * set of flags, so every copy runs the same instructions.
 *
 * @param out   Sum, updated in place; length @p n.
 * @param in    The stream to add, length @p n; must not overlap @p out.
 * @param n     Samples.
 * @param gain  Multiplier on @p in (1.0f for a stream already at its
 *              amplitude).
 */
static WFM_NOINLINE void
wfm_add_scaled (float _Complex *restrict out, const float _Complex *restrict in,
                size_t n, float gain)
{
  for (size_t i = 0; i < n; i++)
    out[i] += gain * in[i];
}

#endif /* WFM_PRIVATE_H */
