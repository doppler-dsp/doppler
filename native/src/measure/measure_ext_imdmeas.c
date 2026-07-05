/*
 * measure_ext_imdmeas.c — IMDMeasure type for the measure module.
 *
 * Included by measure_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only measure_ext.c is compiled.
 */
/* ======================================================== */
/* IMDMeasureObject — wraps imdmeas_state_t *       */
/* ======================================================== */

#include "imdmeas/imdmeas_core.h"

typedef struct
{
  PyObject_HEAD imdmeas_state_t *handle;
  float *_spectrum_dbfs_buf;     /* pre-allocated output for spectrum_dbfs */
  size_t _spectrum_dbfs_buf_cap; /* allocated capacity for spectrum_dbfs */
  void **_spectrum_dbfs_retired; /* gh-219 deferred free */
  size_t _spectrum_dbfs_retired_n;
  size_t _spectrum_dbfs_retired_cap;
} IMDMeasureObject;

static void
IMDMeasureObj_dealloc (IMDMeasureObject *self)
{
  if (self->handle)
    imdmeas_destroy (self->handle);
  free (self->_spectrum_dbfs_buf);
  for (size_t _i = 0; _i < self->_spectrum_dbfs_retired_n; _i++)
    free (self->_spectrum_dbfs_retired[_i]);
  free (self->_spectrum_dbfs_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
IMDMeasureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  IMDMeasureObject *self = (IMDMeasureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
IMDMeasureObj_init (IMDMeasureObject *self, PyObject *args, PyObject *kwds)
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
  self->handle = imdmeas_create (n, fs, full_scale, bits, dynamic_range_db);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "imdmeas_create returned NULL");
      return -1;
    }
  {
    size_t _max = imdmeas_spectrum_dbfs_max_out (self->handle);
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
IMDMeasureObj_reset (IMDMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  imdmeas_reset (self->handle);
  Py_RETURN_NONE;
}

static PyStructSequence_Field IMDMeasureObj_analyze_fields[] = {
  { "f1", NULL },        { "f2", NULL },           { "p1_dbfs", NULL },
  { "p2_dbfs", NULL },   { "imd2_dbc", NULL },     { "imd3_dbc", NULL },
  { "imd2_freq", NULL }, { "imd3_lo_freq", NULL }, { "imd3_hi_freq", NULL },
  { "toi_dbfs", NULL },  { "soi_dbfs", NULL },     { "rbw_hz", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc IMDMeasureObj_analyze_desc
    = { "doppler.measure.IMDMetrics", NULL, IMDMeasureObj_analyze_fields, 12 };
static PyTypeObject *IMDMeasureObj_analyze_type = NULL;

static PyObject *
IMDMeasureObj_analyze (IMDMeasureObject *self, PyObject *args)
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
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  if (!IMDMeasureObj_analyze_type)
    {
      IMDMeasureObj_analyze_type
          = PyStructSequence_NewType (&IMDMeasureObj_analyze_desc);
      if (!IMDMeasureObj_analyze_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  const float *_ng0 = (const float *)PyArray_DATA (in_arr);
  imd_meas_t   _r;
  Py_BEGIN_ALLOW_THREADS
    _r = imdmeas_analyze (self->handle, _ng0, n_in);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (IMDMeasureObj_analyze_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.f1));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.f2));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.p1_dbfs));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.p2_dbfs));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.imd2_dbc));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.imd3_dbc));
  PyStructSequence_SET_ITEM (_o, 6, PyFloat_FromDouble (_r.imd2_freq));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.imd3_lo_freq));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.imd3_hi_freq));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.toi_dbfs));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.soi_dbfs));
  PyStructSequence_SET_ITEM (_o, 11, PyFloat_FromDouble (_r.rbw_hz));
  return _o;
}

static PyObject *
IMDMeasureObj_spectrum_dbfs_max_out (IMDMeasureObject *self,
                                     PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (imdmeas_spectrum_dbfs_max_out (self->handle));
}

static PyObject *
IMDMeasureObj_spectrum_dbfs (IMDMeasureObject *self, PyObject *args,
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
      size_t _omax    = imdmeas_spectrum_dbfs_max_out (self->handle);
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
      size_t n_out = imdmeas_spectrum_dbfs (
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
      size_t _max = imdmeas_spectrum_dbfs_max_out (self->handle);
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
  size_t n_out = imdmeas_spectrum_dbfs (
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
IMDMeasure_getprop_n (IMDMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
IMDMeasure_getprop_nfft (IMDMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
IMDMeasure_getprop_fs (IMDMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}

static PyGetSetDef IMDMeasure_getset[]
    = { { "n", (getter)IMDMeasure_getprop_n, NULL, "N.\n", NULL },
        { "nfft", (getter)IMDMeasure_getprop_nfft, NULL, "Nfft.\n", NULL },
        { "fs", (getter)IMDMeasure_getprop_fs, NULL, "Fs.\n", NULL },
        { NULL } };

static PyObject *
IMDMeasureObj_destroy (IMDMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      imdmeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
IMDMeasureObj_enter (IMDMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
IMDMeasureObj_exit (IMDMeasureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      imdmeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef IMDMeasureObj_methods[] = {
  { "reset", (PyCFunction)IMDMeasureObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "analyze", (PyCFunction)IMDMeasureObj_analyze, METH_VARARGS,
    "analyze(x) -> IMDMetrics record (f1, f2, p1_dbfs, p2_dbfs, imd2_dbc, "
    "imd3_dbc, imd2_freq, imd3_lo_freq, imd3_hi_freq, toi_dbfs, soi_dbfs, "
    "rbw_hz)." },
  { "spectrum_dbfs", (PyCFunction)IMDMeasureObj_spectrum_dbfs,
    METH_VARARGS | METH_KEYWORDS,
    "spectrum_dbfs(x) -> ndarray\n"
    "\n"
    "DC-centred dBFS magnitude spectrum of a capture (length nfft, for "
    "plots).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import IMDMeasure\n"
    "    >>> obj = IMDMeasure(8192, 1.0, 1.0, 0, 0.0)\n"
    "    >>> y = obj.spectrum_dbfs(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "spectrum_dbfs_max_out", (PyCFunction)IMDMeasureObj_spectrum_dbfs_max_out,
    METH_NOARGS,
    "spectrum_dbfs_max_out() -> int\n\nMax output length spectrum_dbfs() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)IMDMeasureObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)IMDMeasureObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)IMDMeasureObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject IMDMeasureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "measure.IMDMeasure",
  .tp_basicsize                           = sizeof (IMDMeasureObject),
  .tp_dealloc                             = (destructor)IMDMeasureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an IMDMeasure analyser (auto Kaiser window).\n",
  .tp_methods = IMDMeasureObj_methods,
  .tp_getset  = IMDMeasure_getset,
  .tp_new     = IMDMeasureObj_new,
  .tp_init    = (initproc)IMDMeasureObj_init,
};
