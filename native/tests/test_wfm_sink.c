/*
 * test_wfm_sink.c — NATS PUB sink smoke test (Phase B, POSIX only).
 *
 * Opens a PUB on a unique subject per wire type and publishes a couple of
 * blocks, asserting the convert+send path succeeds (dp_pub_send returns 0).
 * A PUB with no subscriber still reports success (fire-and-forget), so this
 * is deterministic — the full pub→sub round-trip is exercised by
 * test_stream_nats_core.c and the streaming example apps, not here.
 *
 * Skips (exit 77, CTest SKIP_RETURN_CODE) rather than fails when no
 * nats-server is reachable on 127.0.0.1:4222.
 */
#include "doppler/wfm/wfm_sink.h"
#include "dp_test.h"

#include "doppler/dp_complex.h"
#include "dp_nats_test.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

/* One sink per case, on a subject unique to this run: a PUB with no
 * subscriber still reports success, so the assertions below are about the
 * convert/track path and stay deterministic. */
static wfm_stream_sink_t *
open_sink (int sample_type)
{
  static int seq = 0;
  char       ep[160];
  snprintf (ep, sizeof ep, DP_NATS_URL "/wfm-sink-cert-%d-%d-%llu",
            sample_type, seq++, (unsigned long long)dp_mono_ns ());
  return dp_wfm_stream_sink_open (ep, sample_type);
}

