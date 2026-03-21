// doppler.c
// Top-level doppler DSP engine entry points.
// This file defines the public C API for the native library.
// The FFT subsystem is implemented in fft.c and exposed via fft.h.

#include <dp/core.h>
#include <dp/fft.h>

static int DP_INITIALIZED = 0;

int
dp_init (void)
{
  if (DP_INITIALIZED)
    return 0;

  // Nothing else to initialize yet.
  // FFT setup is performed lazily via fft_global_setup().
  DP_INITIALIZED = 1;
  return 0;
}

void
dp_cleanup (void)
{
  if (!DP_INITIALIZED)
    return;

  // FFTW cleanup is handled internally by FFTW.
  DP_INITIALIZED = 0;
}

const char *
dp_version (void)
{
  return "0.1.0";
}

const char *
dp_build_info (void)
{
  return "doppler DSP Engine — native C library with Python bindings";
}
