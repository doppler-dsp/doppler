/**
 * @file awgn_core.c
 * @brief AWGN generator: xoshiro256++ RNG + Box-Muller transform.
 *
 * ### Algorithm
 *
 * Two paths share the same logical algorithm but differ in width:
 *
 *   For each output sample, consume two 64-bit RNG words:
 *     u1  = (top-24-bits(word0) + 1) × 2^-24   ∈ (0, 1]   (log-safe)
 *     idx = top-16-bits(word1)                  ∈ [0, 65535]
 *     r   = amplitude × sqrt(−2 × ln u1)        (Box-Muller radial)
 *     out = r × cos(idx) + j × r × sin(idx)     (96 dBc LUT phase)
 *
 * ### Scalar path
 *
 * One xoshiro256++ state (4 × uint64).  Two next() calls per sample.
 *
 * ### One path, on purpose (gh-690)
 *
 * There was a second, AVX-512 implementation running EIGHT independent
 * xoshiro256++ streams out of `vs[0..3][0..7]`, selected at run time. It
 * produced a DIFFERENT noise sequence from the same seed — the scalar state
 * `s[0..3]` and the eight `vs` streams are seeded from different SplitMix64
 * draws, so they are unrelated sequences. Same seed, sample [1]:
 * `-0.054472364 -0.171774969` scalar against `-0.345792860 +0.103448674`
 * vectorised.
 *
 * That made every AWGN-derived number — BER curves, EVM, validation output —
 * silently unreproducible across platforms, and nothing noticed, because the
 * only reproducibility test compared a stream to itself after `awgn_reset`
 * (same path, same machine) and passes identically under either.
 *
 * Two measurements decided the removal rather than a rewrite:
 *
 *   - On Linux the fast path was DEAD. Its guard requires the libmvec symbol
 *     `_ZGVdN8v_logf`, declared weak, and no doppler artifact links libmvec —
 *     so it never executed in a shipped Linux build.
 *   - On macOS/Windows x86-64 it was LIVE, because the non-Linux branch
 *     defined that symbol as a scalar-loop shim, so the NULL check could not
 *     fail. Those platforms paid the divergence while getting no vectorised
 *     logarithm at all.
 *
 * Which is to say the cost was paid exactly where the benefit was absent. The
 * benefit was real where it could be had (~1.6x, 446 against 280 Msamp/s with
 * libmvec linked), so this is worth restoring — with a parity gate and an
 * explicit `-lmvec`, and consuming THIS stream rather than its own.
 *
 * ### The eight streams were never independent
 *
 * Worth knowing before anyone rebuilds it. The per-stream seed offset was
 * `seed + j * 0x9e3779b97f4a7c15`, and that constant is exactly what
 * SplitMix64 adds to its own state on every draw. So advancing the STREAM
 * index by one is the same operation as advancing the splitmix chain by one
 * DRAW, and `vs[w][j] == vs[w-1][j+1]` — every stream is its neighbour
 * shifted by a word.
 *
 * Measured at seed 42: that identity holds for all 21 checkable pairs, the
 * 32 state slots contain only **11 distinct 64-bit words**, and stream 0 is
 * bit-for-bit the scalar stream (which is why sample [0] agreed between the
 * two paths while everything after it diverged).
 *
 * Eight lanes of a generator whose starting states overlap in three of their
 * four words are not eight independent streams. Whatever replaces this needs
 * genuinely separated seeds — SplitMix64 run forward per stream, or
 * xoshiro256++'s own `jump()`.
 *
 * `vs[4][8]` is still seeded and serialized. Dropping it would change
 * `awgn_state_bytes`, which `wfm_synth_state_bytes` includes, churning the
 * state protocol of two objects to save 256 bytes. `test_awgn_core.c` pins
 * the sequence, so a future vectorisation has to reproduce it.
 *
 * ### Sin/cos LUT
 *
 * 2^16 single-precision entries, identical layout to lo_core:
 *   lut[i]         = sin(2π·i / 65536)
 *   lut[i + LUT_QTR] = cos(2π·i / 65536)   (uint16 wrap, no branch)
 *
 * SFDR ≈ 96 dBc — sufficient for noise generation at any SNR of
 * practical interest.
 *
 * ### References
 *
 * xoshiro256++: Blackman & Vigna, ACM TOMS 47(4), 2021.
 * SplitMix64:   Steele et al., PLDI 2014.
 */
#include "awgn/awgn_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Sin/cos LUT — 2^16 entries, same layout as lo_core.               */
/* ------------------------------------------------------------------ */

#define LUT_BITS 16u
#define LUT_SIZE (1u << LUT_BITS)
#define LUT_QTR (LUT_SIZE >> 2u)

static float lut[LUT_SIZE];
static int   lut_ready = 0;

static void
lut_init (void)
{
  if (lut_ready)
    return;
  for (unsigned i = 0; i < LUT_SIZE; i++)
    lut[i] = sinf (2.0f * (float)M_PI * (float)i / (float)LUT_SIZE);
  lut_ready = 1;
}

/* ------------------------------------------------------------------ */
/* xoshiro256++ — scalar                                               */
/* ------------------------------------------------------------------ */

static inline uint64_t
rotl64 (uint64_t x, int k)
{
  return (x << k) | (x >> (64 - k));
}

