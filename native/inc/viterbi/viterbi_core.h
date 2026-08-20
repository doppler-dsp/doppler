/**
 * @file viterbi_core.h
 * @brief Soft-decision Viterbi decoding of convolutional codes.
 *
 * `conv` owns the CODE — polynomials, the encoder, the trellis arithmetic —
 * and this owns the DECODER built over one. A caller names the generator
 * polynomials and gets a decoder for them; nothing here knows about CCSDS,
 * which is a configuration of the same code family (see `ccsds_tm`).
 *
 * Soft in, hard out: `decode` takes log-likelihood ratios, one per channel
 * symbol, and returns decoded information bits. A hard-decision decoder
 * throws away most of the gain the code exists to provide, which is why the
 * input is LLRs rather than bits.
 *
 * Lifecycle: `create -> [decode / reset]* -> destroy`.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import Viterbi
 * >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
 * >>> llr = np.array([2.0, -2.0] * 64, dtype=np.float32)
 * >>> bits = v.decode(llr)
 * >>> bits.dtype, len(bits) > 0
 * (dtype('uint8'), True)
 * @endcode
 */
#ifndef VITERBI_CORE_H
#define VITERBI_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "conv/conv_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A streaming maximum-likelihood (Viterbi) decoder.
 *
 * Opaque and heap-allocated: the path metrics and the traceback ring are
 * sized from the code and the depth, and both are wanted contiguous.
 *
 * Allocate with viterbi_create().
 */
typedef struct
{
  conv_code_t code;
  size_t      depth;
  uint32_t    nstate;

  float   *pm;   /**< path metric per state                              */
  float   *pm2;  /**< the next step's, swapped rather than copied        */
  uint8_t *dec;  /**< depth x nstate decisions: which predecessor won   */
  size_t   head; /**< ring cursor, in steps                            */
  size_t   fill; /**< steps recorded, saturating at depth              */

  /* Derived once: for each state, its two predecessors and their output
     words. The butterfly -- predecessors (ns << 1) & mask and | 1, both on
     the same input bit ns >> (k-2) -- holds for every k. */
  uint32_t *pred0;
  uint32_t *pred1;
  unsigned *out0;
  unsigned *out1;
  unsigned *inbit;
/*<<property_struct_fields>>*/
} viterbi_state_t;

/**
 * @brief Build a decoder for the code the polynomials describe.
 *
 * The array IS the code: its length gives the number of outputs per input
 * bit, so `[0o171, 0o133]` is a rate-1/2 code and a three-element array is
 * rate 1/3. @p k is the constraint length, which sets the trellis to
 * `2^(k-1)` states — the dominant term in what a decode costs.
 *
 * @p depth is the traceback depth in information bits. The conventional
 * rule of thumb is `5*(k-1)` or more; a longer depth is safer at low Es/N0
 * and costs only the traceback walk, not the add-compare-selects (measured
 * in `native/benchmarks/bench_viterbi_core.c`).
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import Viterbi
 * >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
 * >>> v.decode(np.zeros(8, dtype=np.float32)).dtype
 * dtype('uint8')
 * @endcode
 *
 * @param poly      Generator polynomials, one per output. The array IS the
 *                  code; `poly_len` gives `n`.
 * @param poly_len  Number of polynomials, 1 to `CONV_N_MAX`.
 * @param k  k (default: 7).
 * @param invert  invert (default: 0).
 * @param depth  depth (default: 35).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call viterbi_destroy() when done.
 */
viterbi_state_t *viterbi_create(const uint32_t *poly, size_t poly_len, uint32_t k, uint32_t invert, size_t depth);

/**
 * @brief Free a decoder and everything it allocated. NULL is a no-op.
 * @param state  May be NULL.
 */
void viterbi_destroy(viterbi_state_t *state);