int
main (void)
{
  if (!dp_nats_broker_reachable ())
    {
      printf ("SKIP: no nats-server on 127.0.0.1:4222 (run `nats-server "
              "-js`)\n");
      return DP_NATS_SKIP;
    }

  float _Complex blk[256];
  for (int i = 0; i < 256; i++)
    blk[i] = 0.5f + 0.5f * I;

  /* one PUB per wire type on a unique subject */
  for (int t = 0; t < 5; t++)
    {
      char ep[160];
      snprintf (ep, sizeof ep, DP_NATS_URL "/wfm-sink-test-%d-%llu", t,
                (unsigned long long)dp_mono_ns ());
      wfm_stream_sink_t *s = dp_wfm_stream_sink_open (ep, t);
      DP_REQUIRE_MSG (s, "sink open");
      DP_REQUIRE_MSG (dp_wfm_stream_sink_send (s, blk, 256, 1e6, 2.4e9) == 0,
                      "send 1");
      DP_REQUIRE_MSG (dp_wfm_stream_sink_send (s, blk, 256, 1e6, 2.4e9) == 0,
                      "send 2");
      dp_wfm_stream_sink_close (s);
    }

  /* invalid wire type → NULL */
  DP_REQUIRE_MSG (
      !dp_wfm_stream_sink_open ("nats://127.0.0.1:4222/wfm-sink-bad", 9),
      "bad type rejected");

  /* drain(NULL) is DP_OK, not a crash and not an error: nothing was
     buffered, so the question "has everything reached the server" is
     vacuously yes. A caller draining in a cleanup path should not have to
     guard the call. */
  DP_REQUIRE_MSG (dp_wfm_stream_sink_drain (NULL, 100) == DP_OK,
                  "draining a NULL sink is vacuously fine");

  /* ── §A  available() must not lie about the component that is linked ─────
   *
   * Zero mentions anywhere in the tree, and wfmgen gates its entire
   * `--output nats://` path on it: the pure-C core ships WEAK no-op stubs of
   * every wfm_stream_sink_* symbol so it links on Mach-O, and linking
   * libdoppler_stream replaces them with the real ones. A stub that reported
   * 1 would send every block into a no-op and report success.
   *
   * The check is that the two agree: this binary opened a sink and published
   * through it above, so the strong definitions ARE linked, and available()
   * must say so. (Under the stubs, open() returns NULL and this test skips
   * at the broker probe long before here.)
   */
  {
    DP_CHECK_MSG (dp_wfm_stream_sink_available () == 1,
                  "available() reports the real component, which is linked");
  }

  /* ── §B  peak: an exact number, not an impression (integer paths) ────────
   *
   * The header promises "largest per-axis magnitude seen on an integer path
   * (pre-clip, full-scale 1), > 1.0 => clipped" -- the number a caller sets
   * headroom by. Untested. It is exactly max(|Re*gain|, |Im*gain|) over
   * everything sent, so it can be asserted against arithmetic rather than
   * against a range.
   */
  {
    float _Complex a[64], b[64];
    for (int i = 0; i < 64; i++)
      {
        a[i] = 0.25f + 0.10f * I;
        b[i] = 0.75f - 0.30f * I;
      }
    wfm_stream_sink_t *s = open_sink (3); /* ci16 */
    DP_REQUIRE_MSG (s, "peak: open");
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 0.0, 1e-12); /* nothing sent */
    DP_CHECK (dp_wfm_stream_sink_send (s, a, 64, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 0.25, 1e-6);
    /* RUNNING max across sends, not per-block: the larger block raises it */
    DP_CHECK (dp_wfm_stream_sink_send (s, b, 64, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 0.75, 1e-6);
    /* ... and the smaller one after it does NOT lower it again */
    DP_CHECK (dp_wfm_stream_sink_send (s, a, 64, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 0.75, 1e-6);
    dp_wfm_stream_sink_close (s);
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (NULL), 0.0, 1e-12);
  }

  /* ── §C  set_gain scales what is measured, and peak crosses 1.0 ─────────
   *
   * `set_gain` is the headroom knob -- "for headroom H dB pass 10^(-H/20)"
   * -- and had zero mentions. The peak is tracked AFTER the gain and BEFORE
   * the clamp, so a gain that drives the signal past full scale is visible
   * as peak > 1.0 rather than being hidden by the saturation that follows.
   * That ordering is the whole value of the number, and it is what this
   * pins: 0.5 at gain 3.0 reads 1.5, not the 1.0 a post-clamp peak would.
   */
  {
    float _Complex x[32];
    for (int i = 0; i < 32; i++)
      x[i] = 0.5f + 0.5f * I;
    const double cases[][2] = {
      /* gain, expected peak */
      { 1.0, 0.5 },
      { 0.5, 0.25 },
      { 1.6, 0.8 },
      { 3.0, 1.5 },
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++)
      {
        wfm_stream_sink_t *s = open_sink (3); /* ci16 */
        DP_REQUIRE_MSG (s, "gain: open");
        dp_wfm_stream_sink_set_gain (s, cases[k][0]);
        DP_CHECK (dp_wfm_stream_sink_send (s, x, 32, 1e6, 2.4e9) == 0);
        DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), cases[k][1], 1e-6);
        /* the caller's own read of "did this clip": > 1.0 exactly when the
           gain pushed it there, and the 1.5 case proves the peak is not
           clamped to 1.0 on the way past */
        DP_CHECK ((dp_wfm_stream_sink_peak (s) > 1.0) == (cases[k][1] > 1.0));
        dp_wfm_stream_sink_close (s);
      }
  }

  /* ── §D  clip_fraction: opt-in, and an exact ratio when it is on ────────
   *
   * "Fraction (0..1) of integer I/Q components that saturated; 0 unless
   * tracked." Zero mentions. Both halves matter: a counter that ran anyway
   * would cost the streaming hot path, and one that reported a plausible
   * number rather than the true ratio would be worse than none.
   *
   * The denominator is COMPONENTS, two per complex sample, so a block whose
   * I saturates and whose Q does not is exactly one half -- a truth that
   * needs no reference implementation, only counting.
   */
  {
    float _Complex half[40], all[40], none[40];
    for (int i = 0; i < 40; i++)
      {
        half[i] = 1.5f + 0.5f * I; /* I clips, Q does not -> 1/2 */
        all[i]  = 1.5f - 2.0f * I; /* both clip            -> 1   */
        none[i] = 0.5f + 0.25f * I;
      }
    /* off by default: everything below saturates and the fraction stays 0 */
    wfm_stream_sink_t *s = open_sink (3);
    DP_REQUIRE_MSG (s, "clip: open");
    DP_CHECK (dp_wfm_stream_sink_send (s, all, 40, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (s), 0.0, 1e-12);
    DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 2.0, 1e-6); /* peak IS on */
    dp_wfm_stream_sink_close (s);

    const struct
    {
      const float _Complex *blk;
      double                want;
      const char           *what;
    } cases[] = {
      { none, 0.0, "nothing saturates" },
      { half, 0.5, "I saturates, Q does not" },
      { all, 1.0, "both components saturate" },
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++)
      {
        wfm_stream_sink_t *t = open_sink (3);
        DP_REQUIRE_MSG (t, "clip: open tracked");
        dp_wfm_stream_sink_track_clipping (t, 1);
        DP_CHECK (dp_wfm_stream_sink_send (t, cases[k].blk, 40, 1e6, 2.4e9)
                  == 0);
        DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (t), cases[k].want,
                       1e-12);
        dp_wfm_stream_sink_close (t);
      }
    /* mixed across sends: 40 clean samples then 40 fully-clipped ones is a
       quarter of all components, which no per-block reading would give */
    wfm_stream_sink_t *m = open_sink (3);
    DP_REQUIRE_MSG (m, "clip: open mixed");
    dp_wfm_stream_sink_track_clipping (m, 1);
    DP_CHECK (dp_wfm_stream_sink_send (m, none, 40, 1e6, 2.4e9) == 0);
    DP_CHECK (dp_wfm_stream_sink_send (m, half, 40, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (m), 0.25, 1e-12);
    /* and it can be turned back OFF mid-stream */
    dp_wfm_stream_sink_track_clipping (m, 0);
    DP_CHECK (dp_wfm_stream_sink_send (m, all, 40, 1e6, 2.4e9) == 0);
    DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (m), 40.0 / 240.0, 1e-12);
    dp_wfm_stream_sink_close (m);
    DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (NULL), 0.0, 1e-12);
  }

  /* ── §E  the cf32 path is left untouched, deliberately ──────────────────
   *
   * The header says so in as many words: cf32 "never clips and is the
   * streaming hot path", so neither the peak nor the counter runs on it.
   * Worth pinning because it reads like a bug from the outside -- a caller
   * who streams cf32 and asks for the peak gets 0.0, and should be told that
   * is the design rather than discovering it against a live stream.
   */
  {
    float _Complex x[32];
    for (int i = 0; i < 32; i++)
      x[i] = 3.0f + 3.0f * I;    /* far past full scale, if anything tracked */
    for (int t = 0; t <= 1; t++) /* 0 cf32, 1 cf64 -- both float paths */
      {
        wfm_stream_sink_t *s = open_sink (t);
        DP_REQUIRE_MSG (s, "cf32: open");
        dp_wfm_stream_sink_track_clipping (s, 1);
        DP_CHECK (dp_wfm_stream_sink_send (s, x, 32, 1e6, 2.4e9) == 0);
        DP_CHECK_NEAR (dp_wfm_stream_sink_peak (s), 0.0, 1e-12);
        DP_CHECK_NEAR (dp_wfm_stream_sink_clip_fraction (s), 0.0, 1e-12);
        dp_wfm_stream_sink_close (s);
      }
  }

  /* ── §F  send_eos, and the ordering the header insists on ───────────────
   *
   * "Publishes an end-of-stream frame, so a consumer learns the sender
   * finished rather than inferring it from silence. Send it BEFORE draining:
   * a drain cannot be reversed and refuses sends once it reaches its
   * publish-flushing phase." Zero mentions -- neither the call nor the rule.
   */
  {
    wfm_stream_sink_t *s = open_sink (3);
    DP_REQUIRE_MSG (s, "eos: open");
    float _Complex x[16];
    for (int i = 0; i < 16; i++)
      x[i] = 0.5f + 0.0f * I;
    DP_CHECK (dp_wfm_stream_sink_send (s, x, 16, 1e6, 2.4e9) == 0);
    DP_CHECK_MSG (dp_wfm_stream_sink_send_eos (s) == DP_OK,
                  "eos in the documented order (before the drain)");
    DP_CHECK_MSG (dp_wfm_stream_sink_drain (s, 2000) == DP_OK,
                  "drain returns once the client has flushed");
    dp_wfm_stream_sink_close (s);
    /* NULL is vacuously fine on both, so a cleanup path need not guard */
    DP_CHECK (dp_wfm_stream_sink_send_eos (NULL) == DP_OK);
    DP_CHECK (dp_wfm_stream_sink_drain (NULL, 100) == DP_OK);
  }

  /* DP_TEST_END, not `return 0`: DP_CHECK records a failure and continues,
   * and only this macro reads the counter. The sections above are the
   * file's first DP_CHECKs -- until it was added, every one of them could
   * have failed with the process still exiting 0. It also refuses a run
   * that asserted nothing at all. */
  DP_TEST_END ("test_wfm_sink");
}
