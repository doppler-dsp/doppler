/**
 * dp_state.h — the standard state **bytes interface** for doppler.
 *
 * Serialization is module-specific (only `lo` knows it holds a phase, only
 * `fir` knows it holds a delay line); the *bytes interface* around it is not.
 * This header owns that universal layer, once, for every serializable object:
 *
 *   - a self-describing envelope (`dp_state_hdr_t`: magic / version / endian /
 *     size) that prefixes every state blob,
 *   - writer / reader cursors that turn hand-packing into a few bounds-checked
 *     calls,
 *   - `dp_state_validate()` — the one check every `set_state` opens with, so a
 *     wrong-object / wrong-config / foreign-endian blob is rejected, never
 *     silently reinterpreted, and
 *   - `DP_DEFINE_RUN()` — the identical `obj_run` pure-transducer wrapper.
 *
 * The per-object ABI (the contract jm's `serializable` binding and the Rust FFI
 * call) stays:
 *
 *   size_t obj_state_bytes(const obj_state_t *);
 *   void   obj_get_state  (const obj_state_t *, void *blob);
 *   int    obj_set_state  (obj_state_t *, const void *blob);  // DP_OK / -err
 *
 * Blob layout, every object:   [ dp_state_hdr_t ] [ module payload ]
 * Compositions embed children as self-contained sub-blobs (each carries its own
 * header): [ hdr ] [ extra? ] [ child blob ]...  with
 * state_bytes = sizeof(hdr) + sizeof(extra) + Σ child_state_bytes.
 *
 * Blobs are native-endian POD for same-machine / same-arch resume (thread,
 * process, pod).  The endian byte is stamped and rejected on mismatch; there is
 * deliberately no cross-endian byte-swap.
 */
#ifndef DP_STATE_H
#define DP_STATE_H

#include "clib_common.h" /* DP_OK, DP_ERR_INVALID, fixed-width ints, memcpy */

/* FourCC type tag, e.g. DP_FOURCC('A','C','Q','R'). Stored little-end-first so
 * the bytes read as "ACQR" in a hex dump on a little-endian host. */
#define DP_FOURCC(a, b, c, d)                                                 \
  ((uint32_t) ((uint32_t) (uint8_t) (a) | ((uint32_t) (uint8_t) (b) << 8)     \
               | ((uint32_t) (uint8_t) (c) << 16)                             \
               | ((uint32_t) (uint8_t) (d) << 24)))

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define DP_STATE_ENDIAN 2u
#else
#  define DP_STATE_ENDIAN 1u /* little-endian (x86-64, arm64 — doppler targets) */
#endif

/**
 * @brief Common 16-byte envelope at the head of every state blob.
 *
 * 16 bytes keeps a following `double`/`uint64_t` (a composition's extra header)
 * naturally 8-aligned.
 */
typedef struct
{
  uint32_t magic;   /**< Per-object FourCC type tag (DP_FOURCC).            */
  uint16_t version; /**< Per-object blob format version.                    */
  uint8_t  endian;  /**< DP_STATE_ENDIAN at serialize time.                 */
  uint8_t  flags;   /**< Reserved; 0.                                       */
  uint32_t bytes;   /**< Total blob size; equals obj_state_bytes().       */
  uint32_t _pad;    /**< Reserved; 0.                                       */
} dp_state_hdr_t;

/* ── writer cursor ──────────────────────────────────────────────────────────
 * Sticky-error model: a bounds overrun sets `err` and subsequent writes no-op,
 * so call sites stay flat (check `w.err` once at the end if desired). Overrun
 * cannot happen when the caller allocated obj_state_bytes() — it is a guard. */
typedef struct
{
  uint8_t *buf;
  size_t   cap;
  size_t   off;
  int      err;
} dp_writer_t;

static inline dp_writer_t
dp_writer_init (void *blob, size_t cap)
{
  dp_writer_t w = { (uint8_t *) blob, cap, 0, 0 };
  return w;
}

static inline void
dp_w_bytes (dp_writer_t *w, const void *src, size_t n)
{
  if (w->err || w->off + n > w->cap)
    {
      w->err = 1;
      return;
    }
  memcpy (w->buf + w->off, src, n);
  w->off += n;
}

