/*
 * spectral_ext.c — Python extension module spectral
 *
 * Objects: FFT, FFT2D
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "spectral/spectral_core.h"

static PyObject *
_bind_kaiser_enbw (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *w_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &w_obj))
    return NULL;
  PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF (
      w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!w_arr)
    return NULL;
  const float *w = (const float *)PyArray_DATA (w_arr);
  size_t wlen = (size_t)PyArray_SIZE (w_arr);
  float enbw = kaiser_enbw (w, wlen);
  Py_DECREF (w_arr);
  return PyFloat_FromDouble ((double)enbw);
}

/* kaiser_window and hann_window fill the array in-place; require writeable. */
static PyObject *
_bind_kaiser_window (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *w_obj = NULL;
  float beta = 0.0f;
  if (!PyArg_ParseTuple (args, "Of", &w_obj, &beta))
    return NULL;
  PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF (
      w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!w_arr)
    return NULL;
  float *w = (float *)PyArray_DATA (w_arr);
  size_t wlen = (size_t)PyArray_SIZE (w_arr);
  kaiser_window (w, wlen, beta);
  Py_DECREF (w_arr);
  Py_RETURN_NONE;
}

static PyObject *
_bind_hann_window (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *w_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &w_obj))
    return NULL;
  PyArrayObject *w_arr = (PyArrayObject *)PyArray_FROM_OTF (
      w_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!w_arr)
    return NULL;
  float *w = (float *)PyArray_DATA (w_arr);
  size_t wlen = (size_t)PyArray_SIZE (w_arr);
  hann_window (w, wlen);
  Py_DECREF (w_arr);
  Py_RETURN_NONE;
}

/* magnitude_db_cf32: allocate float32 output array, return it. */
static PyObject *
_bind_magnitude_db_cf32 (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *in_obj = NULL;
  float lin_floor = 0.0f;
  float offset_db = 0.0f;
  if (!PyArg_ParseTuple (args, "Off", &in_obj, &lin_floor, &offset_db))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  const float complex *in = (const float complex *)PyArray_DATA (in_arr);
  npy_intp n = PyArray_SIZE (in_arr);
  PyObject *out_arr = PyArray_EMPTY (1, &n, NPY_FLOAT, 0);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *out = (float *)PyArray_DATA ((PyArrayObject *)out_arr);
  magnitude_db_cf32 (in, (size_t)n, out, lin_floor, offset_db);
  Py_DECREF (in_arr);
  return out_arr;
}

/* magnitude_db_cf64: allocate float32 output array, return it. */
static PyObject *
_bind_magnitude_db_cf64 (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *in_obj = NULL;
  double lin_floor = 0.0;
  float offset_db = 0.0f;
  if (!PyArg_ParseTuple (args, "Odf", &in_obj, &lin_floor, &offset_db))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  const double complex *in = (const double complex *)PyArray_DATA (in_arr);
  npy_intp n = PyArray_SIZE (in_arr);
  PyObject *out_arr = PyArray_EMPTY (1, &n, NPY_FLOAT, 0);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *out = (float *)PyArray_DATA ((PyArrayObject *)out_arr);
  magnitude_db_cf64 (in, (size_t)n, out, lin_floor, offset_db);
  Py_DECREF (in_arr);
  return out_arr;
}

/* find_peaks_f32: return list[tuple[float, float]] sorted by amplitude desc.
 */
