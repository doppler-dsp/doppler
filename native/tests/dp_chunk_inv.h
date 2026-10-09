/**
 * @file dp_chunk_inv.h
 * @brief The chunk-invariance property, once: a streaming object's output is
 *        a function of the INPUT STREAM, not of how it was split into calls.
 *
 * Three tests had each written this by hand --
 * `test_doppler_channel_core.c` (blockwise == one call),
 * `test_wfm_synth_core.c`
 * (#1115, a pinned chirp read in 64-sample blocks) and the framer's own --
 * and every other streaming object had NO test of it at all, because each
 * copy was a page of loop bounds nobody wanted to write a fourth time. The
 * property is the same everywhere:
 *
 *   one call over the whole input  ==  any partition of it into calls
 *
 * so this header takes the three things that differ (how to make an object,
 * how to push `n` elements through it, how big an element is) and supplies
 * the partitions: single samples, a prime, the frame size and its neighbours
 * (the off-by-one sizes), a size that divides nothing, and seeded random
 * splits.
 *
 * ## Shapes it covers
 *
 * - blockwise in -> out (`doppler_channel`, the framer): `in` is the input.
 * - a SOURCE with no input (`wfm_synth`, generators): `in_size == 0`, `in` is
 *   NULL, and `n` counts the elements to produce.
 * - an object that needs side data per call (a profile aligned to the input):
 *   keep it in the object `create` returns -- `process` sees only that.
 *
 * What it does NOT cover: a property that depends on the call boundaries
 * themselves, such as `resamp`'s positions, which are relative to `in[0]` of
 * each CALL. That is a different law (chunking MOVES the number, by exactly
 * the input count), so it stays a bespoke test, and the harness is not bent
 * to fit it.
 *
 * ## The rule that makes this the default
 *
 * `scripts/check_tests_ssot.py` derives which objects owe the test (they carry
 * state and stream) and fails one with neither a test that wires the object
 * into a `dp_ci_spec_t` `.create` / `.process` nor a line on
 * `scripts/.chunk-invariance-ratchet`, which may only shrink.
 *
 * ## Comparison
 *
 * Bit-exact by default: `memcmp` of the concatenated outputs. An object whose
 * contract is a tolerance supplies `equal`; nothing else loosens it.
 */
#ifndef DP_CHUNK_INV_H
#define DP_CHUNK_INV_H

#include "dp_rng_test.h"
#include "dp_test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief What differs between streaming objects. */
typedef struct
{
  const char *name;            /**< Printed with a failure. */
  void *(*create) (void *arg); /**< A FRESH object, same config every call. */
  void (*destroy) (void *obj);
  /**
   * Push @p n input elements through @p obj (or, if @c in_size is 0 and @p in
   * is NULL, produce @p n elements); write at most @p out_cap output elements
   * to @p out; return how many were written.
   */
  size_t (*process) (void *obj, const void *in, size_t n, void *out,
                     size_t out_cap);
  void  *arg;
  size_t in_size;  /**< Bytes per input element; 0 for a source. */
  size_t out_size; /**< Bytes per output element. */
  /** Elements the largest single call may write: size it for the ONE-SHOT. */
  size_t out_cap;
  /** NULL = bit-exact. Else 1 if @p n elements of @p a and @p b agree. */
  int (*equal) (const void *a, const void *b, size_t n);
  /** Frame/block size of the object, or 0: adds sizes N-1, N, N+1, 3N+5. */
  size_t frame_n;
  /**
   * Extra chunk sizes, 0-terminated, or NULL. For an object with an INTERNAL
   * block boundary far larger than the defaults reach (doppler_channel re-cuts
   * at 65536): the partition that straddles it must be asked for by name.
   */
  const size_t *extra_sizes;
  /** Nonzero: do not print a failure. For a negative control that EXPECTS one.
   */
  int quiet;
} dp_ci_spec_t;

/** @brief Run the object over @p in in the given partition; return elements
 * out. */