static inline uint64_t
xoshiro_next (uint64_t s[4])
{
  uint64_t r = rotl64 (s[0] + s[3], 23) + s[0];
  uint64_t t = s[1] << 17;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = rotl64 (s[3], 45);
  return r;
}

/* SplitMix64: expands one 64-bit seed into 4 independent state words. */
static uint64_t
splitmix64 (uint64_t *x)
{
  uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
  z          = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z          = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static void
seed_state (uint64_t s[4], uint64_t seed)
{
  uint64_t sm = seed;
  s[0]        = splitmix64 (&sm);
  s[1]        = splitmix64 (&sm);
  s[2]        = splitmix64 (&sm);
  s[3]        = splitmix64 (&sm);
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

awgn_state_t *
awgn_create (uint64_t seed, float amplitude)
{
  lut_init ();
  awgn_state_t *s = malloc (sizeof *s);
  if (!s)
    return NULL;
  s->seed      = seed;
  s->amplitude = amplitude;
  seed_state (s->s, seed);
  /* Initialise 8 independent AVX streams via offset seeds.
   * vs is transposed: vs[word][stream], so words for stream j are at
   * vs[0][j], vs[1][j], vs[2][j], vs[3][j] — not contiguous. */
  for (int j = 0; j < 8; j++)
    {
      uint64_t stream_seed = seed + (uint64_t)j * 0x9e3779b97f4a7c15ULL;
      uint64_t sm          = stream_seed;
      s->vs[0][j]          = splitmix64 (&sm);
      s->vs[1][j]          = splitmix64 (&sm);
      s->vs[2][j]          = splitmix64 (&sm);
      s->vs[3][j]          = splitmix64 (&sm);
    }
  return s;
}

void
awgn_destroy (awgn_state_t *state)
{
  free (state);
}

void
awgn_reset (awgn_state_t *state)
{
  seed_state (state->s, state->seed);
  for (int j = 0; j < 8; j++)
    {
      uint64_t stream_seed = state->seed + (uint64_t)j * 0x9e3779b97f4a7c15ULL;
      uint64_t sm          = stream_seed;
      state->vs[0][j]      = splitmix64 (&sm);
      state->vs[1][j]      = splitmix64 (&sm);
      state->vs[2][j]      = splitmix64 (&sm);
      state->vs[3][j]      = splitmix64 (&sm);
    }
}

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * The running RNG state only: scalar s[4] + the 8 AVX2 stream words vs[4][8].
 * seed / amplitude are config restored by create(). */

size_t
awgn_state_bytes (const awgn_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t) * 4
         + sizeof (uint64_t) * 4 * 8;
}

void
awgn_get_state (const awgn_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, awgn_state_bytes (state));
  dp_w_hdr (&w, AWGN_STATE_MAGIC, AWGN_STATE_VERSION,
            awgn_state_bytes (state));
  dp_w_bytes (&w, state->s, sizeof state->s);
  dp_w_bytes (&w, state->vs, sizeof state->vs);
}

int
awgn_set_state (awgn_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, awgn_state_bytes (state), AWGN_STATE_MAGIC,
                              AWGN_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, awgn_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  dp_r_bytes (&r, state->s, sizeof state->s);
  dp_r_bytes (&r, state->vs, sizeof state->vs);
  return DP_OK;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

float
awgn_get_amplitude (const awgn_state_t *state)
{
  return state->amplitude;
}

void
awgn_set_amplitude (awgn_state_t *state, float val)
{
  state->amplitude = val;
}

void
awgn_reseed (awgn_state_t *state, uint64_t seed)
{
  state->seed = seed;
  awgn_reset (state);
}

size_t
awgn_generate_max_out (awgn_state_t *state)
{
  (void)state;
  return 65536;
}

/* ================================================================== */
/* Generate — scalar path                                              */
/* ================================================================== */

static void
generate_scalar (awgn_state_t *state, size_t n, float complex *out)
{
  const float amp = state->amplitude;
  uint64_t   *s   = state->s;

  for (size_t i = 0; i < n; i++)
    {
      uint64_t a   = xoshiro_next (s);
      uint64_t b   = xoshiro_next (s);
      float    u1  = (float)((uint32_t)(a >> 40) + 1u) * 0x1.0p-24f;
      uint16_t idx = (uint16_t)(b >> 48);
      float    r   = amp * sqrtf (-2.0f * logf (u1));
      out[i]       = CMPLXF (r * lut[(uint16_t)(idx + (uint16_t)LUT_QTR)],
                             r * lut[idx]);
    }
}

/* ================================================================== */
/* Public dispatcher                                                   */
/* ================================================================== */

size_t
awgn_generate (awgn_state_t *state, size_t n, float complex *out,
               size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  generate_scalar (state, n, out);
  return n;
}

int
awgn (uint64_t seed, float amplitude, size_t n, float complex *out)
{
  awgn_state_t *g = awgn_create (seed, amplitude);
  if (!g)
    return DP_ERR_MEMORY;
  awgn_generate (g, n, out, n);
  awgn_destroy (g);
  return DP_OK;
}

float
awgn_amplitude_for_snr (float snr_db, float signal_power)
{
  float snr_lin = powf (10.0f, snr_db / 10.0f);
  return sqrtf (signal_power / (2.0f * snr_lin));
}
