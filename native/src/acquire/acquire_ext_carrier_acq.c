/*
 * acquire_ext_carrier_acq.c — CarrierAcquisition type for the acquire module.
 *
 * Included by acquire_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only acquire_ext.c is compiled.
 */
/* ======================================================== */
/* CarrierAcquisitionObject — wraps carrier_acq_state_t *       */
/* ======================================================== */

#include "carrier_acq/carrier_acq_core.h"

typedef struct
{
  PyObject_HEAD carrier_acq_state_t *handle;
} CarrierAcquisitionObject;

static void
CarrierAcquisitionObj_dealloc (CarrierAcquisitionObject *self)
{
  if (self->handle)
    carrier_acq_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CarrierAcquisitionObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CarrierAcquisitionObject *self
      = (CarrierAcquisitionObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CarrierAcquisitionObj_init (CarrierAcquisitionObject *self, PyObject *args,
                            PyObject *kwds)
{
  static char       *kwlist[]         = { "psd_template",
                                          "sample_rate_hz",
                                          "symbol_rate_hz",
                                          "resolution_hz",
                                          "zero_pad",
                                          "window",
                                          "beta",
                                          "pfa",
                                          "pd",
                                          "design_snr",
                                          "sequential",
                                          "max_n_blocks",
                                          NULL };
  PyObject          *psd_template_obj = NULL;
  double             sample_rate_hz   = 0.0;
  double             symbol_rate_hz   = 0.0;
  double             resolution_hz    = 0.0;
  unsigned long long zero_pad_raw     = 4;
  const char        *window_str       = "hann";
  float              beta             = 0.0f;
  double             pfa              = 1e-3;
  double             pd               = 0.9;
  double             design_snr       = 2.0;
  int                sequential_raw   = true;
  unsigned long long max_n_blocks_raw = 100000;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|dddKsfdddpK", kwlist, &psd_template_obj,
          &sample_rate_hz, &symbol_rate_hz, &resolution_hz, &zero_pad_raw,
          &window_str, &beta, &pfa, &pd, &design_snr, &sequential_raw,
          &max_n_blocks_raw))
    return -1;
  size_t zero_pad = (size_t)zero_pad_raw;
  int    window   = 0;
  if (strcmp (window_str, "hann") == 0)
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else if (strcmp (window_str, "blackman-harris") == 0)
    window = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", "
                    "\"blackman-harris\", got '%s'",
                    window_str);
      return -1;
    }
  bool           sequential       = (int)sequential_raw;
  size_t         max_n_blocks     = (size_t)max_n_blocks_raw;
  PyArrayObject *psd_template_arr = (PyArrayObject *)PyArray_FROM_OTF (
      psd_template_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!psd_template_arr)
    {
      return -1;
    }
  size_t psd_template_len = (size_t)PyArray_SIZE (psd_template_arr);
  self->handle            = carrier_acq_create (
      sample_rate_hz, symbol_rate_hz, resolution_hz, zero_pad, window, beta,
      (const float *)PyArray_DATA (psd_template_arr), psd_template_len, pfa,
      pd, design_snr, sequential, max_n_blocks);
  Py_DECREF (psd_template_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "carrier_acq_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CarrierAcquisitionObj_steps (CarrierAcquisitionObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float complex *x     = (const float complex *)PyArray_DATA (x_arr);
  size_t               x_len = (size_t)PyArray_SIZE (x_arr);
  carrier_acq_steps (self->handle, x, x_len);
  Py_DECREF (x_arr);
  Py_RETURN_NONE;
}

static PyObject *
CarrierAcquisitionObj_reset (CarrierAcquisitionObject *self,
                             PyObject                 *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  carrier_acq_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CarrierAcquisitionObj_state_bytes (CarrierAcquisitionObject *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_acq_state_bytes (self->handle));
}

