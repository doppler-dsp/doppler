/*
 * measure_ext_nprmeas.c — NPRMeasure type for the measure module.
 *
 * Included by measure_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only measure_ext.c is compiled.
 */
/* ======================================================== */
/* NPRMeasureObject — wraps nprmeas_state_t *       */
/* ======================================================== */

#include "nprmeas/nprmeas_core.h"

typedef struct
{
  PyObject_HEAD nprmeas_state_t *handle;
  float *_spectrum_dbfs_buf;     /* pre-allocated output for spectrum_dbfs */
  size_t _spectrum_dbfs_buf_cap; /* allocated capacity for spectrum_dbfs */
  void **_spectrum_dbfs_retired; /* gh-219 deferred free */
  size_t _spectrum_dbfs_retired_n;
  size_t _spectrum_dbfs_retired_cap;
} NPRMeasureObject;

static void
NPRMeasureObj_dealloc (NPRMeasureObject *self)
{
  if (self->handle)
    nprmeas_destroy (self->handle);
  free (self->_spectrum_dbfs_buf);
  for (size_t _i = 0; _i < self->_spectrum_dbfs_retired_n; _i++)
    free (self->_spectrum_dbfs_retired[_i]);
  free (self->_spectrum_dbfs_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
NPRMeasureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  NPRMeasureObject *self = (NPRMeasureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
NPRMeasureObj_init (NPRMeasureObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "n", "fs", "full_scale", "bits", "dynamic_range_db", NULL };
  unsigned long long n_raw            = 8192;
  double             fs               = 1.0;
  double             full_scale       = 1.0;
  unsigned long long bits_raw         = 0;
  double             dynamic_range_db = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KddKd", kwlist, &n_raw, &fs,
                                    &full_scale, &bits_raw, &dynamic_range_db))
    return -1;
  size_t n     = (size_t)n_raw;
  size_t bits  = (size_t)bits_raw;
  self->handle = nprmeas_create (n, fs, full_scale, bits, dynamic_range_db);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "nprmeas_create returned NULL");
      return -1;
    }
  {
    size_t _max = nprmeas_spectrum_dbfs_max_out (self->handle);
    if (_max)
      {
        self->_spectrum_dbfs_buf = malloc (_max * sizeof (float));
        if (!self->_spectrum_dbfs_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_spectrum_dbfs_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
NPRMeasureObj_reset (NPRMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  nprmeas_reset (self->handle);
  Py_RETURN_NONE;
}

static PyStructSequence_Field NPRMeasureObj_analyze_fields[] = {
  { "npr_db", NULL },
  { "inband_psd_dbfs", NULL },
  { "notch_psd_dbfs", NULL },
  { "n_inband_bins", NULL },
  { "n_notch_bins", NULL },
  { "rbw_hz", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc NPRMeasureObj_analyze_desc
    = { "doppler.measure.NPRMetrics", NULL, NPRMeasureObj_analyze_fields, 6 };
static PyTypeObject *NPRMeasureObj_analyze_type = NULL;

static PyObject *
NPRMeasureObj_analyze (NPRMeasureObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject    *in_obj    = NULL;
  double       active_lo = 0;
  double       active_hi = 0;
  double       notch_lo  = 0;
  double       notch_hi  = 0;
  double       guard_hz  = 0.0;
  static char *_kwlist[] = { "x",        "active_lo", "active_hi", "notch_lo",
                             "notch_hi", "guard_hz",  NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Odddd|d", _kwlist, &in_obj,
                                    &active_lo, &active_hi, &notch_lo,
                                    &notch_hi, &guard_hz))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  if (!NPRMeasureObj_analyze_type)
    {
      NPRMeasureObj_analyze_type
          = PyStructSequence_NewType (&NPRMeasureObj_analyze_desc);
      if (!NPRMeasureObj_analyze_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  const float *_ng0 = (const float *)PyArray_DATA (in_arr);
  npr_meas_t   _r;
  Py_BEGIN_ALLOW_THREADS
    _r = nprmeas_analyze (self->handle, _ng0, n_in, active_lo, active_hi,
                          notch_lo, notch_hi, guard_hz);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (NPRMeasureObj_analyze_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.npr_db));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.inband_psd_dbfs));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.notch_psd_dbfs));
  PyStructSequence_SET_ITEM (
      _o, 3,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.n_inband_bins));
  PyStructSequence_SET_ITEM (
      _o, 4,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.n_notch_bins));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.rbw_hz));
  return _o;
}

