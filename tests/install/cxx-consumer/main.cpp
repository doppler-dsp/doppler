// main.cpp -- a C++11 consumer of the doppler C library (doppler#3108).
//
// Includes the public headers directly, with no `extern "C"` wrapper and no
// C shim, and makes the calls a C++ program actually makes: its samples
// live in std::vector<std::complex<float>> and cross to doppler by pointer
// (std::complex<T> is layout-compatible with T _Complex), and one complex
// value crosses BY VALUE, in and out, through dp_square_clip(). Two of the
// calls are header-only inlines in C that C++ gets as prototypes, so the
// comparison is also the C inline against the library's exported copy:
// dp_square_clip() and dp_cic_decimate().
//
// It prints every output sample as hexadecimal floats. main.c beside it is
// the same program in C; `make cxx-headers-check` builds both against the
// same libdoppler.a and requires the two outputs to be identical, byte for
// byte. A header that stops parsing fails the build; a prototype that
// parses but means something else in C++ fails the comparison.
#include <complex>
#include <cstdio>
#include <vector>

#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/lo/lo_core.h"
#include "doppler/util/util_core.h"

typedef std::complex<float> cf32;

int
main ()
{
  const std::size_t n = 64;
  const float taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
  std::vector<cf32> x (n), y (n), d (n);

  dp_lo_state_t *lo = dp_lo_create (0.0625);
  dp_fir_state_t *fir = dp_fir_create_real (taps, 5);
  dp_cic_state_t *cic = dp_cic_create (4);
  if (!lo || !fir || !cic)
    return 2;

  if (dp_lo_steps (lo, n, reinterpret_cast<float _Complex *> (x.data ()), n)
      != n)
    return 3;
  if (dp_fir_execute (fir,
                      reinterpret_cast<const float _Complex *> (x.data ()),
                      n, reinterpret_cast<float _Complex *> (y.data ()))
      != n)
    return 4;
  // dp_cic_decimate is header-only to C; from C++ it is a prototype, and
  // this call links the library's out-of-line copy of the same body.
  std::size_t nd
      = dp_cic_decimate (cic, reinterpret_cast<const float _Complex *> (
                             y.data ()),
                         n, reinterpret_cast<float _Complex *> (d.data ()), n);
  if (nd != n / 4)
    return 5;

  for (std::size_t i = 0; i < n; i++)
    {
      // By value, both ways: the argument and the result are C's own
      // `float _Complex`, read back here with the GNU accessors.
      float _Complex z
          = dp_square_clip (*reinterpret_cast<float _Complex *> (&y[i]),
                            0.5f);
      std::printf ("%a %a %a %a\n", (double)y[i].real (),
                   (double)y[i].imag (), (double)__real__ z,
                   (double)__imag__ z);
    }

  for (std::size_t i = 0; i < nd; i++)
    std::printf ("%a %a\n", (double)d[i].real (), (double)d[i].imag ());

  dp_cic_destroy (cic);
  dp_fir_destroy (fir);
  dp_lo_destroy (lo);
  return 0;
}
