// MIT-licensed C wrapper around pocketfft (BSD).
// This file is compiled as C++ but exposes a pure C API.

#include "pocketfft/pocketfft.h"
#include "pocketfft/pocketfft_hdronly.h"
#include <complex>
#include <vector>

extern "C"
{

  struct pocketfft_plan
  {
    int sign;
    size_t n, ny, nx;
    bool is2d;
  };

  /* -------------------------
   * 1D plan
   * ------------------------- */
  pocketfft_plan *
  pocketfft_plan_1d (size_t n, int sign)
  {
    pocketfft_plan *p = new pocketfft_plan;
    p->n = n;
    p->ny = p->nx = 0;
    p->sign = sign;
    p->is2d = false;
    return p;
  }

  /* -------------------------
   * 2D plan
   * ------------------------- */
  pocketfft_plan *
  pocketfft_plan_2d (size_t ny, size_t nx, int sign)
  {
    pocketfft_plan *p = new pocketfft_plan;
    p->ny = ny;
    p->nx = nx;
    p->n = 0;
    p->sign = sign;
    p->is2d = true;
    return p;
  }

  /* -------------------------
   * Execute 1D
   * ------------------------- */
  void
  pocketfft_execute_1d (pocketfft_plan *p, const void *in, void *out)
  {
    using namespace pocketfft;

    shape_t shape = { p->n };
    stride_t stride = { sizeof (std::complex<double>) };
    shape_t axes = { 0 };

    c2c (shape, stride, stride, axes, p->sign < 0,
         reinterpret_cast<const std::complex<double> *> (in),
         reinterpret_cast<std::complex<double> *> (out), 1.0);
  }

  /* -------------------------
   * Execute 2D
   * ------------------------- */
  void
  pocketfft_execute_2d (pocketfft_plan *p, const void *in, void *out)
  {
    using namespace pocketfft;

    shape_t shape = { p->ny, p->nx };
    stride_t stride
        = { (std::ptrdiff_t)(sizeof (std::complex<double>) * p->nx),
            (std::ptrdiff_t)sizeof (std::complex<double>) };
    shape_t axes = { 0, 1 };

    c2c (shape, stride, stride, axes, p->sign < 0,
         reinterpret_cast<const std::complex<double> *> (in),
         reinterpret_cast<std::complex<double> *> (out), 1.0);
  }

  /* -------------------------
   * Execute 1D (float)
   * ------------------------- */
  void
  pocketfft_execute_1d_cf32 (pocketfft_plan *p, const void *in, void *out)
  {
    using namespace pocketfft;

    shape_t shape = { p->n };
    stride_t stride = { sizeof (std::complex<float>) };
    shape_t axes = { 0 };

    c2c (shape, stride, stride, axes, p->sign < 0,
         reinterpret_cast<const std::complex<float> *> (in),
         reinterpret_cast<std::complex<float> *> (out), 1.0f);
  }

  /* -------------------------
   * Execute 2D (float)
   * ------------------------- */
  void
  pocketfft_execute_2d_cf32 (pocketfft_plan *p, const void *in, void *out)
  {
    using namespace pocketfft;

    shape_t shape = { p->ny, p->nx };
    stride_t stride = { (std::ptrdiff_t)(sizeof (std::complex<float>) * p->nx),
                        (std::ptrdiff_t)sizeof (std::complex<float>) };
    shape_t axes = { 0, 1 };

    c2c (shape, stride, stride, axes, p->sign < 0,
         reinterpret_cast<const std::complex<float> *> (in),
         reinterpret_cast<std::complex<float> *> (out), 1.0f);
  }

  /* -------------------------
   * Destroy
   * ------------------------- */
  void
  pocketfft_destroy_plan (pocketfft_plan *p)
  {
    delete p;
  }

} // extern "C"