static PyObject *
_bind_find_peaks_f32 (PyObject *self, PyObject *args)
{
  (void)self;
  PyObject *db_obj = NULL;
  unsigned long long n_peaks_raw = 0ULL;
  float min_db = 0.0f;
  if (!PyArg_ParseTuple (args, "OKf", &db_obj, &n_peaks_raw, &min_db))
    return NULL;
  size_t n_peaks = (size_t)n_peaks_raw;

  PyArrayObject *db_arr = (PyArrayObject *)PyArray_FROM_OTF (
      db_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!db_arr)
    return NULL;
  const float *db = (const float *)PyArray_DATA (db_arr);
  size_t db_len = (size_t)PyArray_SIZE (db_arr);

  dp_peak_t *peaks = (dp_peak_t *)malloc (n_peaks * sizeof (dp_peak_t));
  if (!peaks)
    {
      Py_DECREF (db_arr);
      return PyErr_NoMemory ();
    }
  size_t nfound = find_peaks_f32 (db, db_len, n_peaks, min_db, peaks);
  Py_DECREF (db_arr);

  PyObject *result = PyList_New ((Py_ssize_t)nfound);
  if (!result)
    {
      free (peaks);
      return NULL;
    }
  for (size_t i = 0; i < nfound; i++)
    {
      PyObject *tup
          = Py_BuildValue ("(ff)", peaks[i].freq_norm, peaks[i].amplitude_db);
      if (!tup)
        {
          free (peaks);
          Py_DECREF (result);
          return NULL;
        }
      PyList_SET_ITEM (result, (Py_ssize_t)i, tup);
    }
  free (peaks);
  return result;
}

/* ======================================================== */
/* FFTObject — wraps fft_state_t *       */
/* ======================================================== */

#include "fft/fft_core.h"

typedef struct
{
  PyObject_HEAD fft_state_t *handle;
  double complex
      *_execute_cf64_buf;           /* pre-allocated output for execute_cf64 */
  float complex *_execute_cf32_buf; /* pre-allocated output for execute_cf32 */
  double complex *_execute_inplace_cf64_buf; /* pre-allocated output for
                                                execute_inplace_cf64 */
  float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for
                                                execute_inplace_cf32 */
} FFTObject;

