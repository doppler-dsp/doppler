

# File dp\_state.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_state.h**](dp__state_8h.md)

[Go to the documentation of this file](dp__state_8h.md)


```C++

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

typedef struct
{
  uint32_t magic;   
  uint16_t version; 
  uint8_t  endian;  
  uint8_t  flags;   
  uint32_t bytes;   
  uint32_t _pad;    
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
```


