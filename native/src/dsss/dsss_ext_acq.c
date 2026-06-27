/*
 * dsss_ext_acq.c — Acquisition type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* AcquisitionObject — wraps acq_state_t *       */
/* ======================================================== */

#include "acq/acq_core.h"

typedef struct
{
  PyObject_HEAD acq_state_t *handle;
} AcquisitionObject;

static void
AcquisitionObj_dealloc (AcquisitionObject *self)
{
  if (self->handle)
    acq_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AcquisitionObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AcquisitionObject *self = (AcquisitionObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AcquisitionObj_init (AcquisitionObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "code",     "noise_mode",          "reps", "spc", "chip_rate",
          "cn0_dbhz", "doppler_uncertainty", "pfa",  "pd",  "max_noncoh",
          NULL };
  PyObject          *code_obj            = NULL;
  const char        *noise_mode_str      = "mean";
  unsigned long long reps_raw            = 1;
  unsigned long long spc_raw             = 4;
  double             chip_rate           = 1000000.0;
  double             cn0_dbhz            = 50.0;
  double             doppler_uncertainty = 0.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  unsigned long long max_noncoh_raw      = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|sKKdddddK", kwlist, &code_obj, &noise_mode_str,
          &reps_raw, &spc_raw, &chip_rate, &cn0_dbhz, &doppler_uncertainty,
          &pfa, &pd, &max_noncoh_raw))
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
  size_t         reps       = (size_t)reps_raw;
  size_t         spc        = (size_t)spc_raw;
  size_t         max_noncoh = (size_t)max_noncoh_raw;
  PyArrayObject *code_arr   = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = acq_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, reps, spc, chip_rate,
      cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode, max_noncoh);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "acq_create returned NULL");
      return -1;
    }
  /* Hand-patch (sacred fragment): C cannot raise a Python warning, so surface
   * an under-powered search here — the auto-config built a best-effort grid
   * whose pd_predicted falls short of the requested pd. */
  if (self->handle->underpowered)
    {
      if (PyErr_WarnEx (
              PyExc_UserWarning,
              "Acquisition is under-powered: pd_predicted < pd at this "
              "reps/cn0_dbhz. Raise reps or cn0_dbhz, set max_noncoh>1, "
              "or narrow doppler_uncertainty.",
              1)
          < 0)
        return -1;
    }
  return 0;
}

static PyObject *
AcquisitionObj_reset (AcquisitionObject *self, PyObject *Py_UNUSED (ignored))
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
AcquisitionObj_push (AcquisitionObject *self, PyObject *args)
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
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (in_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = acq_push (self->handle, _ng0, n_in, results, 64);
  Py_END_ALLOW_THREADS
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
Acquisition_getprop_code_bins (AcquisitionObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->code_bins);
}
static PyObject *
Acquisition_getprop_doppler_bins (AcquisitionObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->doppler_bins);
}
static PyObject *
Acquisition_getprop_sf (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->sf);
}
static PyObject *
Acquisition_getprop_spc (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->spc);
}
static PyObject *
Acquisition_getprop_reps (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->reps);
}
static PyObject *
Acquisition_getprop_n_noncoh (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_noncoh);
}
static PyObject *
Acquisition_getprop_ring_cap (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
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
Acquisition_getprop_noise_lo (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
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
Acquisition_getprop_noise_hi (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
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
Acquisition_getprop_threshold (AcquisitionObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->threshold);
}
static PyObject *
Acquisition_getprop_eta (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->eta);
}
static PyObject *
Acquisition_getprop_eta_nc (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->eta_nc);
}
static PyObject *
Acquisition_getprop_pfa_cell (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->pfa_cell);
}
static PyObject *
Acquisition_getprop_pd_predicted (AcquisitionObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->pd_predicted);
}
static PyObject *
Acquisition_getprop_fs (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
Acquisition_getprop_chip_rate (AcquisitionObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->chip_rate);
}
static PyObject *
Acquisition_getprop_cn0_dbhz (AcquisitionObject *self,
                              void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->cn0_dbhz);
}
static PyObject *
Acquisition_getprop_doppler_span_hz (AcquisitionObject *self,
                                     void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_span_hz);
}
static PyObject *
Acquisition_getprop_doppler_res_hz (AcquisitionObject *self,
                                    void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_res_hz);
}
static PyObject *
Acquisition_getprop_pd (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->pd);
}
static PyObject *
Acquisition_getprop_underpowered (AcquisitionObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->underpowered));
}