/**
 * @brief Return to the all-zero start state, discarding the traceback.
 *
 * The code and the depth are unchanged — this is the boundary between two
 * independent captures, not a reconfiguration. The next decode refills the
 * traceback before it emits, exactly as after create, and the all-zero state
 * is given the winning metric, matching an encoder that starts from a reset
 * register.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.coding import Viterbi
 * >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
 * >>> v.reset()
 * @endcode
 */
void viterbi_reset(viterbi_state_t *state);

/**
 * @brief Bits @ref viterbi_decode will emit for @p n_in soft symbols.
 *
 * Accounts for the fill still owed at the start of a stream, so a caller can
 * size a buffer exactly rather than conservatively.
 *
 * @param state  The decoder.
 * @param n_in   Number of soft symbols the next call would be given.
 * @return       Bits that call would write.
 */
size_t viterbi_decode_max_out (const viterbi_state_t *state, size_t n_in);

/**
 * @brief Decode soft channel symbols into information bits.
 *
 * The input carries one value per channel symbol, in the convention
 * `mpsk_soft_demap` produces: `L = log(P(0)/P(1))`, so **positive means
 * symbol 0**. The branch metric for an expected symbol @c e is `+L` when
 * `e == 0` and `-L` otherwise, and the survivor maximises the sum — which
 * makes the decoder agree with `mpsk_demap` on hard decisions by
 * construction rather than by a second convention.
 *
 * A maximum-likelihood path cannot move when every metric is scaled by a
 * positive constant, so **the LLRs need no accurate scaling** — a caller
 * with no SNR estimate may pass unscaled values.
 *
 * Streaming: state carries across calls, so a long capture may be fed in
 * blocks and the bits come out continuously. The first `depth - 1` branches
 * of a stream produce no output — the traceback walks `depth - 1` steps
 * back, so a decision needs that many branches BEHIND it — and thereafter
 * one bit is emitted per `n` symbols consumed. @ref viterbi_decode_max_out
 * is the same statement as arithmetic, and is what a caller should size a
 * buffer with rather than repeating this sentence: they disagreed by one
 * until a test asserted the count against a literal.
 *
 * @param state    The decoder.
 * @param in       Log-likelihood ratios, one per channel symbol. @p n_in
 *                 must be a multiple of the code's `n`.
 * @param n_in     Number of LLRs in @p in.
 * @param out      Receives the decoded information bits, one per byte.
 * @param max_out  Capacity of @p out; see @ref viterbi_decode_max_out.
 * @return         Bits written, which may be 0 while the traceback fills.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import Viterbi
 * >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
 * >>> llr = np.array([2.0, -2.0] * 128, dtype=np.float32)
 * >>> bits = v.decode(llr)
 * >>> set(np.unique(bits)) <= {0, 1}
 * True
 * @endcode
 */
size_t viterbi_decode(viterbi_state_t *state, const float *in, size_t n_in, uint8_t *out, size_t max_out);

/* ── hand-owned: the surface jm does not declare ───────────────────────────
 *
 * jm declares the lifecycle and `decode` from objects/viterbi.toml. What
 * follows is this component's own C API — the conv_code_t constructor its
 * internal callers use, node synchronization, and the state triplet (which
 * is hand-written per docs/design/state-serialization.md; the manifest's
 * `serializable` flag generates the PYTHON side over it).
 */

/**
 * @brief Build a decoder from a code already assembled.
 *
 * The declared `viterbi_create` takes the polynomials directly, because a
 * struct pointer is not expressible in a manifest. Callers that already hold
 * a @ref conv_code_t — the CCSDS configuration, the validators — use this.
 *
 * @param c      The code. Copied, so the caller's may be temporary.
 * @param depth  Traceback depth in input bits. A decision is emitted only
 *               after `depth - 1` further bits have been seen, which is the
 *               decoder's latency and the dominant term in its memory.
 *               **60 is the measured choice for CCSDS's K = 7 rate-1/2
 *               code** — `5*K = 35`, the textbook number, sits 33 % above
 *               the achievable BER (docs/design/viterbi.md section 4). It is
 *               a default for other codes, not a law.
 * @return       The decoder, or NULL if @p c is invalid, @p depth is 0, or
 *               allocation failed.
 */
