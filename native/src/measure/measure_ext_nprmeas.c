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
      = { "window", "n", "fs", "beta", "pad", "full_scale", NULL };
  const char        *window_str = "kaiser";
  unsigned long long n_raw      = 8192;
  double             fs         = 1.0;
  float              beta       = 12.0f;
  unsigned long long pad_raw    = 2;
  double             full_scale = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sKdfKd", kwlist, &window_str,
                                    &n_raw, &fs, &beta, &pad_raw, &full_scale))
    return -1;
  int window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", got '%s'",
                    window_str);
      return -1;
    }
  size_t n     = (size_t)n_raw;
  size_t pad   = (size_t)pad_raw;
  self->handle = nprmeas_create (n, fs, window, beta, pad, full_scale);
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
    = { "nprmeas.NPRMetrics", NULL, NPRMeasureObj_analyze_fields, 6 };
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

static PyMethodDef NPRMeasureObj_methods[]
    = { { "reset", (PyCFunction)NPRMeasureObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "analyze", (PyCFunction)(void *)NPRMeasureObj_analyze,
          METH_VARARGS | METH_KEYWORDS,
          "analyze(x, active_lo, active_hi, notch_lo, notch_hi, guard_hz) -> "
          "NPRMetrics record (npr_db, inband_psd_dbfs, notch_psd_dbfs, "
          "n_inband_bins, n_notch_bins, rbw_hz)." },
        { "destroy", (PyCFunction)NPRMeasureObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)NPRMeasureObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)NPRMeasureObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject NPRMeasureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "measure.NPRMeasure",
  .tp_basicsize                           = sizeof (NPRMeasureObject),
  .tp_dealloc                             = (destructor)NPRMeasureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an NPRMeasure analyser.\n",
  .tp_methods                             = NPRMeasureObj_methods,
  .tp_getset                              = NPRMeasure_getset,
  .tp_new                                 = NPRMeasureObj_new,
  .tp_init                                = (initproc)NPRMeasureObj_init,
};
