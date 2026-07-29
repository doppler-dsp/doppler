/*
 * spectral_ext_fft.c — FFT type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFTObject — wraps fft_state_t *       */
/* ======================================================== */

#include "fft/fft_core.h"

typedef struct
{
  PyObject_HEAD fft_state_t *handle;
} FFTObject;

static void
FFTObj_dealloc (FFTObject *self)
{
  if (self->handle)
    fft_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFTObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
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
  self->handle = fft_create (n, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft_create returned NULL");
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
  fft_reset (self->handle);
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
  return PyLong_FromSize_t (fft_execute_cf64_max_out (self->handle));
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft_execute_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_cf64 (
          self->handle, (const double complex *)PyArray_DATA (in_arr),
          (size_t)n, (double complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = fft_execute_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double complex *_d0 = (double complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = fft_execute_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
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
  return PyLong_FromSize_t (fft_execute_cf32_max_out (self->handle));
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft_execute_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_cf32 (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = fft_execute_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = fft_execute_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
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

/* Integer-IQ executes (ci16/ci8): interleaved int16/int8 I/Q in, CF32 out.
 * The int->float convert is folded into the FFT input read (no separate cvt
 * pass).  Hand-written: not manifest-declared (jm has no params shape for a
 * fused dtype-convert-on-read execute), so it must be re-added by hand after
 * any delete-and-regenerate of this fragment -- see
 * docs/dev/adding-a-module.md. The result is NumPy-owned (a fresh array per
 * call), matching the generated siblings above; the old
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, is8 ? NPY_INT8 : NPY_INT16, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  /* interleaved I/Q: 2 ints per complex sample */
  Py_ssize_t n     = PyArray_SIZE (in_arr) / 2;
  size_t     _need = (size_t)n;
  size_t     _cap  = fft_execute_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out
      = is8 ? fft_execute_ci8 (self->handle,
                               (const int8_t *)PyArray_DATA (in_arr),
                               (size_t)n, _d0)
            : fft_execute_ci16 (self->handle,
                                (const int16_t *)PyArray_DATA (in_arr),
                                (size_t)n, _d0);
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
FFTObj_execute_ci16 (FFTObject *self, PyObject *args)
{
  return FFTObj_execute_int (self, args, 0);
}

static PyObject *
FFTObj_execute_ci8 (FFTObject *self, PyObject *args)
{
  return FFTObj_execute_int (self, args, 1);
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
  return PyLong_FromSize_t (fft_execute_inplace_cf64_max_out (self->handle));
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft_execute_inplace_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_inplace_cf64 (
          self->handle, (const double complex *)PyArray_DATA (in_arr),
          (size_t)n, (double complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = fft_execute_inplace_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double complex *_d0 = (double complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = fft_execute_inplace_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
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
  return PyLong_FromSize_t (fft_execute_inplace_cf32_max_out (self->handle));
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = fft_execute_inplace_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft_execute_inplace_cf32 (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = fft_execute_inplace_cf32_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = fft_execute_inplace_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
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

static PyGetSetDef FFT_getset[]
    = { { "n", (getter)FFT_getprop_n, NULL, "N.\n", NULL },
        { "sign", (getter)FFT_getprop_sign, NULL, "Sign.\n", NULL },
        { NULL } };

static PyObject *
FFTObj_destroy (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft_destroy (self->handle);
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
      fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFTObj_methods[] = {
  { "reset", (PyCFunction)FFTObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute_cf64", (PyCFunction)(void *)FFTObj_execute_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf64(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a double-precision complex input. The "
    "output is written to a fresh caller-supplied buffer; in and out must not "
    "alias.  The transform is unnormalised: the inverse DFT (sign=+1) does "
    "NOT divide by n.  Both buffers must be exactly state->n elements long.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_cf64_max_out", (PyCFunction)FFTObj_execute_cf64_max_out,
    METH_NOARGS,
    "execute_cf64_max_out() -> int\n\nMax output length execute_cf64() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_cf32", (PyCFunction)(void *)FFTObj_execute_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf32(x) -> ndarray\n"
    "\n"
    "Compute an out-of-place 1-D DFT on a single-precision complex input. "
    "Identical to fft_execute_cf64() but operates on float complex (CF32) "
    "buffers, halving memory bandwidth relative to the double-precision "
    "variant. Output is unnormalised; in and out must not alias.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_cf32_max_out", (PyCFunction)FFTObj_execute_cf32_max_out,
    METH_NOARGS,
    "execute_cf32_max_out() -> int\n\nMax output length execute_cf32() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_ci16", (PyCFunction)FFTObj_execute_ci16, METH_VARARGS,
    "execute_ci16(iq) -> ndarray\n"
    "\n"
    "Out-of-place 1-D FFT directly on interleaved int16 I/Q (CF32 out).\n"
    "The int16->float convert (v/32768, full-scale +/-1.0) is fused into\n"
    "the transform, so it is faster than i16_to_f32 then execute_cf32.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_ci16(np.zeros(2048, dtype=np.int16))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ci8", (PyCFunction)FFTObj_execute_ci8, METH_VARARGS,
    "execute_ci8(iq) -> ndarray\n"
    "\n"
    "Out-of-place 1-D FFT directly on interleaved int8 I/Q (CF32 out).\n"
    "As execute_ci16 but int8 input (v/128, full-scale +/-1.0).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_ci8(np.zeros(2048, dtype=np.int8))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_inplace_cf64", (PyCFunction)(void *)FFTObj_execute_inplace_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf64(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF64). The copy step lets "
    "callers preserve their input while keeping the output buffer hot in "
    "cache.  Semantically identical to fft_execute_cf64() for separate in / "
    "out pointers; use this variant when the caller already owns out and "
    "wants the result there without a second allocation.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_inplace_cf64_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf64_max_out, METH_NOARGS,
    "execute_inplace_cf64_max_out() -> int\n\nMax output length "
    "execute_inplace_cf64() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "execute_inplace_cf32", (PyCFunction)(void *)FFTObj_execute_inplace_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf32(x) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF32). Single-precision "
    "variant of fft_execute_inplace_cf64().  Copies state->n CF32 samples "
    "from in to out, then transforms out with the CF32 pocketfft plan.  in is "
    "left unmodified.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_inplace_cf32_max_out",
    (PyCFunction)FFTObj_execute_inplace_cf32_max_out, METH_NOARGS,
    "execute_inplace_cf32_max_out() -> int\n\nMax output length "
    "execute_inplace_cf32() can produce for the current state.\nUse to size "
    "the ``out=`` buffer." },
  { "destroy", (PyCFunction)FFTObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)FFTObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)FFTObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject FFTObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT",
  .tp_basicsize                           = sizeof (FFTObject),
  .tp_dealloc                             = (destructor)FFTObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a reusable 1-D FFT engine for a fixed length and sign. Two "
    "pocketfft plans are created at construction time — one for CF64 and one "
    "for CF32 — so execute calls carry no plan-setup overhead.  The same "
    "instance may be called repeatedly for independent input vectors of the "
    "same length.  nthreads is accepted for API parity but is ignored; "
    "pocketfft plans are single-threaded.\n",
  .tp_methods = FFTObj_methods,
  .tp_getset  = FFT_getset,
  .tp_new     = FFTObj_new,
  .tp_init    = (initproc)FFTObj_init,
};
