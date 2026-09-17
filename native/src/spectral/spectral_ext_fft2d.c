/*
 * spectral_ext_fft2d.c — FFT2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* FFT2DObject — wraps fft2d_state_t *       */
/* ======================================================== */

#include "fft2d/fft2d_core.h"

typedef struct
{
  PyObject_HEAD fft2d_state_t *handle;
} FFT2DObject;

static void
FFT2DObj_dealloc (FFT2DObject *self)
{
  if (self->handle)
    fft2d_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFT2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FFT2DObject *self = (FFT2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFT2DObj_init (FFT2DObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "ny", "nx", "sign", "nthreads", NULL };
  unsigned long long ny_raw   = 64;
  unsigned long long nx_raw   = 64;
  int                sign     = -1;
  int                nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKii", kwlist, &ny_raw,
                                    &nx_raw, &sign, &nthreads))
    return -1;
  size_t ny    = (size_t)ny_raw;
  size_t nx    = (size_t)nx_raw;
  self->handle = fft2d_create (ny, nx, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft2d_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
FFT2DObj_reset (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  fft2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
FFT2DObj_execute_cf64_max_out (FFT2DObject *self,
                               PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_cf64_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_cf64 (FFT2DObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft2d_execute_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_cf64 (
          self->handle, (const double _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (double _Complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = fft2d_execute_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double _Complex *_d0
      = (double _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = fft2d_execute_cf64 (
      self->handle, (const double _Complex *)PyArray_DATA (in_arr), (size_t)n,
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
FFT2DObj_execute_cf32_max_out (FFT2DObject *self,
                               PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_cf32_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_cf32 (FFT2DObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = fft2d_execute_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_cf32 (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = fft2d_execute_cf32_max_out (self->handle);
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
  size_t          n_out = fft2d_execute_cf32 (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
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
FFT2DObj_execute_inplace_cf64_max_out (FFT2DObject *self,
                                       PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_inplace_cf64_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_inplace_cf64 (FFT2DObject *self, PyObject *args,
                               PyObject *kwds)
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
      size_t _omax    = fft2d_execute_inplace_cf64_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_inplace_cf64 (
          self->handle, (const double _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (double _Complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = fft2d_execute_inplace_cf64_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double _Complex *_d0
      = (double _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = fft2d_execute_inplace_cf64 (
      self->handle, (const double _Complex *)PyArray_DATA (in_arr), (size_t)n,
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
FFT2DObj_execute_inplace_cf32_max_out (FFT2DObject *self,
                                       PyObject    *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (fft2d_execute_inplace_cf32_max_out (self->handle));
}

static PyObject *
FFT2DObj_execute_inplace_cf32 (FFT2DObject *self, PyObject *args,
                               PyObject *kwds)
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
      size_t _omax    = fft2d_execute_inplace_cf32_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = fft2d_execute_inplace_cf32 (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = fft2d_execute_inplace_cf32_max_out (self->handle);
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
  size_t          n_out = fft2d_execute_inplace_cf32 (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
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
FFT2D_getprop_ny (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
FFT2D_getprop_nx (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
FFT2D_getprop_sign (FFT2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->sign);
}

static PyGetSetDef FFT2D_getset[]
    = { { "ny", (getter)FFT2D_getprop_ny, NULL, "Row count.\n", NULL },
        { "nx", (getter)FFT2D_getprop_nx, NULL, "Column count.\n", NULL },
        { "sign", (getter)FFT2D_getprop_sign, NULL,
          "-1 forward, +1 inverse.\n", NULL },
        { NULL } };

static PyObject *
FFT2DObj_destroy (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFT2DObj_enter (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFT2DObj_exit (FFT2DObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFT2DObj_methods[] = {
  { "reset", (PyCFunction)FFT2DObj_reset, METH_NOARGS,
    "No-op reset (plans are immutable after creation)." },

  { "execute_cf64", (PyCFunction)(void *)FFT2DObj_execute_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf64(x, out) -> ndarray\n"
    "\n"
    "Compute an out-of-place 2-D DFT on a double-precision complex grid.\n"
    "in is a flat row-major CF64 array of length ny*nx. The output is\n"
    "written to the caller-supplied out buffer (also ny*nx); the two must\n"
    "not alias. The transform is unnormalised.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "out : NDArray[np.complex128] | None\n"
    "    Flat row-major CF64 output, length >= ny*nx (caller-allocated).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex128]\n"
    "    min(ny*nx, max_out) samples.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT2D\n"
    ">>> import numpy as np\n"
    ">>> fft2d = FFT2D(ny=4, nx=4, sign=-1)\n"
    ">>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0\n"
    ">>> out = fft2d.execute_cf64(x)\n"
    ">>> out.shape, out.dtype\n"
    "((16,), dtype('complex128'))\n"
    ">>> bool(np.allclose(out, 1.0))\n"
    "True\n" },
  { "execute_cf64_max_out", (PyCFunction)FFT2DObj_execute_cf64_max_out,
    METH_NOARGS,
    "execute_cf64_max_out() -> int\n"
    "\n"
    "Maximum output samples per execute call (ny * nx).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_cf32", (PyCFunction)(void *)FFT2DObj_execute_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_cf32(x, out) -> ndarray\n"
    "\n"
    "Compute an out-of-place 2-D DFT on a single-precision complex grid.\n"
    "Single-precision variant of fft2d_execute_cf64(). Accepts and returns\n"
    "flat row-major CF32 arrays of length ny*nx. Output is unnormalised; in\n"
    "and out must not alias.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Flat row-major CF32 output, length >= ny*nx (caller-allocated).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    min(ny*nx, max_out) samples.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT2D\n"
    ">>> import numpy as np\n"
    ">>> fft2d = FFT2D(ny=4, nx=4, sign=-1)\n"
    ">>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0\n"
    ">>> out = fft2d.execute_cf32(x)\n"
    ">>> out.shape, out.dtype\n"
    "((16,), dtype('complex64'))\n"
    ">>> bool(np.allclose(out, 1.0))\n"
    "True\n" },
  { "execute_cf32_max_out", (PyCFunction)FFT2DObj_execute_cf32_max_out,
    METH_NOARGS,
    "execute_cf32_max_out() -> int\n"
    "\n"
    "Maximum output samples for CF32 execute (ny * nx).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_inplace_cf64", (PyCFunction)(void *)FFT2DObj_execute_inplace_cf64,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf64(x, out) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF64 2-D). The ny*nx\n"
    "CF64 samples from in are first memcpy'd to out; the 2-D DFT is then\n"
    "applied to out in-place. in is left unmodified. Useful when the caller\n"
    "owns out and wants to preserve in.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "out : NDArray[np.complex128] | None\n"
    "    Destination, length >= ny*nx; must not alias in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex128]\n"
    "    min(ny*nx, max_out) samples.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT2D\n"
    ">>> import numpy as np\n"
    ">>> fft2d = FFT2D(ny=4, nx=4, sign=-1)\n"
    ">>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0\n"
    ">>> out = fft2d.execute_inplace_cf64(x)\n"
    ">>> bool(np.allclose(out, 1.0))\n"
    "True\n" },
  { "execute_inplace_cf64_max_out",
    (PyCFunction)FFT2DObj_execute_inplace_cf64_max_out, METH_NOARGS,
    "execute_inplace_cf64_max_out() -> int\n"
    "\n"
    "Maximum output samples for inplace CF64 execute (ny * nx).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_inplace_cf32", (PyCFunction)(void *)FFT2DObj_execute_inplace_cf32,
    METH_VARARGS | METH_KEYWORDS,
    "execute_inplace_cf32(x, out) -> ndarray\n"
    "\n"
    "Copy in into out, then transform out in-place (CF32 2-D).\n"
    "Single-precision variant of fft2d_execute_inplace_cf64(). Copies ny*nx\n"
    "CF32 samples then applies the CF32 2-D pocketfft plan to out.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Destination, length >= ny*nx; must not alias in.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    min(ny*nx, max_out) samples.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import FFT2D\n"
    ">>> import numpy as np\n"
    ">>> fft2d = FFT2D(ny=4, nx=4, sign=-1)\n"
    ">>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0\n"
    ">>> out = fft2d.execute_inplace_cf32(x)\n"
    ">>> bool(np.allclose(out, 1.0))\n"
    "True\n" },
  { "execute_inplace_cf32_max_out",
    (PyCFunction)FFT2DObj_execute_inplace_cf32_max_out, METH_NOARGS,
    "execute_inplace_cf32_max_out() -> int\n"
    "\n"
    "Maximum output samples for inplace CF32 execute (ny * nx).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "destroy", (PyCFunction)FFT2DObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)FFT2DObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a FFT2D be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "FFT2D\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)FFT2DObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the FFT2D.\n"
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
  { NULL }
};

static PyTypeObject FFT2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT2D",
  .tp_basicsize                           = sizeof (FFT2DObject),
  .tp_dealloc                             = (destructor)FFT2DObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two "
            "pocketfft\n"
            "2-D plans are built at construction time — one CF64, one CF32. "
            "All execute\n"
            "calls accept and return flat row-major arrays of length ny*nx; "
            "the Python\n"
            "layer may reshape them with .reshape(ny, nx). nthreads is "
            "accepted for API\n"
            "parity but ignored.\n"
            "\n"
            "Parameters\n"
            "----------\n"
            "ny : int, default 64\n"
            "    Number of rows (outer dimension).\n"
            "nx : int, default 64\n"
            "    Number of columns (inner dimension).\n"
            "sign : int, default -1\n"
            "    -1 for the forward DFT, +1 for the inverse DFT.\n"
            "nthreads : int, default 1\n"
            "    Accepted for API compatibility; ignored.\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> from doppler.spectral import FFT2D\n"
            ">>> import numpy as np\n"
            ">>> fft2d = FFT2D(ny=4, nx=4, sign=-1, nthreads=1)\n"
            ">>> fft2d.ny, fft2d.nx, fft2d.sign\n"
            "(4, 4, -1)\n"
            ">>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0\n"
            ">>> out = fft2d.execute_cf32(x)\n"
            ">>> out.shape, out.dtype\n"
            "((16,), dtype('complex64'))\n"
            ">>> bool(np.allclose(out, 1.0))\n"
            "True\n",
  .tp_methods = FFT2DObj_methods,
  .tp_getset  = FFT2D_getset,
  .tp_new     = FFT2DObj_new,
  .tp_init    = (initproc)FFT2DObj_init,
};
