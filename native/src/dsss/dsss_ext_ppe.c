/*
 * dsss_ext_ppe.c — PolynomialPhaseEstimator type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* PolynomialPhaseEstimatorObject — wraps ppe_state_t *       */
/* ======================================================== */

#include "ppe/ppe_core.h"

typedef struct
{
  PyObject_HEAD ppe_state_t *handle;
} PolynomialPhaseEstimatorObject;

static void
PolynomialPhaseEstimatorObj_dealloc (PolynomialPhaseEstimatorObject *self)
{
  if (self->handle)
    ppe_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PolynomialPhaseEstimatorObj_new (PyTypeObject *type, PyObject *args,
                                 PyObject *kwds)
{
  PolynomialPhaseEstimatorObject *self
      = (PolynomialPhaseEstimatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PolynomialPhaseEstimatorObj_init (PolynomialPhaseEstimatorObject *self,
                                  PyObject *args, PyObject *kwds)
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
PolynomialPhaseEstimatorObj_reset (PolynomialPhaseEstimatorObject *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ppe_reset (self->handle);
  Py_RETURN_NONE;
}

static PyStructSequence_Field PolynomialPhaseEstimatorObj_estimate_fields[] = {
  { "freq_norm", NULL },
  { "rate_norm", NULL },
  { "snr_db", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc PolynomialPhaseEstimatorObj_estimate_desc
    = { "doppler.dsss.PolynomialPhaseEstimate", NULL,
        PolynomialPhaseEstimatorObj_estimate_fields, 3 };
static PyTypeObject *PolynomialPhaseEstimatorObj_estimate_type = NULL;

static PyObject *
PolynomialPhaseEstimatorObj_estimate (PolynomialPhaseEstimatorObject *self,
                                      PyObject                       *args)
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
  if (!PolynomialPhaseEstimatorObj_estimate_type)
    {
      PolynomialPhaseEstimatorObj_estimate_type = PyStructSequence_NewType (
          &PolynomialPhaseEstimatorObj_estimate_desc);
      if (!PolynomialPhaseEstimatorObj_estimate_type)
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
  PyObject *_o
      = PyStructSequence_New (PolynomialPhaseEstimatorObj_estimate_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.freq_norm));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.rate_norm));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.snr_db));
  return _o;
}
static PyObject *
PolynomialPhaseEstimator_getprop_max_len (PolynomialPhaseEstimatorObject *self,
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
PolynomialPhaseEstimator_getprop_nfft (PolynomialPhaseEstimatorObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
PolynomialPhaseEstimator_getprop_max_rate (
    PolynomialPhaseEstimatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->max_rate);
}
static PyObject *
PolynomialPhaseEstimator_getprop_n_rate (PolynomialPhaseEstimatorObject *self,
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

static PyGetSetDef PolynomialPhaseEstimator_getset[]
    = { { "max_len", (getter)PolynomialPhaseEstimator_getprop_max_len, NULL,
          "Max len.\n", NULL },
        { "nfft", (getter)PolynomialPhaseEstimator_getprop_nfft, NULL,
          "Nfft.\n", NULL },
        { "max_rate", (getter)PolynomialPhaseEstimator_getprop_max_rate, NULL,
          "Max rate.\n", NULL },
        { "n_rate", (getter)PolynomialPhaseEstimator_getprop_n_rate, NULL,
          "N rate.\n", NULL },
        { NULL } };

static PyObject *
PolynomialPhaseEstimatorObj_destroy (PolynomialPhaseEstimatorObject *self,
                                     PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ppe_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PolynomialPhaseEstimatorObj_enter (PolynomialPhaseEstimatorObject *self,
                                   PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PolynomialPhaseEstimatorObj_exit (PolynomialPhaseEstimatorObject *self,
                                  PyObject                       *args)
{
  (void)args;
  if (self->handle)
    {
      ppe_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PolynomialPhaseEstimatorObj_methods[] = {
  { "reset", (PyCFunction)PolynomialPhaseEstimatorObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "estimate", (PyCFunction)PolynomialPhaseEstimatorObj_estimate,
    METH_VARARGS,
    "estimate(x) -> PolynomialPhaseEstimate record (freq_norm, rate_norm, "
    "snr_db)." },
  { "destroy", (PyCFunction)PolynomialPhaseEstimatorObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)PolynomialPhaseEstimatorObj_enter, METH_NOARGS,
    NULL },
  { "__exit__", (PyCFunction)PolynomialPhaseEstimatorObj_exit, METH_VARARGS,
    NULL },
  { NULL }
};

static PyTypeObject PolynomialPhaseEstimatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.PolynomialPhaseEstimator",
  .tp_basicsize = sizeof (PolynomialPhaseEstimatorObject),
  .tp_dealloc   = (destructor)PolynomialPhaseEstimatorObj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc       = "Create a polynomial-phase estimator.\n",
  .tp_methods   = PolynomialPhaseEstimatorObj_methods,
  .tp_getset    = PolynomialPhaseEstimator_getset,
  .tp_new       = PolynomialPhaseEstimatorObj_new,
  .tp_init      = (initproc)PolynomialPhaseEstimatorObj_init,
};
