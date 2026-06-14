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

/* Named result (see measure_ext_tonemeas.c for the lazy-structseq rationale).
 */
static PyStructSequence_Field _npr_fields[]
    = { { "npr_db", "in-band / in-notch noise PSD ratio (dB)" },
        { "inband_psd_dbfs", "mean in-band noise power per bin (dBFS)" },
        { "notch_psd_dbfs", "mean power folded into the notch (dBFS)" },
        { "n_inband_bins", "bins averaged in the active band" },
        { "n_notch_bins", "bins averaged inside the notch" },
        { "rbw_hz", "resolution bandwidth (Hz)" },
        { NULL } };
static PyStructSequence_Desc _npr_desc
    = { "doppler.measure.NPRMetrics", "Noise Power Ratio result.", _npr_fields,
        6 };
static PyTypeObject *_NPRMetricsType = NULL;

static PyObject *
_build_npr_metrics (const npr_meas_t *r)
{
  if (!_NPRMetricsType)
    {
      _NPRMetricsType = PyStructSequence_NewType (&_npr_desc);
      if (!_NPRMetricsType)
        return NULL;
    }
  PyObject *o = PyStructSequence_New (_NPRMetricsType);
  if (!o)
    return NULL;
  int i = 0;
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->npr_db));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->inband_psd_dbfs));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->notch_psd_dbfs));
  PyStructSequence_SetItem (
      o, i++,
      PyLong_FromUnsignedLongLong ((unsigned long long)r->n_inband_bins));
  PyStructSequence_SetItem (
      o, i++,
      PyLong_FromUnsignedLongLong ((unsigned long long)r->n_notch_bins));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->rbw_hz));
  if (PyErr_Occurred ())
    {
      Py_DECREF (o);
      return NULL;
    }
  return o;
}

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
  const char *window_str = "kaiser";
  /* jm drops size_t init-param defaults (jm#244); apply them by hand. */
  unsigned long long n_raw      = 8192ULL;
  double             fs         = 1.0;
  float              beta       = 12.0f;
  unsigned long long pad_raw    = 2ULL;
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

static PyObject *
NPRMeasureObj_analyze (NPRMeasureObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  double    active_lo, active_hi, notch_lo, notch_hi, guard_hz;
  if (!PyArg_ParseTuple (args, "Oddddd", &in_obj, &active_lo, &active_hi,
                         &notch_lo, &notch_hi, &guard_hz))
    return NULL;
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t       n_in  = (size_t)PyArray_SIZE (in_arr);
  const float *_data = (const float *)PyArray_DATA (in_arr);
  npr_meas_t   r;
  Py_BEGIN_ALLOW_THREADS
    nprmeas_analyze (self->handle, _data, n_in, active_lo, active_hi, notch_lo,
                     notch_hi, guard_hz, &r, 1);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  return _build_npr_metrics (&r);
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

        { "analyze", (PyCFunction)NPRMeasureObj_analyze, METH_VARARGS,
          "analyze(x, active_lo, active_hi, notch_lo, notch_hi, guard_hz) -> "
          "NPRMetrics\n"
          "\n"
          "NPR of a notched-noise capture: the mean in-band noise PSD over\n"
          "[active_lo, active_hi] (Hz) divided by the mean PSD that folded "
          "into the\n"
          "notch [notch_lo, notch_hi], with a guard keep-out around the notch "
          "edges.\n"
          "Returns a named NPRMetrics result.\n" },
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