viterbi_state_t *viterbi_create_code (const conv_code_t *c, size_t depth);

/** @brief The code this decoder was built for. */
const conv_code_t *viterbi_code (const viterbi_state_t *s);

/** @brief Its traceback depth, in input bits. */
size_t viterbi_depth (const viterbi_state_t *s);

/* ── node synchronization ────────────────────────────────────────────── */

/**
 * @brief What one alignment hypothesis scored, and what the runner-up did.
 *
 * `errors` against `symbols` IS the channel symbol error rate when the
 * hypothesis is right, because in sync the decoder corrects the channel and
 * the re-encoded stream differs from the received one exactly where the
 * channel put an error.
 *
 * Out of sync the decoder is searching a trellis its input does not lie on.
 * The count then runs at a large fraction of the symbols — but **not at a
 * half**, and the difference is worth stating because a half is what a
 * coin-flip argument predicts and it is wrong: the decoder is a maximum
 * LIKELIHOOD search, so it finds the codeword that agrees with the
 * misaligned stream as well as any codeword can. Measured on a clean
 * stream: **24 % of symbols for CCSDS K=7 r=1/2, 23 % for the same code
 * uninverted, 18 % for a K=5 r=1/3** — against 0 % for the right
 * alignment, which is the separation the decision actually rests on.
 *
 * @c margin is what a caller acts on. The absolute count moves with Es/N0
 * and says nothing on its own; the DIFFERENCE between the best and the next
 * best is the evidence that the search decided.
 */
typedef struct
{
  unsigned phase;   /**< winning offset, `0 .. c->n-1`               */
  size_t   errors;  /**< its disagreements                           */
  size_t   next;    /**< the best competing hypothesis's             */
  size_t   symbols; /**< symbols SCORED per hypothesis, which is
                         fewer than the window — see
                         @ref node_sync_scored_symbols               */
  size_t   margin;  /**< `next - errors`; 0 when nothing separated   */
} node_sync_t;

/**
 * @brief Score the alignment as given: decode, re-encode, count
 *        disagreements against the received hard decisions.
 *
 * The **re-encoding metric**. It needs no truth, no marker and no training
 * sequence — it compares the decoder's own output against the decoder's own
 * input — so it works on a live capture, which is what makes it the
 * statistic a receiver can carry. `docs/design/viterbi.md` §9 derives what
 * it reads in and out of sync, and why a marker correlation is the wrong
 * tool for this even when a marker exists.
 *
 * **It is blind to polarity, and that is correct.** A transparent code
 * (every generator of odd weight, which CCSDS's are) decodes an inverted
 * stream to the complement of the bits, which re-encodes to the inverted
 * symbols — so the disagreement count is identical. Polarity is resolved
 * downstream by something that knows what the bits mean; this resolves
 * only which symbol starts a branch.
 *
 * The first `k - 1` decoded bits are excluded from the count: the encoder
 * used for the comparison starts from a zero register while the real one
 * was mid-stream, so those bits are re-encoded from the wrong state and
 * would bias every hypothesis by a few symbols.
 *
 * @param v      A decoder for the code being synchronized. It is RESET, and
 *               left holding this scoring run's state — a caller decoding
 *               with it afterwards must reset it again.
 * @param llr    Soft symbols, `mpsk_soft_demap`'s convention.
 * @param n_llr  Number of symbols; the tail beyond a whole number of
 *               branches is ignored.
 * @return       Disagreements, or 0 if the window is too short to decode
 *               anything past the traceback and the encoder fill.
 */
size_t node_sync_score (viterbi_state_t *v, const float *llr, size_t n_llr);

/**
 * @brief Symbols @ref node_sync_score will actually score for a window of
 *        @p n_llr, which is fewer than @p n_llr.
 *
 * The head of a window is skipped: the decoder starts from its own
 * all-zero prior, which is wrong whenever the window opens mid-capture, and
 * the comparison encoder starts from a zero register while the
 * transmitter's was mid-stream. A caller reading `errors / symbols` as a
 * channel symbol error rate wants this denominator rather than the window
 * length.
 */
