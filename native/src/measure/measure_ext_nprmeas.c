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
} NPRMeasureObject;

static void
NPRMeasureObj_dealloc (NPRMeasureObject *self)
{
  if (self->handle)
    nprmeas_destroy (self->handle);
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
  static char *_kwlist[] = { "x",        "active_lo", "active_hi", "notch_lo",
                             "notch_hi", "guard_hz",  NULL };
  PyObject    *x_obj     = NULL;
  double       active_lo = 0.0;
  double       active_hi = 0.0;
  double       notch_lo  = 0.0;
  double       notch_hi  = 0.0;
  double       guard_hz  = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Odddd|d", _kwlist, &x_obj,
                                    &active_lo, &active_hi, &notch_lo,
                                    &notch_hi, &guard_hz))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float *x     = (const float *)PyArray_DATA (x_arr);
  size_t       x_len = (size_t)PyArray_SIZE (x_arr);
  if (!NPRMeasureObj_analyze_type)
    {
      NPRMeasureObj_analyze_type
          = PyStructSequence_NewType (&NPRMeasureObj_analyze_desc);
      if (!NPRMeasureObj_analyze_type)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  npr_meas_t _r;
  Py_BEGIN_ALLOW_THREADS
    _r = nprmeas_analyze (self->handle, x, x_len, active_lo, active_hi,
                          notch_lo, notch_hi, guard_hz);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
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
  size_t _cap  = nprmeas_spectrum_dbfs_max_out (self->handle);
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
  size_t n_out = nprmeas_spectrum_dbfs (
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
    = { { "n", (getter)NPRMeasure_getprop_n, NULL,
          "Window / frame length (samples).\n", NULL },
        { "nfft", (getter)NPRMeasure_getprop_nfft, NULL,
          "Zero-padded transform length.\n", NULL },
        { "fs", (getter)NPRMeasure_getprop_fs, NULL, "Sample rate, Hz.\n",
          NULL },
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
    "Reset the analyser (a no-op: each analyze() call is independent)." },

  { "analyze", (PyCFunction)(void *)NPRMeasureObj_analyze,
    METH_VARARGS | METH_KEYWORDS,
    "analyze(x, active_lo, active_hi, notch_lo, notch_hi, guard_hz) -> "
    "NPRMetrics record (npr_db, inband_psd_dbfs, notch_psd_dbfs, "
    "n_inband_bins, n_notch_bins, rbw_hz)\n"
    "\n"
    "NPR of a notched-noise capture over [active_lo,active_hi] with a\n"
    "notch [notch_lo,notch_hi] (Hz) and guard keep-out.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Real time-domain capture.\n"
    "active_lo : float\n"
    "    Active noise band lower edge (Hz).\n"
    "active_hi : float\n"
    "    Active noise band upper edge (Hz).\n"
    "notch_lo : float\n"
    "    Notch lower edge (Hz).\n"
    "notch_hi : float\n"
    "    Notch upper edge (Hz).\n"
    "guard_hz : float\n"
    "    Keep-out around the notch edges (Hz).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NPRMetrics\n"
    "    the NPR metric record (by value).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.measure import NPRMeasure\n"
    ">>> import numpy as np\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> n = 1 << 15\n"
    ">>> F = np.fft.rfft(rng.standard_normal(n))\n"
    ">>> f = np.fft.rfftfreq(n)\n"
    ">>> F[(f < 0.05) | (f > 0.45)] = 0  # band-limit to [0.05,0.45]\n"
    ">>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep\n"
    ">>> x = np.fft.irfft(F, n)\n"
    ">>> x = (0.3*x/np.std(x)).astype(np.float32)\n"
    ">>> r = NPRMeasure(n=n, fs=1.0).analyze(\n"
    "...     x, 0.05, 0.45, 0.20, 0.25, 0.01)\n"
    ">>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs\n"
    "(True, True)\n" },
  { "spectrum_dbfs", (PyCFunction)(void *)NPRMeasureObj_spectrum_dbfs,
    METH_VARARGS | METH_KEYWORDS,
    "spectrum_dbfs(x) -> ndarray\n"
    "\n"
    "DC-centred dBFS magnitude spectrum of a capture (length nfft, for "
    "plots).\n"
    "\n"
    "The same windowed, zero-padded PSD the NPR metrics are read off, laid\n"
    "out DC-centred (fftshifted) and normalised to dBFS for an\n"
    "analyzer-display backdrop. Use it to see the notch and the active band\n"
    "that analyze() integrates over.\n"
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
    ">>> from doppler.measure import NPRMeasure\n"
    ">>> import numpy as np\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> x = (0.3*rng.standard_normal(8192)).astype(np.float32)  # noise\n"
    ">>> s = NPRMeasure(n=8192, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS\n"
    ">>> s.shape                                          # zero-padded nfft\n"
    "(16384,)\n"
    ">>> round(float(np.median(s)), 0)   # broadband floor, below 0 dBFS\n"
    "-48.0\n" },
  { "spectrum_dbfs_max_out", (PyCFunction)NPRMeasureObj_spectrum_dbfs_max_out,
    METH_NOARGS,
    "spectrum_dbfs_max_out() -> int\n\nMax output length spectrum_dbfs() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)NPRMeasureObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)NPRMeasureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a NPRMeasure be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NPRMeasure\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)NPRMeasureObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the NPRMeasure.\n"
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