/** Reserve @p n bytes and return the region (for a child's get_state to fill);
 *  NULL on overrun. */
static inline void *
dp_w_reserve (dp_writer_t *w, size_t n)
{
  if (w->err || w->off + n > w->cap)
    {
      w->err = 1;
      return NULL;
    }
  void *p = w->buf + w->off;
  w->off += n;
  return p;
}

static inline void
dp_w_u32 (dp_writer_t *w, uint32_t v)
{
  dp_w_bytes (w, &v, sizeof v);
}
static inline void
dp_w_u64 (dp_writer_t *w, uint64_t v)
{
  dp_w_bytes (w, &v, sizeof v);
}
static inline void
dp_w_f64 (dp_writer_t *w, double v)
{
  dp_w_bytes (w, &v, sizeof v);
}
static inline void
dp_w_cf32 (dp_writer_t *w, const float _Complex *p, size_t n)
{
  dp_w_bytes (w, p, n * sizeof (float _Complex));
}
static inline void
dp_w_f32 (dp_writer_t *w, const float *p, size_t n)
{
  dp_w_bytes (w, p, n * sizeof (float));
}

/** Stamp the standard envelope. @p total must equal obj_state_bytes(). */
static inline void
dp_w_hdr (dp_writer_t *w, uint32_t magic, uint16_t version, size_t total)
{
  dp_state_hdr_t h
      = { magic, version, (uint8_t) DP_STATE_ENDIAN, 0, (uint32_t) total, 0 };
  dp_w_bytes (w, &h, sizeof h);
}

/* ── reader cursor ──────────────────────────────────────────────────────── */
typedef struct
{
  const uint8_t *buf;
  size_t         cap;
  size_t         off;
  int            err;
} dp_reader_t;

static inline dp_reader_t
dp_reader_init (const void *blob, size_t cap)
{
  dp_reader_t r = { (const uint8_t *) blob, cap, 0, 0 };
  return r;
}

static inline void
dp_r_bytes (dp_reader_t *r, void *dst, size_t n)
{
  if (r->err || r->off + n > r->cap)
    {
      r->err = 1;
      return;
    }
  memcpy (dst, r->buf + r->off, n);
  r->off += n;
}

/** Borrow @p n bytes in place (for a child's set_state to read); NULL on
 *  overrun. */
static inline const void *
dp_r_reserve (dp_reader_t *r, size_t n)
{
  if (r->err || r->off + n > r->cap)
    {
      r->err = 1;
      return NULL;
    }
  const void *p = r->buf + r->off;
  r->off += n;
  return p;
}

static inline uint32_t
dp_r_u32 (dp_reader_t *r)
{
  uint32_t v = 0;
  dp_r_bytes (r, &v, sizeof v);
  return v;
}
static inline uint64_t
dp_r_u64 (dp_reader_t *r)
{
  uint64_t v = 0;
  dp_r_bytes (r, &v, sizeof v);
  return v;
}
static inline double
dp_r_f64 (dp_reader_t *r)
{
  double v = 0.0;
  dp_r_bytes (r, &v, sizeof v);
  return v;
}
static inline void
dp_r_cf32 (dp_reader_t *r, float _Complex *p, size_t n)
{
  dp_r_bytes (r, p, n * sizeof (float _Complex));
}
static inline void
dp_r_f32 (dp_reader_t *r, float *p, size_t n)
{
  dp_r_bytes (r, p, n * sizeof (float));
}

/**
 * @brief Validate a blob's envelope before trusting its payload.
 *
 * Every obj_set_state opens with this.  @p expect_bytes is the receiving
 * object's obj_state_bytes() — a blob from a different object (magic),
 * format (version), endianness, or config/size (bytes) is rejected rather than
 * reinterpreted.
 *
 * @return DP_OK, or DP_ERR_INVALID on any mismatch.
 */
