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

/* Named result (see measure_ext_tonemeas.c for the lazy-structseq rationale).
 */
static PyStructSequence_Field _imd_fields[]
    = { { "f1", "lower tone frequency (Hz)" },
        { "f2", "upper tone frequency (Hz)" },
        { "p1_dbfs", "lower tone level (dBFS)" },
        { "p2_dbfs", "upper tone level (dBFS)" },
        { "imd2_dbc", "2nd-order product (f2-f1) vs mean tone (dBc)" },
        { "imd3_dbc", "worst 3rd-order product vs mean tone (dBc)" },
        { "imd2_freq", "2nd-order product frequency (Hz)" },
        { "imd3_lo_freq", "2f1-f2 product frequency (Hz)" },
        { "imd3_hi_freq", "2f2-f1 product frequency (Hz)" },
        { "toi_dbfs", "third-order intercept (dBFS)" },
        { "soi_dbfs", "second-order intercept (dBFS)" },
        { "rbw_hz", "resolution bandwidth (Hz)" },
        { NULL } };
static PyStructSequence_Desc _imd_desc
    = { "doppler.measure.IMDMetrics", "Two-tone intermodulation result.",
        _imd_fields, 12 };
static PyTypeObject *_IMDMetricsType = NULL;

static PyObject *
_build_imd_metrics (const imd_meas_t *r)
{
  if (!_IMDMetricsType)
    {
      _IMDMetricsType = PyStructSequence_NewType (&_imd_desc);
      if (!_IMDMetricsType)
        return NULL;
    }
  PyObject *o = PyStructSequence_New (_IMDMetricsType);
  if (!o)
    return NULL;
  int i = 0;
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->f1));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->f2));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->p1_dbfs));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->p2_dbfs));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->imd2_dbc));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->imd3_dbc));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->imd2_freq));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->imd3_lo_freq));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->imd3_hi_freq));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->toi_dbfs));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->soi_dbfs));
  PyStructSequence_SetItem (o, i++, PyFloat_FromDouble (r->rbw_hz));
  if (PyErr_Occurred ())
    {
      Py_DECREF (o);
      return NULL;
    }
  return o;
}

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
  self->handle = imdmeas_create (n, fs, window, beta, pad, full_scale);
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
  size_t       n_in  = (size_t)PyArray_SIZE (in_arr);
  const float *_data = (const float *)PyArray_DATA (in_arr);
  imd_meas_t   r;
  Py_BEGIN_ALLOW_THREADS
    imdmeas_analyze (self->handle, _data, n_in, &r, 1);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  return _build_imd_metrics (&r);
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
    "analyze(x) -> IMDMetrics\n"
    "\n"
    "Two-tone IMD/TOI of a real capture: finds the two strongest tones, then\n"
    "integrates the IMD2 (f2-f1) and IMD3 (2f1-f2, 2f2-f1) products over "
    "their\n"
    "window main lobes.  Returns a named IMDMetrics result.\n" },
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
  .tp_doc                                 = "Create an IMDMeasure analyser.\n",
  .tp_methods                             = IMDMeasureObj_methods,
  .tp_getset                              = IMDMeasure_getset,
  .tp_new                                 = IMDMeasureObj_new,
  .tp_init                                = (initproc)IMDMeasureObj_init,
};
