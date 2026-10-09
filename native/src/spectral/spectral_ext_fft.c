/* jm:generated spectral_ext_fft.c */
/*
 * spectral_ext_fft.c — FFT type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_fft_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFTObject — wraps dp_fft_state_t *       */
/* ======================================================== */

#include "doppler/fft/fft_core.h"

typedef struct
{
  PyObject_HEAD dp_fft_state_t *handle;
} FFTObject;

static void
FFTObj_dealloc (FFTObject *self)
{
  if (self->handle)
    dp_fft_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFTObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  FFTObject *self = (FFTObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFTObj_init (FFTObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "n", "sign", "nthreads", NULL };
  unsigned long long n_raw    = 1024;
  int                sign     = -1;
  int                nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kii", kwlist, &n_raw, &sign,
                                    &nthreads))
    return -1;
  size_t n     = (size_t)n_raw;
  self->handle = dp_fft_create (n, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_fft_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
FFTObj_reset (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_fft_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FFTObj_execute_cf64_max_out (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_fft_execute_cf64_max_out (self->handle));
}

static PyObject *
FFTObj_execute_cf64 (FFTObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX128
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX128,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_fft_execute_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = dp_fft_execute_cf64 (
          self->handle, (const double _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (double _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not the plan length (FFT.n); "
                           "execute takes exactly one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "FFT.execute_cf64: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_fft_execute_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "FFT.execute_cf64: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double _Complex *_d0
      = (double _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_fft_execute_cf64 (
      self->handle, (const double _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not the plan length (FFT.n); execute "
                       "takes exactly one frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "FFT.execute_cf64: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
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
FFTObj_execute_cf32_max_out (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_fft_execute_cf32_max_out (self->handle));
}

static PyObject *
FFTObj_execute_cf32 (FFTObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_fft_execute_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = dp_fft_execute_cf32 (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not the plan length (FFT.n); "
                           "execute takes exactly one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "FFT.execute_cf32: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_fft_execute_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "FFT.execute_cf32: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = dp_fft_execute_cf32 (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not the plan length (FFT.n); execute "
                       "takes exactly one frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "FFT.execute_cf32: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
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
FFTObj_execute_inplace_cf64_max_out (FFTObject *self,
                                     PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_fft_execute_inplace_cf64_max_out (self->handle));
}

static PyObject *
FFTObj_execute_inplace_cf64 (FFTObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX128
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX128,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_fft_execute_inplace_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = dp_fft_execute_inplace_cf64 (
          self->handle, (const double _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (double _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not the plan length (FFT.n); "
                           "execute takes exactly one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "FFT.execute_inplace_cf64: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_fft_execute_inplace_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "FFT.execute_inplace_cf64: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double _Complex *_d0
      = (double _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = dp_fft_execute_inplace_cf64 (
      self->handle, (const double _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not the plan length (FFT.n); execute "
                       "takes exactly one frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "FFT.execute_inplace_cf64: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
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
FFTObj_execute_inplace_cf32_max_out (FFTObject *self,
                                     PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_fft_execute_inplace_cf32_max_out (self->handle));
}

static PyObject *
FFTObj_execute_inplace_cf32 (FFTObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_fft_execute_inplace_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = dp_fft_execute_inplace_cf32 (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not the plan length (FFT.n); "
                           "execute takes exactly one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "FFT.execute_inplace_cf32: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_fft_execute_inplace_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "FFT.execute_inplace_cf32: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = dp_fft_execute_inplace_cf32 (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not the plan length (FFT.n); execute "
                       "takes exactly one frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "FFT.execute_inplace_cf32: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
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
FFT_getprop_n (FFTObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
FFT_getprop_sign (FFTObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->sign);
}

static PyGetSetDef FFT_getset[] = { { "n", (getter)FFT_getprop_n, NULL,
                                      "Transform length (samples).\n", NULL },
                                    { "sign", (getter)FFT_getprop_sign, NULL,
                                      "-1 forward, +1 inverse.\n", NULL },
                                    { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
FFTObj_destroy (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFTObj_enter (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFTObj_exit (FFTObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFTObj_methods[] = {
  { "reset", (PyCFunction)FFTObj_reset, METH_NOARGS,
    "No-op reset (plans are immutable after creation).\n" },

  { "execute_cf64", (PyCFunction)(void *)FFTObj_execute_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf64(x, out) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a double-precision complex input.\n"
    "The output is written to a fresh caller-supplied buffer; in and out\n"
    "must not alias. The transform is unnormalised: the inverse DFT\n"
    "(sign=+1) does NOT divide by n. Both buffers must be exactly state->n\n"
    "elements long.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex128]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex128] | None\n"
    "    Output buffer of length >= state->n (CF64, caller-allocated).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex128]\n"
    "    min(state->n, max_out) bins. If n_in is not the required length the\n"
    "    call is REFUSED with nothing read, written or counted: SIZE_MAX in\n"
    "    C, ValueError in Python (#1925).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not the plan\n"
    "    length (FFT.n); execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> import numpy as np\n"
    ">>> fft = FFT(n=4, sign=-1)\n"
    ">>> x = np.array([1, 0, 0, 0], dtype=np.complex128)\n"
    ">>> fft.execute_cf64(x).tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n" },
  { "execute_cf64_max_out", (PyCFunction)FFTObj_execute_cf64_max_out,
    METH_NOARGS,
    "execute_cf64_max_out() -> int\n"
    "\n"
    "Maximum output samples per execute call (always == n).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_cf32", (PyCFunction)(void *)FFTObj_execute_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf32(x, out) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a single-precision complex input.\n"
    "Identical to dp_fft_execute_cf64() but operates on float _Complex\n"
    "(CF32) buffers, halving memory bandwidth relative to the\n"
    "double-precision variant. Output is unnormalised; in and out must not\n"
    "alias.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer of length >= state->n (CF32, caller-allocated).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    min(state->n, max_out) bins. If n_in is not the required length the\n"
    "    call is REFUSED with nothing read, written or counted: SIZE_MAX in\n"
    "    C, ValueError in Python (#1925).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not the plan\n"
    "    length (FFT.n); execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> import numpy as np\n"
    ">>> fft = FFT(n=4, sign=-1)\n"
    ">>> x = np.ones(4, dtype=np.complex64)\n"
    ">>> fft.execute_cf32(x).tolist()\n"
    "[(4+0j), 0j, 0j, 0j]\n" },
  { "execute_cf32_max_out", (PyCFunction)FFTObj_execute_cf32_max_out,
    METH_NOARGS,
    "execute_cf32_max_out() -> int\n"
    "\n"
    "Maximum output samples for CF32 execute (always == n).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_inplace_cf64", (PyCFunction)(void *)FFTObj_execute_inplace_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf64(x, out) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF64). The copy step\n"
    "lets callers preserve their input while keeping the output buffer hot\n"
    "in cache. Semantically identical to dp_fft_execute_cf64() for separate\n"
    "in / out pointers; use this variant when the caller already owns out\n"
    "and wants the result there without a second allocation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex128]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex128] | None\n"
    "    Destination buffer, length >= state->n; must not alias in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex128]\n"
    "    min(state->n, max_out) bins. If n_in is not the required length the\n"
    "    call is REFUSED with nothing read, written or counted: SIZE_MAX in\n"
    "    C, ValueError in Python (#1925).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not the plan\n"
    "    length (FFT.n); execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> import numpy as np\n"
    ">>> fft = FFT(n=4, sign=-1)\n"
    ">>> x = np.array([1, 0, 0, 0], dtype=np.complex128)\n"
    ">>> fft.execute_inplace_cf64(x).tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n" },
  { "execute_inplace_cf64_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf64_max_out, METH_NOARGS,
    "execute_inplace_cf64_max_out() -> int\n"
    "\n"
    "Maximum output samples for inplace CF64 (always == n).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_inplace_cf32", (PyCFunction)(void *)FFTObj_execute_inplace_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf32(x, out) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF32).\n"
    "Single-precision variant of dp_fft_execute_inplace_cf64(). Copies\n"
    "state->n CF32 samples from in to out, then transforms out with the CF32\n"
    "pocketfft plan. in is left unmodified.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Destination buffer, length >= state->n; must not alias in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    min(state->n, max_out) bins. If n_in is not the required length the\n"
    "    call is REFUSED with nothing read, written or counted: SIZE_MAX in\n"
    "    C, ValueError in Python (#1925).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not the plan\n"
    "    length (FFT.n); execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> import numpy as np\n"
    ">>> fft = FFT(n=4, sign=-1)\n"
    ">>> x = np.array([1, 0, 0, 0], dtype=np.complex64)\n"
    ">>> fft.execute_inplace_cf32(x).tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n" },
  { "execute_inplace_cf32_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf32_max_out, METH_NOARGS,
    "execute_inplace_cf32_max_out() -> int\n"
    "\n"
    "Maximum output samples for inplace CF32 (always == n).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_ci16", (PyCFunction)(void (*) (void))FFTObj_execute_ci16,
    METH_VARARGS,
    "Out-of-place 1-D FFT directly on interleaved int16 I/Q (CF32 out).\n"
    "\n"
    "The int16->float convert (v/32768, full-scale +/-1.0) is fused into the\n"
    "transform, so it is faster than i16_to_f32 then execute_cf32.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``iq`` does not hold exactly ``2 * FFT.n`` values, two to a\n"
    "    complex sample; an odd count is not whole I/Q pairs and is\n"
    "    refused too (#1933). The message names the length it got,\n"
    "    ``execute_ci16 takes exactly one frame: 2 * FFT.n = <2n>\n"
    "    interleaved I/Q values, got <len>``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> obj = FFT(1024, -1, 1)\n"
    ">>> y = obj.execute_ci16(np.zeros(2048, dtype=np.int16))\n"
    ">>> y.dtype\n"
    "dtype('complex64')\n" },
  { "execute_ci8", (PyCFunction)(void (*) (void))FFTObj_execute_ci8,
    METH_VARARGS,
    "Out-of-place 1-D FFT directly on interleaved int8 I/Q (CF32 out).\n"
    "\n"
    "As execute_ci16 but int8 input (v/128, full-scale +/-1.0).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``iq`` does not hold exactly ``2 * FFT.n`` values, two to a\n"
    "    complex sample; an odd count is not whole I/Q pairs and is\n"
    "    refused too (#1933). The message names the length it got,\n"
    "    ``execute_ci8 takes exactly one frame: 2 * FFT.n = <2n>\n"
    "    interleaved I/Q values, got <len>``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> obj = FFT(1024, -1, 1)\n"
    ">>> y = obj.execute_ci8(np.zeros(2048, dtype=np.int8))\n"
    ">>> y.dtype\n"
    "dtype('complex64')\n" },
  { "destroy", (PyCFunction)FFTObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)FFTObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a FFT be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FFT\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)FFTObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the FFT.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject FFTObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.FFT",
  .tp_basicsize                           = sizeof (FFTObject),
  .tp_dealloc                             = (destructor)FFTObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a reusable 1-D FFT engine for a fixed length and sign. Two\n"
    "pocketfft plans are created at construction time — one for CF64 and one "
    "for\n"
    "CF32 — so execute calls carry no plan-setup overhead. The same instance "
    "may\n"
    "be called repeatedly for independent input vectors of the same length.\n"
    "nthreads is accepted for API parity but is ignored; pocketfft plans are\n"
    "single-threaded.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int, default 1024\n"
    "    Transform length in samples (power of two recommended).\n"
    "sign : int, default -1\n"
    "    -1 for the forward DFT, +1 for the inverse DFT.\n"
    "nthreads : int, default 1\n"
    "    Accepted for API compatibility; ignored.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT\n"
    ">>> import numpy as np\n"
    ">>> fft = FFT(n=4, sign=-1, nthreads=1)\n"
    ">>> fft.n, fft.sign\n"
    "(4, -1)\n"
    ">>> x = np.array([1, 0, 0, 0], dtype=np.complex64)\n"
    ">>> fft.execute_cf32(x).tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n",
  .tp_methods = FFTObj_methods,
  .tp_getset  = FFT_getset,
  .tp_new     = FFTObj_new,
  .tp_init    = (initproc)FFTObj_init,
};