static PyGetSetDef Acquisition_getset[] = {
  { "code_bins", (getter)Acquisition_getprop_code_bins, NULL,
    "Code-phase hypotheses searched (= sf*spc, one code period).\n", NULL },
  { "doppler_bins", (getter)Acquisition_getprop_doppler_bins, NULL,
    "Coherent depth chosen: the slow-time FFT length in code reps (<= "
    "reps).\n",
    NULL },
  { "sf", (getter)Acquisition_getprop_sf, NULL,
    "Chips per PN segment, inferred from len(code).\n", NULL },
  { "spc", (getter)Acquisition_getprop_spc, NULL,
    "Samples per chip (chip-rate oversample factor).\n", NULL },
  { "reps", (getter)Acquisition_getprop_reps, NULL,
    "Max coherent code repetitions (the coherence ceiling).\n", NULL },
  { "n_noncoh", (getter)Acquisition_getprop_n_noncoh, NULL,
    "Non-coherent looks per detection (1 = pure coherent).\n", NULL },
  { "ring_cap", (getter)Acquisition_getprop_ring_cap, NULL,
    "Input ring capacity in complex samples.\n", NULL },
  { "noise_lo", (getter)Acquisition_getprop_noise_lo, NULL,
    "First CFAR reference bin (inclusive).\n", NULL },
  { "noise_hi", (getter)Acquisition_getprop_noise_hi, NULL,
    "Last CFAR reference bin (inclusive).\n", NULL },
  { "threshold", (getter)Acquisition_getprop_threshold, NULL,
    "CFAR gate on the test statistic (coherent path).\n", NULL },
  { "eta", (getter)Acquisition_getprop_eta, NULL,
    "Raw per-cell Rayleigh amplitude threshold.\n", NULL },
  { "eta_nc", (getter)Acquisition_getprop_eta_nc, NULL,
    "Non-coherent CFAR threshold (order-N_nc Marcum).\n", NULL },
  { "pfa_cell", (getter)Acquisition_getprop_pfa_cell, NULL,
    "Bonferroni per-cell false-alarm probability over the searched cells.\n",
    NULL },
  { "pd_predicted", (getter)Acquisition_getprop_pd_predicted, NULL,
    "Predicted Pd at cn0_dbhz and the chosen grid.\n", NULL },
  { "fs", (getter)Acquisition_getprop_fs, NULL,
    "Sample rate (Hz) = chip_rate * spc.\n", NULL },
  { "chip_rate", (getter)Acquisition_getprop_chip_rate, NULL,
    "Chip rate (Hz).\n", NULL },
  { "cn0_dbhz", (getter)Acquisition_getprop_cn0_dbhz, NULL,
    "Carrier-to-noise density used to size the search (dB-Hz).\n", NULL },
  { "doppler_span_hz", (getter)Acquisition_getprop_doppler_span_hz, NULL,
    "Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz.\n",
    NULL },
  { "doppler_res_hz", (getter)Acquisition_getprop_doppler_res_hz, NULL,
    "Doppler bin width = chip_rate/(sf*doppler_bins) Hz.\n", NULL },
  { "pd", (getter)Acquisition_getprop_pd, NULL,
    "Target detection probability.\n", NULL },
  { "underpowered", (getter)Acquisition_getprop_underpowered, NULL,
    "True when pd_predicted < pd (the search cannot meet the target).\n",
    NULL },
  { NULL }
};

static PyObject *
AcquisitionObj_destroy (AcquisitionObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquisitionObj_enter (AcquisitionObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AcquisitionObj_exit (AcquisitionObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AcquisitionObj_methods[]
    = { { "reset", (PyCFunction)AcquisitionObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "push", (PyCFunction)AcquisitionObj_push, METH_VARARGS,
          "push(x) -> list[tuple]\n"
          "\n"
          "Returns list of (doppler_bin, code_phase, peak_mag, noise_est, "
          "test_stat, snr_est,) tuples.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import Acquisition\n"
          "    >>> obj = Acquisition(np.zeros(1, dtype=np.uint8), \"mean\", "
          "1, 4, 1000000.0, 50.0, 0.0, 1e-3, 0.9, 1)\n"
          "    >>> results = obj.push(np.zeros(4, dtype=np.complex64))\n"
          "    >>> isinstance(results, list)\n"
          "    True\n" },
        { "destroy", (PyCFunction)AcquisitionObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)AcquisitionObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)AcquisitionObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject AcquisitionObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.Acquisition",
  .tp_basicsize                           = sizeof (AcquisitionObject),
  .tp_dealloc                             = (destructor)AcquisitionObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a streaming DSSS acquisition engine.\n",
  .tp_methods = AcquisitionObj_methods,
  .tp_getset  = Acquisition_getset,
  .tp_new     = AcquisitionObj_new,
  .tp_init    = (initproc)AcquisitionObj_init,
};
