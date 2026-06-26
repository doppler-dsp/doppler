/*
 * spectral_ext_corr.c — Corr type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* CorrObject — wraps corr_state_t *       */
/* ======================================================== */

#include "corr/corr_core.h"

typedef struct
{
  PyObject_HEAD corr_state_t *handle;
  float complex *_execute_buf;     /* pre-allocated output for execute */
  size_t         _execute_buf_cap; /* allocated capacity for execute */
  void         **_execute_retired; /* gh-219 deferred free */
  size_t         _execute_retired_n;
  size_t         _execute_retired_cap;
} CorrObject;

static void
CorrObj_dealloc (CorrObject *self)
{
  if (self->handle)
    corr_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CorrObject *self = (CorrObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrObj_init (CorrObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]  = { "ref", "dwell", "nthreads", "n_out", NULL };
  PyObject          *ref_obj   = NULL;
  unsigned long long dwell_raw = 1;
  int                nthreads  = 1;
  unsigned long long n_out_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KiK", kwlist, &ref_obj,
                                    &dwell_raw, &nthreads, &n_out_raw))
    return -1;
  size_t         dwell   = (size_t)dwell_raw;
  size_t         n_out   = (size_t)n_out_raw;
  PyArrayObject *ref_arr = (PyArrayObject *)PyArray_FROM_OTF (
      ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!ref_arr)
    {
      return -1;
    }
  size_t ref_len = (size_t)PyArray_SIZE (ref_arr);
  self->handle   = corr_create ((const float complex *)PyArray_DATA (ref_arr),
                                ref_len, dwell, nthreads, n_out);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "corr_create returned NULL");
      return -1;
    }
  {
    size_t _max = corr_execute_max_out (self->handle);
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
CorrObj_reset (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  corr_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CorrObj_execute_max_out (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (corr_execute_max_out (self->handle));
}

static PyObject *
CorrObj_execute (CorrObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax = corr_execute_max_out (self->handle);
      if (_cap < _omax)
        {
          PyErr_Format (PyExc_ValueError,
                        "out has %zu elements, need >= %zu (execute_max_out)",
                        _cap, _omax);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = corr_execute (
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
      size_t _max = corr_execute_max_out (self->handle);
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
  size_t n_out = corr_execute (self->handle,
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
Corr_getprop_n (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
Corr_getprop_n_out (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n_out);
}
static PyObject *
Corr_getprop_dwell (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
Corr_getprop_count (CorrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}

static PyGetSetDef Corr_getset[]
    = { { "n", (getter)Corr_getprop_n, NULL, "N.\n", NULL },
        { "n_out", (getter)Corr_getprop_n_out, NULL, "N out.\n", NULL },
        { "dwell", (getter)Corr_getprop_dwell, NULL, "Dwell.\n", NULL },
        { "count", (getter)Corr_getprop_count, NULL, "Count.\n", NULL },
        { NULL } };

static PyObject *
CorrObj_destroy (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      corr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrObj_enter (CorrObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CorrObj_exit (CorrObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      corr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrObj_methods[] = {
  { "reset", (PyCFunction)CorrObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute", (PyCFunction)CorrObj_execute, METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Correlate one frame and optionally dump the coherent accumulator. Runs: "
    "forward FFT → pointwise multiply with ref_spec → accumulate the "
    "cross-spectrum; on dump, inverse FFT → normalise (÷ n).  Accumulating in "
    "the frequency domain and inverting once is exactly the per-frame inverse "
    "summed, by linearity of the IFFT — valid because the dwell is "
    "**coherent** (a complex sum); a non-coherent (magnitude) integration "
    "could not defer the inverse. On the @p dwell-th call @p out is written, "
    "the accumulator is zeroed, and the counter resets; the function returns "
    "n_out.  All other calls return 0 and leave @p out unmodified.  In "
    "Python, a dump returns an ndarray and a no-dump returns None.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Corr\n"
    "    >>> obj = Corr(np.zeros(1, dtype=np.complex64), 1, 1, 0)\n"
    "    >>> y = obj.execute(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)CorrObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)CorrObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CorrObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CorrObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CorrObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "spectral.Corr",
  .tp_basicsize                           = sizeof (CorrObject),
  .tp_dealloc                             = (destructor)CorrObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate a 1-D FFT correlator with coherent integrate-and-dump. "
    "Pre-computes conj(FFT(ref)) once at construction so each execute() call "
    "costs only two FFTs and n complex multiplies.  @p ref may be freed after "
    "this returns.  With @p dwell == 1 every call produces output; with "
    "larger values the accumulator absorbs @p dwell frames before dumping.\n",
  .tp_methods = CorrObj_methods,
  .tp_getset  = Corr_getset,
  .tp_new     = CorrObj_new,
  .tp_init    = (initproc)CorrObj_init,
};
