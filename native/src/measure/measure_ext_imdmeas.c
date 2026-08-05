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
} IMDMeasureObject;

static void
IMDMeasureObj_dealloc (IMDMeasureObject *self)
{
  if (self->handle)
    imdmeas_destroy (self->handle);
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
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (x_arr);
          return NULL;
        }
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
          (size_t)PyArray_SIZE (x_arr), (float *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = imdmeas_spectrum_dbfs_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float *_d0   = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = imdmeas_spectrum_dbfs (
      self->handle, (const float *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), _d0, _cap);
  Py_DECREF (x_arr);
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
    = { { "n", (getter)IMDMeasure_getprop_n, NULL,
          "Window / frame length (samples).\n", NULL },
        { "nfft", (getter)IMDMeasure_getprop_nfft, NULL,
          "Zero-padded transform length.\n", NULL },
        { "fs", (getter)IMDMeasure_getprop_fs, NULL, "Sample rate, Hz.\n",
          NULL },
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
    "Reset the analyser (a no-op: each analyze() call is independent)." },

  { "analyze", (PyCFunction)IMDMeasureObj_analyze, METH_VARARGS,
    "analyze(x) -> IMDMetrics record (f1, f2, p1_dbfs, p2_dbfs, imd2_dbc, "
    "imd3_dbc, imd2_freq, imd3_lo_freq, imd3_hi_freq, toi_dbfs, soi_dbfs, "
    "rbw_hz)." },
  { "spectrum_dbfs", (PyCFunction)(void *)IMDMeasureObj_spectrum_dbfs,
    METH_VARARGS | METH_KEYWORDS,
    "spectrum_dbfs(x) -> ndarray\n"
    "\n"
    "DC-centred dBFS magnitude spectrum of a capture (length nfft, for "
    "plots).\n"
    "\n"
    "The same windowed, zero-padded PSD the IMD metrics are read off, laid\n"
    "out DC-centred (fftshifted) and normalised to dBFS for an\n"
    "analyzer-display backdrop. Use it to see the two fundamentals and the\n"
    "intermodulation products that analyze() integrates.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Real time-domain capture (length x_len).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    DC-centred dBFS magnitude spectrum, one value per FFT bin (nfft).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.measure import IMDMeasure\n"
    ">>> import numpy as np\n"
    ">>> t = np.arange(4096)\n"
    ">>> x = (0.5*np.cos(2*np.pi*200*t/4096)\n"
    "...      + 0.5*np.cos(2*np.pi*250*t/4096)).astype(np.float32)\n"
    ">>> s = IMDMeasure(n=4096, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS\n"
    ">>> s.shape\n"
    "(8192,)\n"
    ">>> round(float(s.max()), 1)   # each tone splits into two images\n"
    "-12.0\n" },
  { "spectrum_dbfs_max_out", (PyCFunction)IMDMeasureObj_spectrum_dbfs_max_out,
    METH_NOARGS,
    "spectrum_dbfs_max_out() -> int\n\nMax output length spectrum_dbfs() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)IMDMeasureObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)IMDMeasureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Imdmeas be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Imdmeas\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)IMDMeasureObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Imdmeas.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
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
