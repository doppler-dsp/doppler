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

static PyStructSequence_Field IMDMeasureObj_analyze_fields[] = {
  { "f1", NULL },        { "f2", NULL },           { "p1_dbfs", NULL },
  { "p2_dbfs", NULL },   { "imd2_dbc", NULL },     { "imd3_dbc", NULL },
  { "imd2_freq", NULL }, { "imd3_lo_freq", NULL }, { "imd3_hi_freq", NULL },
  { "toi_dbfs", NULL },  { "soi_dbfs", NULL },     { "rbw_hz", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc IMDMeasureObj_analyze_desc
    = { "imdmeas.IMDMetrics", NULL, IMDMeasureObj_analyze_fields, 12 };
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

static PyMethodDef IMDMeasureObj_methods[]
    = { { "reset", (PyCFunction)IMDMeasureObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "analyze", (PyCFunction)IMDMeasureObj_analyze, METH_VARARGS,
          "analyze(x) -> IMDMetrics record (f1, f2, p1_dbfs, p2_dbfs, "
          "imd2_dbc, imd3_dbc, imd2_freq, imd3_lo_freq, imd3_hi_freq, "
          "toi_dbfs, soi_dbfs, rbw_hz)." },
        { "destroy", (PyCFunction)IMDMeasureObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)IMDMeasureObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)IMDMeasureObj_exit, METH_VARARGS, NULL },
        { NULL } };

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