static size_t
dp_ci_run_ (const dp_ci_spec_t *s, const void *in, size_t n,
            const size_t *sizes, size_t n_sizes, uint32_t *rng, void *out,
            int *ok)
{
  void  *obj      = s->create (s->arg);
  void  *tmp      = malloc (s->out_cap * s->out_size);
  size_t produced = 0, off = 0, k = 0;
  if (!obj || !tmp)
    {
      *ok = 0;
      free (tmp);
      if (obj)
        s->destroy (obj);
      return 0;
    }
  while (off < n)
    {
      size_t m;
      if (n_sizes)
        m = sizes[k++ % n_sizes];
      else
        {
          /* Seeded and shared (dp_rng_test.h): a failure reproduces. */
          m = 1 + dp_xs32 (rng) % (2 * (s->frame_n ? s->frame_n : 64));
        }
      if (m > n - off)
        m = n - off;
      const void *chunk
          = s->in_size ? (const void *)((const char *)in + off * s->in_size)
                       : NULL;
      size_t got = s->process (obj, chunk, m, tmp, s->out_cap);
      if (got > s->out_cap || produced + got > s->out_cap)
        {
          *ok = 0;
          break;
        }
      memcpy ((char *)out + produced * s->out_size, tmp, got * s->out_size);
      produced += got;
      off += m;
    }
  free (tmp);
  s->destroy (obj);
  return produced;
}

/**
 * @brief Check that every partition of @p in (@p n elements) reproduces the
 *        one-shot output.
 *
 * @return Number of partitions that disagreed (0 = invariant). Each failure
 *         prints the object, the partition and the first differing element, so
 *         the sabotage that broke it is readable from the log.
 */
static int
dp_chunk_invariance (const dp_ci_spec_t *s, const void *in, size_t n)
{
  const size_t one = n ? n : 1;
  void        *ref = malloc (s->out_cap * s->out_size);
  void        *got = malloc (s->out_cap * s->out_size);
  int          ok = 1, bad = 0;
  uint32_t     rng = 0x9E3779B9u;
  if (!ref || !got)
    {
      free (ref);
      free (got);
      return 1;
    }
  size_t n_ref = dp_ci_run_ (s, in, n, &one, 1, &rng, ref, &ok);
  if (!ok)
    {
      printf ("FAIL %s: the one-shot run overflowed out_cap\n", s->name);
      free (ref);
      free (got);
      return 1;
    }

  const size_t N = s->frame_n;
  size_t       fixed[48], nf = 0;
  fixed[nf++] = 1;
  fixed[nf++] = 7;
  fixed[nf++] = 64;
  fixed[nf++] = 4093; /* prime: divides nothing */
  if (N)
    {
      if (N > 1)
        fixed[nf++] = N - 1;
      fixed[nf++] = N;
      fixed[nf++] = N + 1;
      fixed[nf++] = 3 * N + 5;
    }
  if (s->extra_sizes)
    for (const size_t *e = s->extra_sizes; *e && nf < 40; e++)
      fixed[nf++] = *e;

  for (size_t p = 0; p < nf + 8; p++)
    {
      int       pok          = 1;
      const int random_split = p >= nf;
      size_t    n_got = dp_ci_run_ (s, in, n, random_split ? NULL : &fixed[p],
                                    random_split ? 0 : 1, &rng, got, &pok);
      int       same  = pok && n_got == n_ref;
      if (same && n_ref)
        same = s->equal ? s->equal (ref, got, n_ref)
                        : memcmp (ref, got, n_ref * s->out_size) == 0;
      if (!same)
        {
          bad++;
          if (s->quiet)
            continue;
          size_t i = 0;
          if (n_got == n_ref)
            while (i < n_ref
                   && memcmp ((char *)ref + i * s->out_size,
                              (char *)got + i * s->out_size, s->out_size)
                          == 0)
              i++;
          printf ("FAIL %s: %s %zu: %zu out vs %zu one-shot, first diff at "
                  "element %zu\n",
                  s->name, random_split ? "random split" : "chunk size",
                  random_split ? p - nf : fixed[p], n_got, n_ref, i);
        }
    }
  free (ref);
  free (got);
  return bad;
}

#endif /* DP_CHUNK_INV_H */
