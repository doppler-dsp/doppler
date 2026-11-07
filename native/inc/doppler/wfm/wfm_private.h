/**
 * @file wfm_private.h
 * @brief Shared internals of the wfm composer, the Plan and the synth.
 *
 * Not part of the public API: nothing here is exported, and nothing here is
 * promised to a caller (the correlator_private.h / correlator_private.h pattern).
 */
#ifndef WFM_PRIVATE_H
#define WFM_PRIVATE_H

#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
/** @brief Keep a function out of line: a loop of its own no caller can fuse. */
#define WFM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
/** @brief Keep a function out of line: a loop of its own no caller can fuse. */
#define WFM_NOINLINE __declspec(noinline)
#else
/** @brief Keep a function out of line: a loop of its own no caller can fuse. */
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

/** @brief Samples a mix block holds; every carrier mix runs in blocks of this
 *  many, the last one padded with zeros. */
#define WFM_MIX_BLOCK 32

/**
 * @brief One block of the carrier mix: `out[i] *= c[i]` for exactly
 *        WFM_MIX_BLOCK complex samples.
 *
 * The one place a complex multiply of the synth's block path is compiled. A
 * vectorised loop has a body and a scalar tail, and a compiler is free to
 * round them differently: a fused product-sum (-ffp-contract), or, on clang
 * for ARMv8.3+ (Apple silicon), FCMLA in the body, which -ffp-contract=off
 * does not stop because -ffast-math marks every operation fusable. So a
 * sample's last bit depended on where the caller's block ended (a Plan builds
 * in one run; a composer pulls 257 at a time), doppler#2805.
 *
 * Every sample now goes through THIS function, and it is out of line with a
 * fixed trip count, so its machine code is one body: a sample is multiplied
 * the same way at any position in any pull, on every compiler.
 *
 * `noinline` stops inlining; it is not `noipa`. A compiler may still analyse
 * the call across the translation unit (constant arguments, cloning), so the
 * guarantee rests on the fixed trip count, which every call site shares.
 */
static WFM_NOINLINE void
wfm_mix_block (float _Complex *restrict out,
               const float _Complex *restrict c)
{
  for (int i = 0; i < WFM_MIX_BLOCK; i++)
    out[i] = out[i] * c[i];
}

/**
 * @brief `out[i] *= carrier[i]` (complex) -- the carrier mix, in whole blocks.
 *
 * The final partial block is run through the same wfm_mix_block() on a
 * zero-padded copy and only the samples that exist are kept, as the FIR does
 * (fir_core.c, #1893).
 *
 * @param out      Signal, mixed in place; length @p n.
 * @param carrier  The carrier, length @p n; must not overlap @p out.
 * @param n        Samples.
 */
static inline void
wfm_mix_carrier (float _Complex *restrict out,
                 const float _Complex *restrict carrier, size_t n)
{
  size_t i = 0;
  for (; i + WFM_MIX_BLOCK <= n; i += WFM_MIX_BLOCK)
    wfm_mix_block (out + i, carrier + i);
  if (i < n)
    {
      float _Complex o[WFM_MIX_BLOCK] = { 0 }, c[WFM_MIX_BLOCK] = { 0 };
      const size_t   r = n - i;
      for (size_t k = 0; k < r; k++)
        o[k] = out[i + k], c[k] = carrier[i + k];
      wfm_mix_block (o, c);
      for (size_t k = 0; k < r; k++)
        out[i + k] = o[k];
    }
}

#endif /* WFM_PRIVATE_H */