size_t node_sync_scored_symbols (const viterbi_state_t *v, size_t n_llr);

/**
 * @brief Try every branch alignment and report which one the stream is on.
 *
 * `c->n` hypotheses for a rate-1/n code — the offsets `0 .. n-1` — each
 * scored by @ref node_sync_score over the same window.
 *
 * **Re-runnable, and it has to be.** A symbol slip moves the stream by an
 * odd number of symbols and the alignment changes mid-capture; measured
 * through a real receiver at Es/N0 = 0 dB, that happened three times in
 * forty-six frame slots (`docs/design/fec-receive.md` §8). A one-shot at
 * start of stream would decode noise from the first slip onward, so this
 * takes its window as an argument and holds no state between calls.
 *
 * @param v      A decoder for the code; reset per hypothesis.
 * @param llr    Soft symbols.
 * @param n_llr  Window length. It buys the separation: the counts differ by
 *               about `0.5 - SER` per symbol, so a window of a few hundred
 *               symbols decides at any Es/N0 a coded link runs at.
 * @param out    Receives the outcome; may be `NULL`.
 * @return       Non-zero when a hypothesis was scored. Zero — with @p out
 *               untouched — when the window is too short.
 *
 * @code
 * node_sync_t ns;
 * if (node_sync_scan (v, llr, 1000, &ns) && ns.margin > 100)
 *   {
 *     viterbi_reset (v);
 *     viterbi_decode (v, llr + ns.phase, n - ns.phase, bits, cap);
 *   }
 * @endcode
 */
int node_sync_scan (viterbi_state_t *v, const float *llr, size_t n_llr,
                    node_sync_t *out);

/* ── the state bytes interface ───────────────────────────────────────────
 *
 * The decoder carries running state across calls — a path metric per state,
 * the traceback ring, and where the ring is — so it speaks the standard
 * bytes interface like every other stateful object in the tree. A decoder
 * sits inside a chain (behind the receiver, in front of the R-S decoder),
 * and one link that cannot be checkpointed is enough to make the chain
 * un-resumable. See docs/design/state-serialization.md.
 */

/** @brief Blob type tag: "VTRB". */
#define VITERBI_STATE_MAGIC DP_FOURCC ('V', 'T', 'R', 'B')
/** @brief Blob format version. */
#define VITERBI_STATE_VERSION 1u

/**
 * @brief Bytes @ref viterbi_get_state writes: envelope, code identity,
 *        ring cursor, the path metrics and the traceback ring.
 *
 * Depends on the configuration (`2^(k-1)` metrics and a
 * `depth x 2^(k-1)` ring), so it is not a constant across decoders.
 */
size_t viterbi_state_bytes (const viterbi_state_t *s);

/**
 * @brief Serialize @p s into @p blob, which must hold
 *        @ref viterbi_state_bytes bytes.
 *
 * The ring travels in its stored order with the cursor beside it rather
 * than rotated into a canonical one — the rotation would cost a pass and
 * buy nothing, since only @ref viterbi_set_state reads it back.
 */
void viterbi_get_state (const viterbi_state_t *s, void *blob);

/**
 * @brief Restore @p s from @p blob.
 *
 * The code and the depth are configuration, restored by
 * @ref viterbi_create rather than carried in the payload — but they are
 * *stamped* in it and checked here, because a size match is not a
 * configuration match: two codes with the same `k` and `n` differing only
 * in a polynomial or in @c invert produce blobs of identical length, and
 * reinterpreting one as the other yields a decoder that is confidently
 * wrong rather than one that refuses.
 *
 * @return @c DP_OK, or @c DP_ERR_INVALID if the envelope, the code, the
 *         depth, or the ring cursor does not match this decoder — in which
 *         case @p s is untouched.
 */
int viterbi_set_state (viterbi_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* VITERBI_CORE_H */
