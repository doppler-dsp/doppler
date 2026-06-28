/*
 * dsss_ext_ppe.c — PolyPhaseEstimator type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* PolyPhaseEstimatorObject — wraps ppe_state_t *       */
/* ======================================================== */

#include "ppe/ppe_core.h"

typedef struct
{
  PyObject_HEAD ppe_state_t *handle;
} PolyPhaseEstimatorObject;

static void
PolyPhaseEstimatorObj_dealloc (PolyPhaseEstimatorObject *self)
{
  if (self->handle)
    ppe_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PolyPhaseEstimatorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PolyPhaseEstimatorObject *self
      = (PolyPhaseEstimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PolyPhaseEstimatorObj_init (PolyPhaseEstimatorObject *self, PyObject *args,
                            PyObject *kwds)
{
  static char       *kwlist[]    = { "max_len", "max_rate", NULL };
  unsigned long long max_len_raw = 4096;
  double             max_rate    = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kd", kwlist, &max_len_raw,
                                    &max_rate))
    return -1;
  size_t max_len = (size_t)max_len_raw;
  self->handle   = ppe_create (max_len, max_rate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "ppe_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
PolyPhaseEstimatorObj_reset (PolyPhaseEstimatorObject *self,
                             PyObject                 *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ppe_reset (self->handle);
  Py_RETURN_NONE;
}

static PyStructSequence_Field PolyPhaseEstimatorObj_estimate_fields[] = {
  { "freq_norm", NULL },
  { "rate_norm", NULL },
  { "snr_db", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc PolyPhaseEstimatorObj_estimate_desc
    = { "doppler.dsss.PolyPhaseEstimate", NULL,
        PolyPhaseEstimatorObj_estimate_fields, 3 };
static PyTypeObject *PolyPhaseEstimatorObj_estimate_type = NULL;

static PyObject *
PolyPhaseEstimatorObj_estimate (PolyPhaseEstimatorObject *self, PyObject *args)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  size_t n_in = (size_t)PyArray_SIZE (in_arr);
  if (!PolyPhaseEstimatorObj_estimate_type)
    {
      PolyPhaseEstimatorObj_estimate_type
          = PyStructSequence_NewType (&PolyPhaseEstimatorObj_estimate_desc);
      if (!PolyPhaseEstimatorObj_estimate_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (in_arr);
  ppe_result_t         _r;
  Py_BEGIN_ALLOW_THREADS
    _r = ppe_estimate (self->handle, _ng0, n_in);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (PolyPhaseEstimatorObj_estimate_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.freq_norm));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.rate_norm));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.snr_db));
  return _o;
}
static PyObject *
PolyPhaseEstimator_getprop_max_len (PolyPhaseEstimatorObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->max_len);
}
static PyObject *
PolyPhaseEstimator_getprop_nfft (PolyPhaseEstimatorObject *self,
                                 void                     *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
PolyPhaseEstimator_getprop_max_rate (PolyPhaseEstimatorObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->max_rate);
}
static PyObject *
PolyPhaseEstimator_getprop_n_rate (PolyPhaseEstimatorObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_rate);
}

static PyGetSetDef PolyPhaseEstimator_getset[]
    = { { "max_len", (getter)PolyPhaseEstimator_getprop_max_len, NULL,
          "Max len.\n", NULL },
        { "nfft", (getter)PolyPhaseEstimator_getprop_nfft, NULL, "Nfft.\n",
          NULL },
        { "max_rate", (getter)PolyPhaseEstimator_getprop_max_rate, NULL,
          "Max rate.\n", NULL },
        { "n_rate", (getter)PolyPhaseEstimator_getprop_n_rate, NULL,
          "N rate.\n", NULL },
        { NULL } };

static PyObject *
PolyPhaseEstimatorObj_destroy (PolyPhaseEstimatorObject *self,
                               PyObject                 *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ppe_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PolyPhaseEstimatorObj_enter (PolyPhaseEstimatorObject *self,
                             PyObject                 *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PolyPhaseEstimatorObj_exit (PolyPhaseEstimatorObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ppe_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PolyPhaseEstimatorObj_methods[] = {
  { "reset", (PyCFunction)PolyPhaseEstimatorObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "estimate", (PyCFunction)PolyPhaseEstimatorObj_estimate, METH_VARARGS,
    "estimate(x) -> PolyPhaseEstimate record (freq_norm, rate_norm, "
    "snr_db)." },
  { "destroy", (PyCFunction)PolyPhaseEstimatorObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)PolyPhaseEstimatorObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)PolyPhaseEstimatorObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject PolyPhaseEstimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.PolyPhaseEstimator",
  .tp_basicsize                           = sizeof (PolyPhaseEstimatorObject),
  .tp_dealloc = (destructor)PolyPhaseEstimatorObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a polynomial-phase estimator.\n",
  .tp_methods = PolyPhaseEstimatorObj_methods,
  .tp_getset  = PolyPhaseEstimator_getset,
  .tp_new     = PolyPhaseEstimatorObj_new,
  .tp_init    = (initproc)PolyPhaseEstimatorObj_init,
};
