/*
 * spectral_ext_fft_extra.c — FFT.execute_ci16() / execute_ci8(), hand-written.
 *
 * The integer-IQ executes fold the int->float convert into the FFT's input
 * read, a fused dtype-convert-on-read shape jm's method rows do not render.
 * objects/fft.toml's [[fft.extra_methods]] rows register the two wrappers:
 * jm owns each PyMethodDef entry, its forward prototype, the stub and the
 * #include of this file (after the generated FFTObject), and never touches
 * this file (doppler#1886). The wrappers take self as PyObject *, as CPython
 * calls every method; the shared body and its refusal helper are internal
 * and keep FFTObject *. The odd-count refusal (#1933) lives here with them.
 */

#include "doppler/fft/fft_core.h"

/* What execute_ci16/ci8 say instead when handed a str (jm_array_arg_hint):
 * text is refused rather than read by numpy as a number, as a manifest
 * str_hint does for a generated binding (gh-1824). */
#define FFT_IQ_HINT "pass interleaved I/Q as an array of integers"

/* The one refusal message for the interleaved-integer path, whichever check
 * caught it.  It counts in the caller's unit -- int values, two to a complex
 * sample -- because "not FFT.n" is wrong by a factor of two for an array
 * that must hold 2 * FFT.n of them. */
static PyObject *
fft_int_length_error (FFTObject *self, int is8, Py_ssize_t n_vals)
{
  PyErr_Format (PyExc_ValueError,
                "execute_%s takes exactly one frame: 2 * FFT.n = %zu "
                "interleaved I/Q values, got %zd",
                is8 ? "ci8" : "ci16", 2 * self->handle->n, n_vals);
  return NULL;
}

/* Integer-IQ executes (ci16/ci8): interleaved int16/int8 I/Q in, CF32 out.
 * The int->float convert is folded into the FFT input read (no separate cvt
 * pass).  Hand-written: not manifest-declared (jm has no params shape for a
 * fused dtype-convert-on-read execute), so it must be re-added by hand after
 * any delete-and-regenerate of this fragment -- see
 * docs/dev/contributing/adding-a-module.md. The result is NumPy-owned (a fresh
 * array per call), matching the generated siblings above; the old
 * view-onto-a-reused-buffer form was the gh-219 UAF. */
static PyObject *
FFTObj_execute_int (FFTObject *self, PyObject *args, int is8)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg_hint (in_obj, is8 ? NPY_INT8 : NPY_INT16,
                           NPY_ARRAY_C_CONTIGUOUS, "iq", FFT_IQ_HINT);
  if (!in_arr)
    return NULL;
  /* interleaved I/Q: 2 ints per complex sample.  An odd count is not
   * whole pairs, and halving it would round 2n+1 down to a valid frame and
   * silently drop the last value (#1933).  The kernel counts complex
   * samples, so it cannot see this; the binding refuses it here. */
  Py_ssize_t n_vals = PyArray_SIZE (in_arr);
  if (n_vals % 2)
    {
      Py_DECREF (in_arr);
      return fft_int_length_error (self, is8, n_vals);
    }
  Py_ssize_t n     = n_vals / 2;
  size_t     _need = (size_t)n;
  size_t     _cap  = dp_fft_execute_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out
      = is8 ? dp_fft_execute_ci8 (self->handle,
                                  (const int8_t *)PyArray_DATA (in_arr),
                                  (size_t)n, _d0)
            : dp_fft_execute_ci16 (self->handle,
                                   (const int16_t *)PyArray_DATA (in_arr),
                                   (size_t)n, _d0);
  if (n_out == SIZE_MAX)
    {
      /* n_in != n: the kernel refused (#1925), nothing was written. */
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      return fft_int_length_error (self, is8, n_vals);
    }
  Py_DECREF (in_arr);
  if ((size_t)n_out == _cap)
    return arr0;
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
FFTObj_execute_ci16 (PyObject *obj, PyObject *args)
{
  FFTObject *self = (FFTObject *)obj;
  return FFTObj_execute_int (self, args, 0);
}

static PyObject *
FFTObj_execute_ci8 (PyObject *obj, PyObject *args)
{
  FFTObject *self = (FFTObject *)obj;
  return FFTObj_execute_int (self, args, 1);
}