static inline int
dp_state_validate (const void *blob, size_t expect_bytes, uint32_t magic,
                   uint16_t version)
{
  if (expect_bytes < sizeof (dp_state_hdr_t))
    return DP_ERR_INVALID;
  const dp_state_hdr_t *h = (const dp_state_hdr_t *) blob;
  if (h->magic != magic || h->version != version
      || h->endian != (uint8_t) DP_STATE_ENDIAN
      || h->bytes != (uint32_t) expect_bytes)
    return DP_ERR_INVALID;
  return DP_OK;
}

/**
 * @brief Define the standard `<pfx>_run` pure-transducer wrapper.
 *
 * Generates `size_t <pfx>_run(state, state_in, state_out, in, n_in, out,
 * max_out)`: restore @p state_in (or keep current), call `<pfx>_execute`, then
 * export @p state_out — the `(state_in, input) -> (state_out, output)` face for
 * elastic fan-out.  For objects whose middle step is a single `_execute`; an
 * object with a different step shape (e.g. acq's frame/push) defines its own.
 */
#define DP_DEFINE_RUN(pfx, STATE_T, IN_T, OUT_T)                              \
  size_t pfx##_run (STATE_T *s, const void *state_in, void *state_out,        \
                    const IN_T *in, size_t n_in, OUT_T *out, size_t max_out)  \
  {                                                                           \
    if (state_in && pfx##_set_state (s, state_in) != DP_OK)                   \
      return 0;                                                               \
    size_t _n_out = pfx##_execute (s, in, n_in, out, max_out);               \
    if (state_out)                                                           \
      pfx##_get_state (s, state_out);                                         \
    return _n_out;                                                           \
  }

/**
 * @brief Define the whole-struct state triplet for a pointer-free POD object.
 *
 * Generates `<pfx>_state_bytes/get_state/set_state` that snapshot the entire
 * `STATE_T` after the envelope.  Correct only when the struct holds no pointers
 * (the snapshot would capture a stale address): the running state *and* the
 * derived config are serialized, and config restores identically into an
 * identically-built instance.  For a struct with pointers or a composition,
 * hand-write the triplet (pack running fields / delegate to children) instead.
 */
#define DP_DEFINE_POD_STATE(pfx, STATE_T, MAGIC, VERSION)                     \
  size_t pfx##_state_bytes (const STATE_T *s)                                 \
  {                                                                           \
    (void)s;                                                                  \
    return sizeof (dp_state_hdr_t) + sizeof (STATE_T);                        \
  }                                                                           \
  void pfx##_get_state (const STATE_T *s, void *blob)                         \
  {                                                                           \
    dp_writer_t _w = dp_writer_init (blob, pfx##_state_bytes (s));            \
    dp_w_hdr (&_w, (MAGIC), (VERSION), pfx##_state_bytes (s));                \
    dp_w_bytes (&_w, s, sizeof *s);                                           \
  }                                                                           \
  int pfx##_set_state (STATE_T *s, const void *blob)                         \
  {                                                                           \
    int _rc = dp_state_validate (blob, pfx##_state_bytes (s), (MAGIC),        \
                                 (VERSION));                                  \
    if (_rc != DP_OK)                                                         \
      return _rc;                                                             \
    dp_reader_t _r = dp_reader_init (blob, pfx##_state_bytes (s));            \
    _r.off         = sizeof (dp_state_hdr_t);                                 \
    dp_r_bytes (&_r, s, sizeof *s);                                           \
    return DP_OK;                                                             \
  }

/**
 * @brief Whole-struct POD triplet for an object carrying a live telemetry
 *        attachment (or any other non-state member that must not travel in
 *        the blob).
 *
 * Identical to DP_DEFINE_POD_STATE except that @p MEMBER — a struct member
 * holding live pointers/ids (e.g. an object's `tlm` attachment) — is zeroed
 * in the serialized copy (deterministic blobs, no leaked address) and kept
 * live across set_state (a restore must not clobber the receiving
 * instance's attachment with the sender's stale one).
 */
#define DP_DEFINE_POD_STATE_TLM(pfx, STATE_T, MAGIC, VERSION, MEMBER)         \
  size_t pfx##_state_bytes (const STATE_T *s)                                 \
  {                                                                           \
    (void)s;                                                                  \
    return sizeof (dp_state_hdr_t) + sizeof (STATE_T);                        \
  }                                                                           \
  void pfx##_get_state (const STATE_T *s, void *blob)                         \
  {                                                                           \
    STATE_T _c = *s;                                                          \
    memset (&_c.MEMBER, 0, sizeof _c.MEMBER);                                 \
    dp_writer_t _w = dp_writer_init (blob, pfx##_state_bytes (s));            \
    dp_w_hdr (&_w, (MAGIC), (VERSION), pfx##_state_bytes (s));                \
    dp_w_bytes (&_w, &_c, sizeof _c);                                         \
  }                                                                           \
  int pfx##_set_state (STATE_T *s, const void *blob)                         \
  {                                                                           \
    int _rc = dp_state_validate (blob, pfx##_state_bytes (s), (MAGIC),        \
                                 (VERSION));                                  \
    if (_rc != DP_OK)                                                         \
      return _rc;                                                             \
    STATE_T _c;                                                               \
    dp_reader_t _r = dp_reader_init (blob, pfx##_state_bytes (s));            \
    _r.off         = sizeof (dp_state_hdr_t);                                 \
    dp_r_bytes (&_r, &_c, sizeof _c);                                         \
    _c.MEMBER = s->MEMBER; /* keep the live attachment */                     \
    *s = _c;                                                                  \
    return DP_OK;                                                             \
  }

/* ── field-wise scaffolding ──────────────────────────────────────────────────
 * For a hand-written triplet that packs a subset of fields (running state only;
 * config restored by create()).  Use inside `<obj>_get_state(s, blob)` /
 * `<obj>_set_state(s, blob)` (those exact param names): open, then pack/unpack
 * the running fields via `&_w` / `&_r`, and `return DP_OK;` from set.
 *
 *   void foo_get_state (const foo_state_t *s, void *blob)
 *   { DP_GET_OPEN (FOO_MAGIC, FOO_VERSION, foo_state_bytes (s));
 *     dp_w_f64 (&_w, s->gain); }
 *   int  foo_set_state (foo_state_t *s, const void *blob)
 *   { DP_SET_OPEN (FOO_MAGIC, FOO_VERSION, foo_state_bytes (s));
 *     s->gain = dp_r_f64 (&_r); return DP_OK; }
 */
#define DP_GET_OPEN(MAGIC, VERSION, BYTES)                                     \
  dp_writer_t _w = dp_writer_init (blob, (BYTES));                             \
  dp_w_hdr (&_w, (MAGIC), (VERSION), (BYTES))

#define DP_SET_OPEN(MAGIC, VERSION, BYTES)                                     \
  int _rc = dp_state_validate (blob, (BYTES), (MAGIC), (VERSION));             \
  if (_rc != DP_OK)                                                            \
    return _rc;                                                                \
  dp_reader_t _r = dp_reader_init (blob, (BYTES));                             \
  _r.off = sizeof (dp_state_hdr_t)

/* ── composition: nest a child's self-contained sub-blob ─────────────────────
 * A composition's state_bytes sums `<child>_state_bytes(child_ptr)`; its
 * get/set then writes/reads each child as a nested envelope.  `child_ptr` may be
 * a pointer member or the address of an embedded-by-value member (`&s->lf`).
 * DP_R_CHILD returns DP_ERR_INVALID from the enclosing set_state if the child
 * rejects, so the whole restore is atomic-by-validation. */
#define DP_W_CHILD(w, pfx, child_ptr)                                          \
  do                                                                           \
    {                                                                          \
      void *_cr = dp_w_reserve ((w), pfx##_state_bytes (child_ptr));           \
      if (_cr)                                                                 \
        pfx##_get_state ((child_ptr), _cr);                                    \
    }                                                                          \
  while (0)

#define DP_R_CHILD(r, pfx, child_ptr)                                          \
  do                                                                           \
    {                                                                          \
      const void *_cr = dp_r_reserve ((r), pfx##_state_bytes (child_ptr));     \
      if (!_cr || pfx##_set_state ((child_ptr), _cr) != DP_OK)                 \
        return DP_ERR_INVALID;                                                 \
    }                                                                          \
  while (0)

#endif /* DP_STATE_H */
