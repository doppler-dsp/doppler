/*
 * spectral_ext_corr2d.c — Corr2D type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* Corr2DObject — wraps corr2d_state_t *       */
/* ======================================================== */

#include "corr2d/corr2d_core.h"

typedef struct
{
  PyObject_HEAD corr2d_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
  void         **_execute_retired; /* gh-219 deferred free */
  size_t         _execute_retired_n;
  size_t         _execute_retired_cap;
} Corr2DObject;

static void
Corr2DObj_dealloc (Corr2DObject *self)
{
  if (self->handle)
    corr2d_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Corr2DObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Corr2DObject *self = (Corr2DObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
Corr2DObj_init (Corr2DObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "ref", "dwell", "nthreads", "ny_out", "nx_out", NULL };
  PyObject          *ref_obj    = NULL;
  unsigned long long dwell_raw  = 1;
  int                nthreads   = 1;
  unsigned long long ny_out_raw = 0;
  unsigned long long nx_out_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KiKK", kwlist, &ref_obj,
                                    &dwell_raw, &nthreads, &ny_out_raw,
                                    &nx_out_raw))
    return -1;
  size_t         dwell   = (size_t)dwell_raw;
  size_t         ny_out  = (size_t)ny_out_raw;
  size_t         nx_out  = (size_t)nx_out_raw;
  PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  /* Bespoke 2-D ref handling: jm's array init-param is 1-D, but Corr2D takes a
   * (ny, nx) ndarray and corr2d_create wants the two dims split out. */
  if (PyArray_NDIM (ref_arr) != 2)
    {
      Py_DECREF (ref_arr);
      PyErr_SetString (PyExc_ValueError, "ref must be a 2-D (ny, nx) array");
      return -1;
    }
  size_t ref_dim0 = (size_t)PyArray_DIM (ref_arr, 0);
  size_t ref_dim1 = (size_t)PyArray_DIM (ref_arr, 1);
  self->handle
      = corr2d_create ((const float complex *)PyArray_DATA (ref_arr), ref_dim0,
                       ref_dim1, dwell, nthreads, ny_out, nx_out);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "corr2d_create returned NULL");
      return -1;
    }
  {
    size_t _max = corr2d_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (float complex));
        if (!self->_execute_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
Corr2DObj_reset (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  corr2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
Corr2DObj_execute_max_out (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (corr2d_execute_max_out (self->handle));
}

static PyObject *
Corr2DObj_execute (Corr2DObject *self, PyObject *args, PyObject *kwds)
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = corr2d_execute_max_out (self->handle);
      if (_cap < _omax)
        {
          PyErr_Format (PyExc_ValueError,
                        "out has %zu elements, need >= %zu (execute_max_out)",
                        _cap, _omax);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = corr2d_execute (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
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
  if (!self->_execute_buf || self->_execute_buf_cap < _need)
    {
      size_t _max = corr2d_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_buf
          && self->_execute_retired_n == self->_execute_retired_cap)
        {
          size_t _rcap = self->_execute_retired_cap
                             ? self->_execute_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  size_t n_out = corr2d_execute (self->handle,
                                 (const float complex *)PyArray_DATA (in_arr),
                                 (size_t)n, self->_execute_buf);
  if (!n_out)
    Py_RETURN_NONE;
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (in_arr);
  return arr;
}
static PyObject *
Corr2D_getprop_ny (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
Corr2D_getprop_nx (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
Corr2D_getprop_ny_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->ny_out);
}
static PyObject *
Corr2D_getprop_nx_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->nx_out);
}
static PyObject *
Corr2D_getprop_n_out (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n_out);
}
static PyObject *
Corr2D_getprop_dwell (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr2D_getprop_count (Corr2DObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr2D_getset[]
    = { { "ny", (getter)Corr2D_getprop_ny, NULL, "Ny.\n", NULL },
        { "nx", (getter)Corr2D_getprop_nx, NULL, "Nx.\n", NULL },
        { "ny_out", (getter)Corr2D_getprop_ny_out, NULL, "Ny out.\n", NULL },
        { "nx_out", (getter)Corr2D_getprop_nx_out, NULL, "Nx out.\n", NULL },
        { "n_out", (getter)Corr2D_getprop_n_out, NULL, "N out.\n", NULL },
        { "dwell", (getter)Corr2D_getprop_dwell, NULL, "Dwell.\n", NULL },
        { "count", (getter)Corr2D_getprop_count, NULL, "Count.\n", NULL },
        { NULL } };

static PyObject *
Corr2DObj_destroy (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      corr2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Corr2DObj_enter (Corr2DObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Corr2DObj_exit (Corr2DObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      corr2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef Corr2DObj_methods[] = {
  { "reset", (PyCFunction)Corr2DObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute", (PyCFunction)Corr2DObj_execute, METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Correlate one 2-D frame and optionally dump the coherent accumulator. "
    "Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → "
    "accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny*nx).  "
    "Accumulating in the frequency domain and inverting once is exactly the "
    "per-frame inverse summed, by linearity of the IFFT — valid because the "
    "dwell is **coherent** (a complex sum); a non-coherent (magnitude) "
    "integration could not defer the inverse.  The Python wrapper accepts a "
    "(ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx ndarray, a "
    "no-dump returns None.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Corr2D\n"
    "    >>> obj = Corr2D(np.zeros(1, dtype=np.complex64), 1, 1, 0, 0)\n"
    "    >>> y = obj.execute(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)Corr2DObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)Corr2DObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)Corr2DObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Corr2DObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject Corr2DObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.Corr2D",
  .tp_basicsize                           = sizeof (Corr2DObject),
  .tp_dealloc                             = (destructor)Corr2DObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a 2-D FFT correlator with coherent integrate-and-dump. "
    "Two-dimensional extension of corr_create().  The reference is a flat "
    "row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once "
    "so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies. "
    "The Python wrapper requires @p ref to be a 2-D ndarray with shape (ny, "
    "nx); it passes a flat view to C.\n",
  .tp_methods = Corr2DObj_methods,
  .tp_getset  = Corr2D_getset,
  .tp_new     = Corr2DObj_new,
  .tp_init    = (initproc)Corr2DObj_init,
};
