/**
 * dp_state_test.h — uniform round-trip + reject test for the state bytes
 * interface (see native/inc/dp_state.h).
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

#define DP_STATE_ROUNDTRIP_TEST(pfx, a, b)                                    \
  do                                                                          \
    {                                                                         \
      size_t _cb   = pfx##_state_bytes (a);                                   \
      void  *_blob = malloc (_cb);                                            \
      DP_CHECK (_blob != NULL);                                               \
      pfx##_get_state ((a), _blob);                                           \
      DP_CHECK (pfx##_set_state ((b), _blob) == DP_OK);                       \
      ((char *)_blob)[0] ^= (char)0xFF; /* clobber the envelope magic */      \
      DP_CHECK (pfx##_set_state ((b), _blob) == DP_ERR_INVALID);              \
      free (_blob);                                                           \
    }                                                                         \
  while (0)

#endif /* DP_STATE_TEST_H */
