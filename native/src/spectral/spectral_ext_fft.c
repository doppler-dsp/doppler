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
  double complex
                *_execute_cf64_buf; /* pre-allocated output for execute_cf64 */
  float complex *_execute_cf32_buf; /* pre-allocated output for execute_cf32 */
  double complex *_execute_inplace_cf64_buf; /* pre-allocated output for
                                                execute_inplace_cf64 */
  float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for
                                                execute_inplace_cf32 */
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
  unsigned long long n_raw    = 0ULL;
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
FFTObj_execute_cf64 (FFTObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->_execute_cf64_buf)
    {
      size_t _max = fft_execute_cf64_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_cf64_buf = malloc (_max * sizeof (double complex));
      if (!self->_execute_cf64_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = fft_execute_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf64_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                             self->_execute_cf64_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFTObj_execute_cf32 (FFTObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->_execute_cf32_buf)
    {
      size_t _max = fft_execute_cf32_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_cf32_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_cf32_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = fft_execute_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

/* Integer-IQ executes (ci16/ci8): interleaved int16/int8 I/Q in, CF32 out.
 * The int->float convert is folded into the FFT input read (no separate cvt
 * pass).  Output reuses the same pre-allocated CF32 buffer as execute_cf32. */
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
  Py_ssize_t n = PyArray_SIZE (in_arr) / 2;
  if (!self->_execute_cf32_buf)
    {
      size_t _max = fft_execute_cf32_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_cf32_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_cf32_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out
      = is8 ? fft_execute_ci8 (self->handle,
                               (const int8_t *)PyArray_DATA (in_arr),
                               (size_t)n, self->_execute_cf32_buf)
            : fft_execute_ci16 (self->handle,
                                (const int16_t *)PyArray_DATA (in_arr),
                                (size_t)n, self->_execute_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
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
FFTObj_execute_inplace_cf64 (FFTObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->_execute_inplace_cf64_buf)
    {
      size_t _max = fft_execute_inplace_cf64_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_inplace_cf64_buf
          = malloc (_max * sizeof (double complex));
      if (!self->_execute_inplace_cf64_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = fft_execute_inplace_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf64_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                             self->_execute_inplace_cf64_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFTObj_execute_inplace_cf32 (FFTObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->_execute_inplace_cf32_buf)
    {
      size_t _max = fft_execute_inplace_cf32_max_out (self->handle);
      if (!_max)
        _max = (size_t)n;
      self->_execute_inplace_cf32_buf = malloc (_max * sizeof (float complex));
      if (!self->_execute_inplace_cf32_buf)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = fft_execute_inplace_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf32_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_inplace_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
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
    = { { "n", (getter)FFT_getprop_n, NULL, NULL, NULL },
        { "sign", (getter)FFT_getprop_sign, NULL, NULL, NULL },
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

  { "execute_cf64", (PyCFunction)FFTObj_execute_cf64, METH_VARARGS,
    "execute_cf64(x) -> ndarray\n"
    "\n"
    "Out-of-place 1-D CF64 FFT.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_cf32", (PyCFunction)FFTObj_execute_cf32, METH_VARARGS,
    "execute_cf32(x) -> ndarray\n"
    "\n"
    "Out-of-place 1-D CF32 FFT.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
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
  { "execute_inplace_cf64", (PyCFunction)FFTObj_execute_inplace_cf64,
    METH_VARARGS,
    "execute_inplace_cf64(x) -> ndarray\n"
    "\n"
    "In-place 1-D CF64 FFT (copies in→out, then transforms in out).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_inplace_cf32", (PyCFunction)FFTObj_execute_inplace_cf32,
    METH_VARARGS,
    "execute_inplace_cf32(x) -> ndarray\n"
    "\n"
    "In-place 1-D CF32 FFT (copies in→out, then transforms in out).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import FFT\n"
    "    >>> obj = FFT(1024, -1, 1)\n"
    "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
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
  .tp_doc                                 = "Create a 1-D FFT instance.\n",
  .tp_methods                             = FFTObj_methods,
  .tp_getset                              = FFT_getset,
  .tp_new                                 = FFTObj_new,
  .tp_init                                = (initproc)FFTObj_init,
};
