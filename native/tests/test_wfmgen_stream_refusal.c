/**
 * @file test_wfmgen_stream_refusal.c
 * @brief wfmgen exits 1 when the stream refuses a block (doppler#2011).
 *
 * A refused send is a block that never left the process: during a
 * reconnect the NATS client stops taking publishes once 8 MiB is pending.
 * wfmgen ignored dp_wfm_stream_sink_send()'s status and exited 0, so "wfmgen
 * exited 0" did not mean the samples arrived. A broker cannot refuse on
 * cue, so this replaces the sink: libdoppler carries WEAK no-op stubs for
 * every dp_wfm_stream_sink_* symbol (wfm_sink_stub.c), and the strong
 * definitions below win the link. The same run is made three times:
 * against a sink that takes everything, one that refuses every block, and
 * one that takes every block but refuses the end-of-stream (a subscriber
 * then sees silence, not an end). The exit code follows the refusal and
 * nothing else.
 */

#include "doppler/wfm/wfm_sink.h"
#include "doppler/wfm/wfmgen.h"
#include "dp_test.h"

static int refuse;     /* refuse every block */
static int refuse_eos; /* refuse the end-of-stream */
static int sends;      /* blocks offered, refused or not */

static struct wfm_stream_sink *const SINK = (struct wfm_stream_sink *)&refuse;

int
dp_wfm_stream_sink_available (void)
{
  return 1;
}

wfm_stream_sink_t *
dp_wfm_stream_sink_open (const char *endpoint, int sample_type)
{
  (void)endpoint;
  (void)sample_type;
  return SINK; /* never dereferenced: every sink call is defined here */
}

int
dp_wfm_stream_sink_send (wfm_stream_sink_t *sink, const float _Complex *iq,
                         size_t n, double fs, double fc)
{
  (void)sink;
  (void)iq;
  (void)n;
  (void)fs;
  (void)fc;
  sends++;
  return refuse ? DP_ERR_SEND : DP_OK;
}

int
dp_wfm_stream_sink_send_eos (wfm_stream_sink_t *sink)
{
  (void)sink;
  return refuse_eos ? DP_ERR_SEND : DP_OK;
}

int
dp_wfm_stream_sink_drain (wfm_stream_sink_t *sink, int timeout_ms)
{
  (void)sink;
  (void)timeout_ms;
  return DP_OK;
}

void
dp_wfm_stream_sink_close (wfm_stream_sink_t *sink)
{
  (void)sink;
}

void
dp_wfm_stream_sink_track_clipping (wfm_stream_sink_t *sink, int on)
{
  (void)sink;
  (void)on;
}

void
dp_wfm_stream_sink_set_gain (wfm_stream_sink_t *sink, double gain)
{
  (void)sink;
  (void)gain;
}

double
dp_wfm_stream_sink_peak (const wfm_stream_sink_t *sink)
{
  (void)sink;
  return 0.0;
}

double
dp_wfm_stream_sink_clip_fraction (const wfm_stream_sink_t *sink)
{
  (void)sink;
  return 0.0;
}

int
main (void)
{
  char *av[] = { "wfmgen",
                 "--type",
                 "tone",
                 "--count",
                 "8192",
                 "--output",
                 "nats://127.0.0.1:1/never-dialled",
                 NULL };

  refuse = 0;
  sends  = 0;
  DP_CHECK_MSG (dp_doppler_wfmgen (7, av) == 0,
                "every block taken: wfmgen exits 0");
  DP_CHECK_MSG (sends > 0, "the stream path was the one exercised");

  refuse = 1;
  sends  = 0;
  DP_CHECK_MSG (dp_doppler_wfmgen (7, av) == 1,
                "a refused block makes wfmgen exit 1, not 0");
  DP_CHECK (sends > 0);

  refuse     = 0;
  refuse_eos = 1;
  DP_CHECK_MSG (dp_doppler_wfmgen (7, av) == 1,
                "a refused end-of-stream makes wfmgen exit 1, not 0");

  DP_TEST_END ("test_wfmgen_stream_refusal");
}
