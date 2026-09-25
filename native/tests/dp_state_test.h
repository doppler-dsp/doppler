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
 * acq_get_state did exactly that with its ring's unused tail: the blob's
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

#endif /* DP_STATE_TEST_H */
