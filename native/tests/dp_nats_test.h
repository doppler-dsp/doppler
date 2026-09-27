/**
 * @file dp_nats_test.h
 * @brief The live-broker fixture every NATS test shares: is a broker there,
 *        a subject no other run will collide with, and the settle wait.
 *
 * `test_stream_nats_core` and `test_tlm_sink` each carried the same three
 * pieces, written against BSD sockets and `getpid`/`usleep`, which is what
 * kept both off Windows (#1575). They are built here from doppler's own
 * portable primitives instead:
 *
 *   - the broker probe opens a subscriber through doppler's stream API --
 *     the thing under test decides whether a broker is reachable, so there
 *     is no second networking stack in the harness;
 *   - the unique subject is keyed by the monotonic clock (`dp_mono_ns`);
 *   - the settle wait is `dp_thread_sleep_us`.
 *
 * A test with no broker exits DP_NATS_SKIP, which its CMake target declares
 * as SKIP_RETURN_CODE: absent, the test is SKIPPED, never passed.
 */
#ifndef DP_NATS_TEST_H
#define DP_NATS_TEST_H

#include "doppler/dp_thread.h"
#include "doppler/stream/stream.h"
#include "doppler/timing/timing_core.h"
#include <stdio.h>

/** Exit code a broker-less run returns; the CTest target's SKIP_RETURN_CODE.
 */
#define DP_NATS_SKIP 77

/** The broker every live test targets (`make nats-up` starts one here). */
#define DP_NATS_URL "nats://127.0.0.1:4222"

/** Core NATS keeps no history: a subscriber must exist before the publish. */
#define DP_NATS_SETTLE_US 300000u

/** 1 when a NATS broker answers at DP_NATS_URL, else 0. */
static inline int
dp_nats_broker_reachable (void)
{
  dp_sub_t *probe = dp_sub_create (DP_NATS_URL "/dp-nats-test-probe");
  if (!probe)
    return 0;
  dp_sub_destroy (probe);
  return 1;
}

/**
 * A subject no concurrent or earlier run shares: `hint` plus the monotonic
 * clock. Returns a static buffer, valid until the next call.
 */
static inline const char *
dp_nats_endpoint (const char *hint)
{
  static char buf[160];
  snprintf (buf, sizeof buf, DP_NATS_URL "/%s-%llu", hint,
            (unsigned long long)dp_mono_ns ());
  return buf;
}

/** Wait for a just-created subscriber to be live on the broker. */
static inline void
dp_nats_settle (void)
{
  dp_thread_sleep_us (DP_NATS_SETTLE_US);
}

#endif /* DP_NATS_TEST_H */
