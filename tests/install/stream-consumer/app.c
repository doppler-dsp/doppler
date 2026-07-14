/* app.c — the "three faces" consumer: the quickstart FFT example plus
   one streaming call, so a single binary proves the full two-component
   link line (libdoppler + libdoppler_stream).  stdout is deterministic
   for a given environment, which lets build-three-ways.sh assert that
   the cc, CMake, and pkg-config builds behave identically.  Docs
   include this file via --8<-- so the shown consumer IS the tested
   consumer. */
// --8<-- [start:app]
#include <complex.h>
#include <stdio.h>

#include <fft/fft_core.h>  /* core:   libdoppler          */
#include <stream/stream.h> /* stream: libdoppler_stream   */

int
main (void)
{
  /* core — the homepage FFT example, checksummed */
  float complex in[1024] = { 0 };
  float complex out[1024];
  for (int i = 0; i < 1024; i++)
    in[i] = (i % 8 == 0) ? 1.0f : 0.0f; /* impulse train */

  fft_state_t *fft = fft_create (1024, -1, 1);
  fft_execute_cf32 (fft, in, 1024, out);
  fft_destroy (fft);

  double acc = 0.0;
  for (int i = 0; i < 1024; i++)
    acc += cabsf (out[i]);
  printf ("fft checksum: %.1f\n", acc);

  /* stream — dp_pub_* lives in libdoppler_stream; linking this call is
     the point of the exercise.  A live broker is optional here. */
  dp_pub_t *tx = dp_pub_create ("nats://127.0.0.1:4222/smoke", CF64);
  printf ("stream: %s\n", tx ? "connected" : "linked, no broker");
  if (tx)
    dp_pub_destroy (tx);
  return 0;
}
// --8<-- [end:app]
