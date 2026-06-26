/*
 * dsss_ext_acq.c — Acquirer type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AcquirerObject — wraps acq_state_t *       */
/* ======================================================== */

#include "acq/acq_core.h"

typedef struct
{
  PyObject_HEAD acq_state_t *handle;
} AcquirerObject;

static void
AcquirerObj_dealloc (AcquirerObject *self)
{
  if (self->handle)
    acq_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AcquirerObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AcquirerObject *self = (AcquirerObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AcquirerObj_init (AcquirerObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "code", "noise_mode", "sf",      "spc",       "ny",
          "pfa",  "pd",         "min_snr", "max_dwell", NULL };
  PyObject          *code_obj       = NULL;
  const char        *noise_mode_str = "mean";
  unsigned long long sf_raw         = 1;
  unsigned long long spc_raw        = 1;
  unsigned long long ny_raw         = 16;
  double             pfa            = 1e-3;
  double             pd             = 0.9;
  double             min_snr        = 0.1;
  unsigned long long max_dwell_raw  = 64;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|sKKKdddK", kwlist, &code_obj, &noise_mode_str,
          &sf_raw, &spc_raw, &ny_raw, &pfa, &pd, &min_snr, &max_dwell_raw))
    return -1;
  int noise_mode = 0;
  if (strcmp (noise_mode_str, "mean") == 0)
    noise_mode = 0;
  else if (strcmp (noise_mode_str, "median") == 0)
    noise_mode = 1;
  else if (strcmp (noise_mode_str, "min") == 0)
    noise_mode = 2;
  else if (strcmp (noise_mode_str, "max") == 0)
    noise_mode = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "noise_mode must be one of \"mean\", \"median\", \"min\", "
                    "\"max\", got '%s'",
                    noise_mode_str);
      return -1;
    }
  size_t         sf        = (size_t)sf_raw;
  size_t         spc       = (size_t)spc_raw;
  size_t         ny        = (size_t)ny_raw;
  size_t         max_dwell = (size_t)max_dwell_raw;
  PyArrayObject *code_arr  = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle
      = acq_create ((const uint8_t *)PyArray_DATA (code_arr), code_len, sf,
                    spc, ny, pfa, pd, min_snr, noise_mode, max_dwell);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acq_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AcquirerObj_reset (AcquirerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  acq_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AcquirerObj_push (AcquirerObject *self, PyObject *args)
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
  size_t       n_in = (size_t)PyArray_SIZE (in_arr);
  acq_result_t results[64];
  size_t       n_out
      = acq_push (self->handle, (const float complex *)PyArray_DATA (in_arr),
                  n_in, results, 64);
  Py_DECREF (in_arr);
  PyObject *lst = PyList_New ((Py_ssize_t)n_out);
  if (!lst)
    return NULL;
  for (size_t i = 0; i < n_out; i++)
    {
      PyObject *tup = Py_BuildValue (
          "(KKffff)", (unsigned long long)results[i].doppler_bin,
          (unsigned long long)results[i].code_phase, results[i].peak_mag,
          results[i].noise_est, results[i].test_stat, results[i].snr_est);
      if (!tup)
        {
          Py_DECREF (lst);
          return NULL;
        }
      PyList_SET_ITEM (lst, (Py_ssize_t)i, tup);
    }
  return lst;
}
static PyObject *
Acquirer_getprop_ny (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
Acquirer_getprop_nx (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
Acquirer_getprop_n (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
Acquirer_getprop_sf (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->sf);
}
static PyObject *
Acquirer_getprop_spc (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->spc);
}
static PyObject *
Acquirer_getprop_dwell (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
Acquirer_getprop_max_dwell (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->max_dwell);
}
static PyObject *
Acquirer_getprop_ring_cap (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->ring_cap);
}
static PyObject *
Acquirer_getprop_noise_lo (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->noise_lo);
}
static PyObject *
Acquirer_getprop_noise_hi (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->noise_hi);
}
static PyObject *
Acquirer_getprop_threshold (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->threshold);
}
static PyObject *
Acquirer_getprop_eta (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->eta);
}
static PyObject *
Acquirer_getprop_pfa_cell (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->pfa_cell);
}
static PyObject *
Acquirer_getprop_pd_predicted (AcquirerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->pd_predicted);
}

static PyGetSetDef Acquirer_getset[] = {
  { "ny", (getter)Acquirer_getprop_ny, NULL, "Ny.\n", NULL },
  { "nx", (getter)Acquirer_getprop_nx, NULL, "Nx.\n", NULL },
  { "n", (getter)Acquirer_getprop_n, NULL, "N.\n", NULL },
  { "sf", (getter)Acquirer_getprop_sf, NULL, "Sf.\n", NULL },
  { "spc", (getter)Acquirer_getprop_spc, NULL, "Spc.\n", NULL },
  { "dwell", (getter)Acquirer_getprop_dwell, NULL, "Dwell.\n", NULL },
  { "max_dwell", (getter)Acquirer_getprop_max_dwell, NULL, "Max dwell.\n",
    NULL },
  { "ring_cap", (getter)Acquirer_getprop_ring_cap, NULL, "Ring cap.\n", NULL },
  { "noise_lo", (getter)Acquirer_getprop_noise_lo, NULL, "Noise lo.\n", NULL },
  { "noise_hi", (getter)Acquirer_getprop_noise_hi, NULL, "Noise hi.\n", NULL },
  { "threshold", (getter)Acquirer_getprop_threshold, NULL, "Threshold.\n",
    NULL },
  { "eta", (getter)Acquirer_getprop_eta, NULL, "Eta.\n", NULL },
  { "pfa_cell", (getter)Acquirer_getprop_pfa_cell, NULL, "Pfa cell.\n", NULL },
  { "pd_predicted", (getter)Acquirer_getprop_pd_predicted, NULL,
    "Pd predicted.\n", NULL },
  { NULL }
};

static PyObject *
AcquirerObj_destroy (AcquirerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquirerObj_enter (AcquirerObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AcquirerObj_exit (AcquirerObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AcquirerObj_methods[]
    = { { "reset", (PyCFunction)AcquirerObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "push", (PyCFunction)AcquirerObj_push, METH_VARARGS,
          "push(x) -> list[tuple]\n"
          "\n"
          "Returns list of (doppler_bin, code_phase, peak_mag, noise_est, "
          "test_stat, snr_est,) tuples.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import Acquirer\n"
          "    >>> obj = Acquirer(np.zeros(1, dtype=np.uint8), \"mean\", 1, "
          "1, 16, 1e-3, 0.9, 0.1, 64)\n"
          "    >>> results = obj.push(np.zeros(4, dtype=np.complex64))\n"
          "    >>> isinstance(results, list)\n"
          "    True\n" },
        { "destroy", (PyCFunction)AcquirerObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)AcquirerObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)AcquirerObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject AcquirerObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.Acquirer",
  .tp_basicsize                           = sizeof (AcquirerObject),
  .tp_dealloc                             = (destructor)AcquirerObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a streaming DSSS acquisition engine.\n",
  .tp_methods = AcquirerObj_methods,
  .tp_getset  = Acquirer_getset,
  .tp_new     = AcquirerObj_new,
  .tp_init    = (initproc)AcquirerObj_init,
};
