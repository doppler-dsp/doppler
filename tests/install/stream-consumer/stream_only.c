/* stream_only.c — a consumer that uses ONLY the stream API (dp_pub_*): no
   core symbol, so a linker that drops what is not referenced (gcc on Debian
   and Ubuntu defaults to --as-needed) leaves libdoppler out of this binary's
   own dependencies. libdoppler_stream then has to find libdoppler itself,
   which is the case build-three-ways.sh's fourth and fifth builds exist for.
 */
#include <stdio.h>

#include <doppler/stream/stream.h>

int
main (void)
{
  dp_pub_t *tx = dp_pub_create ("nats://127.0.0.1:4222/smoke", CF64);
  printf ("stream: %s\n", tx ? "connected" : "linked, no broker");
  if (tx)
    dp_pub_destroy (tx);
  return 0;
}