static PyObject *
CarrierAcquisitionObj_get_state (CarrierAcquisitionObject *self,
                                 PyObject                 *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = carrier_acq_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  carrier_acq_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CarrierAcquisitionObj_set_state (CarrierAcquisitionObject *self, PyObject *arg)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!PyBytes_Check (arg))
    {
      PyErr_SetString (PyExc_TypeError, "set_state expects bytes");
      return NULL;
    }
  if ((size_t)PyBytes_GET_SIZE (arg) != carrier_acq_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (carrier_acq_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CarrierAcquisition_getprop_ready (CarrierAcquisitionObject *self,
                                  void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->ready));
}
static PyObject *
CarrierAcquisition_getprop_residual_hz (CarrierAcquisitionObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->residual_hz);
}
static PyObject *
CarrierAcquisition_getprop_n_blocks (CarrierAcquisitionObject *self,
                                     void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_blocks);
}
static PyObject *
CarrierAcquisition_getprop_dwell_target (CarrierAcquisitionObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->dwell_target);
}
static PyObject *
CarrierAcquisition_getprop_max_n_blocks (CarrierAcquisitionObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->max_n_blocks);
}
static PyObject *
CarrierAcquisition_getprop_nfft (CarrierAcquisitionObject *self,
                                 void                     *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}

static PyGetSetDef CarrierAcquisition_getset[] = {
  { "ready", (getter)CarrierAcquisition_getprop_ready, NULL,
    "True once a detection has fired (or the dwell_target give-up cap was "
    "reached) -- residual_hz is only meaningful once this is true.\n",
    NULL },
  { "residual_hz", (getter)CarrierAcquisition_getprop_residual_hz, NULL,
    "Sub-bin-refined residual carrier frequency estimate, Hz. Valid only when "
    "ready is true.\n",
    NULL },
  { "n_blocks", (getter)CarrierAcquisition_getprop_n_blocks, NULL,
    "Number of n_fft-length blocks actually folded into the PSD average so "
    "far.\n",
    NULL },
  { "dwell_target", (getter)CarrierAcquisition_getprop_dwell_target, NULL,
    "Non-sequential mode's precomputed fixed wait count, from "
    "det_n_noncoh(design_snr, ...) at construction. Ignored by sequential "
    "mode's own give-up bound -- see max_n_blocks.\n",
    NULL },
  { "max_n_blocks", (getter)CarrierAcquisition_getprop_max_n_blocks, NULL,
    "Sequential mode's own give-up cap (independent of dwell_target) -- the "
    "max_n_blocks constructor argument, echoed back.\n",
    NULL },
  { "nfft", (getter)CarrierAcquisition_getprop_nfft, NULL,
    "PSD transform length (next_pow2(n_fft*zero_pad)) -- the length any "
    "caller-supplied template array must match.\n",
    NULL },
  { NULL }
};

static PyObject *
CarrierAcquisitionObj_destroy (CarrierAcquisitionObject *self,
                               PyObject                 *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      carrier_acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierAcquisitionObj_enter (CarrierAcquisitionObject *self,
                             PyObject                 *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CarrierAcquisitionObj_exit (CarrierAcquisitionObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      carrier_acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CarrierAcquisitionObj_methods[] = {

  { "steps", (PyCFunction)(void *)CarrierAcquisitionObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> None\n"
    "\n"
    "Fold raw complex samples into the running PSD average and test for a "
    "detection; any chunk size across repeated calls (a partial trailing "
    "block carries to the next call).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CarrierAcquisition\n"
    "    >>> obj = CarrierAcquisition(np.zeros(1, dtype=np.float32), .0, .0, "
    "0.0, 4, \"hann\", 0.0, 1e-3, 0.9, 2.0, true, 100000)\n"
    "    >>> obj.steps(np.zeros(4, dtype=np.complex64))\n" },
  { "reset", (PyCFunction)CarrierAcquisitionObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Discard the running PSD average and detection state; counters return to "
    "zero.\n"
    "\n"
    "    >>> from doppler import CarrierAcquisition\n"
    "    >>> obj = CarrierAcquisition(np.zeros(1, dtype=np.float32), .0, .0, "
    "0.0, 4, \"hann\", 0.0, 1e-3, 0.9, 2.0, true, 100000)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)CarrierAcquisitionObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)CarrierAcquisitionObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)CarrierAcquisitionObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)CarrierAcquisitionObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CarrierAcquisitionObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CarrierAcquisitionObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CarrierAcquisitionObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "acquire.CarrierAcquisition",
  .tp_basicsize                           = sizeof (CarrierAcquisitionObject),
  .tp_dealloc = (destructor)CarrierAcquisitionObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "CarrierAcquisition type.\n",
  .tp_methods = CarrierAcquisitionObj_methods,
  .tp_getset  = CarrierAcquisition_getset,
  .tp_new     = CarrierAcquisitionObj_new,
  .tp_init    = (initproc)CarrierAcquisitionObj_init,
};