static PyObject *
NPRMeasureObj_spectrum_dbfs_max_out (NPRMeasureObject *self,
                                     PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nprmeas_spectrum_dbfs_max_out (self->handle));
}

static PyObject *
NPRMeasureObj_spectrum_dbfs (NPRMeasureObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nprmeas_spectrum_dbfs_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = nprmeas_spectrum_dbfs (
          self->handle, (const float *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr), (float *)PyArray_DATA (out_arr));
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_spectrum_dbfs_buf || self->_spectrum_dbfs_buf_cap < _need)
    {
      size_t _max = nprmeas_spectrum_dbfs_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_spectrum_dbfs_buf
          && self->_spectrum_dbfs_retired_n
                 == self->_spectrum_dbfs_retired_cap)
        {
          size_t _rcap = self->_spectrum_dbfs_retired_cap
                             ? self->_spectrum_dbfs_retired_cap * 2
                             : 4;
          void **_rt   = realloc (self->_spectrum_dbfs_retired,
                                  _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_spectrum_dbfs_retired     = _rt;
          self->_spectrum_dbfs_retired_cap = _rcap;
        }
      float *_tmp = malloc (_max * sizeof (float));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_spectrum_dbfs_buf)
        self->_spectrum_dbfs_retired[self->_spectrum_dbfs_retired_n++]
            = self->_spectrum_dbfs_buf;
      self->_spectrum_dbfs_buf     = _tmp;
      self->_spectrum_dbfs_buf_cap = _max;
    }
  size_t n_out = nprmeas_spectrum_dbfs (
      self->handle, (const float *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_spectrum_dbfs_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_FLOAT,
                                             self->_spectrum_dbfs_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}
static PyObject *
NPRMeasure_getprop_n (NPRMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
NPRMeasure_getprop_nfft (NPRMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
NPRMeasure_getprop_fs (NPRMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
NPRMeasure_getprop_rbw (NPRMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->enbw * self->handle->fs
                             / (double)self->handle->n);
}

static PyGetSetDef NPRMeasure_getset[]
    = { { "n", (getter)NPRMeasure_getprop_n, NULL, "N.\n", NULL },
        { "nfft", (getter)NPRMeasure_getprop_nfft, NULL, "Nfft.\n", NULL },
        { "fs", (getter)NPRMeasure_getprop_fs, NULL, "Fs.\n", NULL },
        { "rbw", (getter)NPRMeasure_getprop_rbw, NULL, "Rbw.\n", NULL },
        { NULL } };

static PyObject *
NPRMeasureObj_destroy (NPRMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      nprmeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
NPRMeasureObj_enter (NPRMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
NPRMeasureObj_exit (NPRMeasureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      nprmeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef NPRMeasureObj_methods[] = {
  { "reset", (PyCFunction)NPRMeasureObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "analyze", (PyCFunction)(void *)NPRMeasureObj_analyze,
    METH_VARARGS | METH_KEYWORDS,
    "analyze(x, active_lo, active_hi, notch_lo, notch_hi, guard_hz) -> "
    "NPRMetrics record (npr_db, inband_psd_dbfs, notch_psd_dbfs, "
    "n_inband_bins, n_notch_bins, rbw_hz)." },
  { "spectrum_dbfs", (PyCFunction)NPRMeasureObj_spectrum_dbfs,
    METH_VARARGS | METH_KEYWORDS,
    "spectrum_dbfs(x) -> ndarray\n"
    "\n"
    "DC-centred dBFS magnitude spectrum of a capture (length nfft, for "
    "plots).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NPRMeasure\n"
    "    >>> obj = NPRMeasure(8192, 1.0, 1.0, 0, 0.0)\n"
    "    >>> y = obj.spectrum_dbfs(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "spectrum_dbfs_max_out", (PyCFunction)NPRMeasureObj_spectrum_dbfs_max_out,
    METH_NOARGS,
    "spectrum_dbfs_max_out() -> int\n\nMax output length spectrum_dbfs() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)NPRMeasureObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)NPRMeasureObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)NPRMeasureObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject NPRMeasureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "measure.NPRMeasure",
  .tp_basicsize                           = sizeof (NPRMeasureObject),
  .tp_dealloc                             = (destructor)NPRMeasureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an NPRMeasure analyser (auto Kaiser window).\n",
  .tp_methods = NPRMeasureObj_methods,
  .tp_getset  = NPRMeasure_getset,
  .tp_new     = NPRMeasureObj_new,
  .tp_init    = (initproc)NPRMeasureObj_init,
};
