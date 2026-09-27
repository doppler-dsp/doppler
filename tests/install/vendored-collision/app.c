/*
 * A consumer that brings its OWN cJSON and nats.c must still link doppler
 * statically, and doppler must keep using its own copies (#1565).
 *
 * libdoppler.a embeds cJSON and libdoppler_stream.a embeds nats.c. Their
 * symbols used to be exported under their own names, so defining
 * cJSON_Parse here was a duplicate definition at this program's link -- or,
 * worse, doppler silently called this stub instead of its own parser. The
 * archives now carry them as dp__v_*, so this links and both halves hold:
 *
 *   exit 0  doppler parsed the scene with its own cJSON, ours was untouched
 *   exit 1  doppler could not parse a scene it documents as valid
 *   exit 2  doppler called OUR cJSON_Parse -- its vendored copy leaked
 */
#include <doppler/stream/stream.h>
#include <doppler/wfm/wfm_compose.h>
#include <stdio.h>

typedef struct cJSON            cJSON;
typedef struct __natsConnection natsConnection;

static int ours_called;

/* The consumer's own cJSON and nats.c entry points, same names. */
cJSON *
cJSON_Parse (const char *value)
{
  (void)value;
  ours_called = 1;
  return NULL;
}

void
natsConnection_Destroy (natsConnection *nc)
{
  (void)nc;
  ours_called = 1;
}

int
main (void)
{
  /* docs/design/wfmgen-composition.md's scene, shortened. */
  const char *scene = "{\"segments\":[{\"n\":1000,\"sum\":["
                      "{\"type\":\"qpsk\",\"freq\":0,\"sps\":8,\"snr\":15,"
                      "\"snr_mode\":\"esno\"},"
                      "{\"type\":\"tone\",\"freq\":200000,\"level\":-12}]}],"
                      "\"headroom\":6}";

  /* Referencing the stream API pulls nats.c's members into this link, so a
     leaked nats symbol would collide here even though nothing connects. */
  volatile void *pull_in = (void *)dp_pub_destroy;
  (void)pull_in;

  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (scene);
  if (ours_called)
    {
      fprintf (stderr, "vendored-collision: doppler called the consumer's "
                       "cJSON_Parse -- its vendored copy is not private\n");
      return 2;
    }
  if (!c)
    {
      fprintf (stderr, "vendored-collision: dp_wfm_compose_from_json "
                       "refused a documented scene\n");
      return 1;
    }
  dp_wfm_compose_destroy (c);
  puts ("vendored-collision: OK -- doppler used its own cJSON; "
        "the consumer's cJSON and nats.c linked alongside");
  return 0;
}