static void
FFT_dealloc (FFTObject *self)
{
  if (self->handle)
    fft_destroy (self->handle);
  free (self->_execute_cf64_buf);
  free (self->_execute_cf32_buf);
  free (self->_execute_inplace_cf64_buf);
  free (self->_execute_inplace_cf32_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFT_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FFTObject *self = (FFTObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFT_init (FFTObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "n", "sign", "nthreads", NULL };
  unsigned long long n_raw = 0ULL;
  int sign = -1;
  int nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kii", kwlist, &n_raw, &sign,
                                    &nthreads))
    return -1;
  size_t n = (size_t)n_raw;
  self->handle = fft_create (n, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft_create returned NULL");
      return -1;
    }
  self->_execute_cf64_buf = malloc (fft_execute_cf64_max_out (self->handle)
                                    * sizeof (double complex));
  if (!self->_execute_cf64_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_cf32_buf = malloc (fft_execute_cf32_max_out (self->handle)
                                    * sizeof (float complex));
  if (!self->_execute_cf32_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_inplace_cf64_buf
      = malloc (fft_execute_inplace_cf64_max_out (self->handle)
                * sizeof (double complex));
  if (!self->_execute_inplace_cf64_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_inplace_cf32_buf
      = malloc (fft_execute_inplace_cf32_max_out (self->handle)
                * sizeof (float complex));
  if (!self->_execute_inplace_cf32_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  return 0;
}

static PyObject *
FFT_reset (FFTObject *self, PyObject *Py_UNUSED (ignored))
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
FFT_execute_cf64 (FFTObject *self, PyObject *args)
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
  size_t n_out = fft_execute_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf64_buf);
  npy_intp dim = (npy_intp)n_out;
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
FFT_execute_cf32 (FFTObject *self, PyObject *args)
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
  size_t n_out = fft_execute_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf32_buf);
  npy_intp dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFT_execute_inplace_cf64 (FFTObject *self, PyObject *args)
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
  size_t n_out = fft_execute_inplace_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf64_buf);
  npy_intp dim = (npy_intp)n_out;
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
FFT_execute_inplace_cf32 (FFTObject *self, PyObject *args)
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
  size_t n_out = fft_execute_inplace_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf32_buf);
  npy_intp dim = (npy_intp)n_out;
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
FFT_destroy (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFT_enter (FFTObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFT_exit (FFTObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fft_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFT_methods[]
    = { { "reset", (PyCFunction)FFT_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "execute_cf64", (PyCFunction)FFT_execute_cf64, METH_VARARGS,
          "execute_cf64(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT\n"
          "    >>> obj = FFT(1024, -1, 1)\n"
          "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex128')\n" },
        { "execute_cf32", (PyCFunction)FFT_execute_cf32, METH_VARARGS,
          "execute_cf32(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT\n"
          "    >>> obj = FFT(1024, -1, 1)\n"
          "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "execute_inplace_cf64", (PyCFunction)FFT_execute_inplace_cf64,
          METH_VARARGS,
          "execute_inplace_cf64(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT\n"
          "    >>> obj = FFT(1024, -1, 1)\n"
          "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex128')\n" },
        { "execute_inplace_cf32", (PyCFunction)FFT_execute_inplace_cf32,
          METH_VARARGS,
          "execute_inplace_cf32(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT\n"
          "    >>> obj = FFT(1024, -1, 1)\n"
          "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "destroy", (PyCFunction)FFT_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)FFT_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)FFT_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject FFTType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT",
  .tp_basicsize = sizeof (FFTObject),
  .tp_dealloc = (destructor)FFT_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "FFT type.",
  .tp_methods = FFT_methods,
  .tp_getset = FFT_getset,
  .tp_new = FFT_new,
  .tp_init = (initproc)FFT_init,
};
/* ======================================================== */
/* FFT2DObject — wraps fft2d_state_t *       */
/* ======================================================== */

#include "fft2d/fft2d_core.h"

typedef struct
{
  PyObject_HEAD fft2d_state_t *handle;
  double complex
      *_execute_cf64_buf;           /* pre-allocated output for execute_cf64 */
  float complex *_execute_cf32_buf; /* pre-allocated output for execute_cf32 */
  double complex *_execute_inplace_cf64_buf; /* pre-allocated output for
                                                execute_inplace_cf64 */
  float complex *_execute_inplace_cf32_buf;  /* pre-allocated output for
                                                execute_inplace_cf32 */
} FFT2DObject;

static void
FFT2D_dealloc (FFT2DObject *self)
{
  if (self->handle)
    fft2d_destroy (self->handle);
  free (self->_execute_cf64_buf);
  free (self->_execute_cf32_buf);
  free (self->_execute_inplace_cf64_buf);
  free (self->_execute_inplace_cf32_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
FFT2D_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  FFT2DObject *self = (FFT2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
FFT2D_init (FFT2DObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "ny", "nx", "sign", "nthreads", NULL };
  unsigned long long ny_raw = 0ULL;
  unsigned long long nx_raw = 0ULL;
  int sign = -1;
  int nthreads = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKii", kwlist, &ny_raw,
                                    &nx_raw, &sign, &nthreads))
    return -1;
  size_t ny = (size_t)ny_raw;
  size_t nx = (size_t)nx_raw;
  self->handle = fft2d_create (ny, nx, sign, nthreads);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "fft2d_create returned NULL");
      return -1;
    }
  self->_execute_cf64_buf = malloc (fft2d_execute_cf64_max_out (self->handle)
                                    * sizeof (double complex));
  if (!self->_execute_cf64_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_cf32_buf = malloc (fft2d_execute_cf32_max_out (self->handle)
                                    * sizeof (float complex));
  if (!self->_execute_cf32_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_inplace_cf64_buf
      = malloc (fft2d_execute_inplace_cf64_max_out (self->handle)
                * sizeof (double complex));
  if (!self->_execute_inplace_cf64_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  self->_execute_inplace_cf32_buf
      = malloc (fft2d_execute_inplace_cf32_max_out (self->handle)
                * sizeof (float complex));
  if (!self->_execute_inplace_cf32_buf)
    {
      PyErr_NoMemory ();
      return -1;
    }
  return 0;
}

static PyObject *
FFT2D_reset (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
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
FFT2D_execute_cf64 (FFT2DObject *self, PyObject *args)
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
  size_t n_out = fft2d_execute_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf64_buf);
  npy_intp dim = (npy_intp)n_out;
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
FFT2D_execute_cf32 (FFT2DObject *self, PyObject *args)
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
  size_t n_out = fft2d_execute_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_cf32_buf);
  npy_intp dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_cf32_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}

static PyObject *
FFT2D_execute_inplace_cf64 (FFT2DObject *self, PyObject *args)
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
  size_t n_out = fft2d_execute_inplace_cf64 (
      self->handle, (const double complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf64_buf);
  npy_intp dim = (npy_intp)n_out;
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
FFT2D_execute_inplace_cf32 (FFT2DObject *self, PyObject *args)
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
  size_t n_out = fft2d_execute_inplace_cf32 (
      self->handle, (const float complex *)PyArray_DATA (in_arr), (size_t)n,
      self->_execute_inplace_cf32_buf);
  npy_intp dim = (npy_intp)n_out;
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
    = { { "ny", (getter)FFT2D_getprop_ny, NULL, NULL, NULL },
        { "nx", (getter)FFT2D_getprop_nx, NULL, NULL, NULL },
        { "sign", (getter)FFT2D_getprop_sign, NULL, NULL, NULL },
        { NULL } };

static PyObject *
FFT2D_destroy (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
FFT2D_enter (FFT2DObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
FFT2D_exit (FFT2DObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      fft2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef FFT2D_methods[]
    = { { "reset", (PyCFunction)FFT2D_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "execute_cf64", (PyCFunction)FFT2D_execute_cf64, METH_VARARGS,
          "execute_cf64(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT2D\n"
          "    >>> obj = FFT2D(64, 64, -1, 1)\n"
          "    >>> y = obj.execute_cf64(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex128')\n" },
        { "execute_cf32", (PyCFunction)FFT2D_execute_cf32, METH_VARARGS,
          "execute_cf32(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT2D\n"
          "    >>> obj = FFT2D(64, 64, -1, 1)\n"
          "    >>> y = obj.execute_cf32(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "execute_inplace_cf64", (PyCFunction)FFT2D_execute_inplace_cf64,
          METH_VARARGS,
          "execute_inplace_cf64(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT2D\n"
          "    >>> obj = FFT2D(64, 64, -1, 1)\n"
          "    >>> y = obj.execute_inplace_cf64(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex128')\n" },
        { "execute_inplace_cf32", (PyCFunction)FFT2D_execute_inplace_cf32,
          METH_VARARGS,
          "execute_inplace_cf32(x) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import FFT2D\n"
          "    >>> obj = FFT2D(64, 64, -1, 1)\n"
          "    >>> y = obj.execute_inplace_cf32(1.0 + 0.0j)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "destroy", (PyCFunction)FFT2D_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)FFT2D_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)FFT2D_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject FFT2DType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.FFT2D",
  .tp_basicsize = sizeof (FFT2DObject),
  .tp_dealloc = (destructor)FFT2D_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "FFT2D type.",
  .tp_methods = FFT2D_methods,
  .tp_getset = FFT2D_getset,
  .tp_new = FFT2D_new,
  .tp_init = (initproc)FFT2D_init,
};

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef Spectral_methods[] = {
  { "kaiser_enbw", _bind_kaiser_enbw, METH_VARARGS, "kaiser_enbw." },
  { "kaiser_window", _bind_kaiser_window, METH_VARARGS, "kaiser_window." },
  { "hann_window", _bind_hann_window, METH_VARARGS, "hann_window." },
  { "magnitude_db_cf32", _bind_magnitude_db_cf32, METH_VARARGS,
    "magnitude_db_cf32." },
  { "magnitude_db_cf64", _bind_magnitude_db_cf64, METH_VARARGS,
    "magnitude_db_cf64." },
  { "find_peaks_f32", _bind_find_peaks_f32, METH_VARARGS, "find_peaks_f32." },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef spectral_moduledef = {
  PyModuleDef_HEAD_INIT,         .m_name = "spectral",
  .m_doc = "Spectral module.",   .m_size = -1,
  .m_methods = Spectral_methods,
};

PyMODINIT_FUNC
PyInit_spectral (void)
{
  import_array ();
  if (PyType_Ready (&FFTType) < 0)
    return NULL;
  if (PyType_Ready (&FFT2DType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&spectral_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&FFTType);
  if (PyModule_AddObject (m, "FFT", (PyObject *)&FFTType) < 0)
    {
      Py_DECREF (&FFTType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&FFT2DType);
  if (PyModule_AddObject (m, "FFT2D", (PyObject *)&FFT2DType) < 0)
    {
      Py_DECREF (&FFT2DType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
