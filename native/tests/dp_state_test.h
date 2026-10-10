/**
 * dp_state_test.h — uniform round-trip + reject test for the state bytes
 * interface (see native/inc/doppler/dp_state.h).
 *
 * Every serializable object's C test calls one macro: serialize object @p a,
 * restore into a *fresh* object @p b of the same config (must succeed), then
 * clobber the envelope magic and confirm the restore is rejected (not silently
 * reinterpreted).  The reject half is finally meaningful for leaves, which had
 * no envelope to validate before the standard.
 *
 * Requires the DP_OK / DP_ERR_INVALID codes (via clib_common.h) and
 * malloc/free. It used to also require `CHECK` -- "already present in every
 * test_*_core.c", which was true only because 90 files each defined their
 * own, in six incompatible ways. It includes dp_test.h now, so the
 * requirement is a dependency rather than an expectation of the caller.
 */
#ifndef DP_STATE_TEST_H
#define DP_STATE_TEST_H

#include "dp_test.h"
#include <stdlib.h>
#include <string.h>

/*
 * The FIDELITY half was missing until test_dp_state.c went looking for it.
 *
 * The macro asserted that `set_state` RETURNS DP_OK on a good blob and
 * DP_ERR_INVALID on a clobbered one -- and never that the restored object
 * carries the state it was handed. So a `set_state` that validated the
 * envelope, returned DP_OK and restored NOTHING passed at all 31 call sites.
 * The project's actual claim is bit-exact resume; every object that meets it
 * did so in its own test, by hand, while the shared macro a new object reaches
 * for first proved only the envelope.
 *
 * Re-serializing `b` and comparing to `a`'s blob is the generic form of that
 * check, and it needs no knowledge of the object. The standard is what makes
 * it well defined: a blob carries only the RUNNING fields (config is restored
 * by `create()`), and `b` is required to be a fresh object of the SAME config,
 * so two objects in the same state must serialize identically.
 */
/*
 * The DETERMINISM half was missing too (doppler#1471).
 *
 * The fidelity check compares two blobs from two fresh `malloc`s, and a fresh
 * allocation is usually a zeroed page -- so a `get_state` that left bytes of
 * its own blob unwritten produced two identical, all-zero gaps and passed.
 * dp_acq_get_state did exactly that with its ring's unused tail: the blob's
 * bytes depended on whatever the caller's buffer held, and a blob shipped to
 * another pod carried that heap along with it.
 *
 * So both buffers are FILLED first, with two different patterns, and `a` is
 * serialized into both. Every byte `get_state` owns must then agree; a byte
 * it skipped still holds 0xA5 in one and 0x5A in the other.
 */
#define DP_STATE_ROUNDTRIP_TEST(pfx, a, b)                                    \
  do                                                                          \
    {                                                                         \
      size_t _cb   = pfx##_state_bytes (a);                                   \
      void  *_blob = malloc (_cb);                                            \
      void  *_back = malloc (_cb);                                            \
      DP_CHECK (_blob != NULL && _back != NULL);                              \
      memset (_blob, 0xA5, _cb);                                              \
      memset (_back, 0x5A, _cb);                                              \
      pfx##_get_state ((a), _blob);                                           \
      pfx##_get_state ((a), _back);                                           \
      /* Determinism: every byte of the blob is written, whatever the         \
         buffer held before. */                                               \
      DP_CHECK (memcmp (_blob, _back, _cb) == 0);                             \
      memset (_back, 0x5A, _cb);                                              \
      DP_CHECK (pfx##_set_state ((b), _blob) == DP_OK);                       \
      /* Fidelity: b must now BE a, which it re-serializing identically is    \
         the object-agnostic way to say. */                                   \
      DP_CHECK (pfx##_state_bytes (b) == _cb);                                \
      pfx##_get_state ((b), _back);                                           \
      DP_CHECK (memcmp (_blob, _back, _cb) == 0);                             \
      ((char *)_blob)[0] ^= (char)0xFF; /* clobber the envelope magic */      \
      DP_CHECK (pfx##_set_state ((b), _blob) == DP_ERR_INVALID);              \
      free (_blob);                                                           \
      free (_back);                                                           \
    }                                                                         \
  while (0)

/*
 * A CHILD's old format is refused through its PARENT (#2094).
 *
 * A composition nests each child's self-validating sub-blob, so a child's
 * version bump needs no bump in the parent: the child's own envelope check
 * runs inside the parent's set_state. This proves it rather than assuming it.
 * Serialize @p obj, find the one nested envelope whose magic is
 * @p child_magic, stamp it with @p old_version (every other byte stays
 * valid, so only the version can refuse it), and require the parent to
 * refuse the blob AND to be unchanged by the refusal: it re-serializes to
 * the original blob byte for byte. Exactly one nested envelope of that magic
 * must exist, so a renamed or flattened child turns this red rather than
 * passing vacuously.
 */
#define DP_STATE_NESTED_VERSION_TEST(pfx, obj, child_magic, old_version)      \
  do                                                                          \
    {                                                                         \
      const size_t   _nb = pfx##_state_bytes (obj);                           \
      unsigned char *_g  = (unsigned char *)malloc (_nb);                     \
      unsigned char *_l  = (unsigned char *)malloc (_nb);                     \
      unsigned char *_a  = (unsigned char *)malloc (_nb);                     \
      DP_CHECK (_g != NULL && _l != NULL && _a != NULL);                      \
      if (_g && _l && _a)                                                     \
        {                                                                     \
          pfx##_get_state ((obj), _g);                                        \
          memcpy (_l, _g, _nb);                                               \
          size_t _at = 0, _hits = 0;                                          \
          for (size_t _i = sizeof (dp_state_hdr_t);                           \
               _i + sizeof (dp_state_hdr_t) <= _nb; _i++)                     \
            {                                                                 \
              dp_state_hdr_t _h;                                              \
              memcpy (&_h, _g + _i, sizeof _h);                               \
              if (_h.magic == (uint32_t)(child_magic))                        \
                _at = _i, _hits++;                                            \
            }                                                                 \
          DP_CHECK (_hits == 1);                                              \
          if (_hits == 1)                                                     \
            {                                                                 \
              dp_state_hdr_t _h;                                              \
              memcpy (&_h, _l + _at, sizeof _h);                              \
              _h.version = (uint16_t)(old_version);                           \
              memcpy (_l + _at, &_h, sizeof _h);                              \
              DP_CHECK (pfx##_set_state ((obj), _l) == DP_ERR_INVALID);       \
              pfx##_get_state ((obj), _a);                                    \
              DP_CHECK (memcmp (_a, _g, _nb) == 0);                           \
            }                                                                 \
        }                                                                     \
      free (_g);                                                              \
      free (_l);                                                              \
      free (_a);                                                              \
    }                                                                         \
  while (0)

#endif /* DP_STATE_TEST_H */
