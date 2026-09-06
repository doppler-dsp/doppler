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
  static char       *kwlist[]    = { "code",         "spc",
                                     "chip_rate",    "symbol_rate",
                                     "cn0_dbhz",     "doppler_uncertainty",
                                     "pfa",          "pd",
                                     "noise_mode",   "code_only_epochs",
                                     "doppler_rate", NULL };
  PyObject          *code_obj    = NULL;
  unsigned long long spc_raw     = 4;
  double             chip_rate   = 1000000.0;
  double             symbol_rate = 1000.0;
  double             cn0_dbhz    = 50.0;
  double             doppler_uncertainty  = 0.0;
  double             pfa                  = 1e-3;
  double             pd                   = 0.9;
  const char        *noise_mode_str       = "mean";
  unsigned long long code_only_epochs_raw = 1;
  double             doppler_rate         = 0.0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|KddddddsKd", kwlist, &code_obj, &spc_raw, &chip_rate,
          &symbol_rate, &cn0_dbhz, &doppler_uncertainty, &pfa, &pd,
          &noise_mode_str, &code_only_epochs_raw, &doppler_rate))
    return -1;
  size_t spc        = (size_t)spc_raw;
  int    noise_mode = 0;
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
  size_t         code_only_epochs = (size_t)code_only_epochs_raw;
  PyArrayObject *code_arr         = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = acq_create_continuous (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, spc, chip_rate,
      symbol_rate, cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode,
      code_only_epochs, doppler_rate);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "acq_create_continuous returned NULL");
      return -1;
    }
  if (self->handle->underpowered)
    {
      if (PyErr_WarnEx (PyExc_UserWarning,
                        "Acquisition is under-powered: pd_predicted < pd at "
                        "this cn0_dbhz. Raise cn0_dbhz or narrow "
                        "doppler_uncertainty.",
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
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
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
          "(NNNNNNN)",
          PyLong_FromUnsignedLongLong (
              (unsigned long long)results[i].doppler_bin),
          PyLong_FromUnsignedLongLong (
              (unsigned long long)results[i].code_phase),
          PyFloat_FromDouble ((double)results[i].peak_mag),
          PyFloat_FromDouble ((double)results[i].noise_est),
          PyFloat_FromDouble ((double)results[i].test_stat),
          PyFloat_FromDouble ((double)results[i].cn0_dbhz_est),
          PyLong_FromUnsignedLongLong (
              (unsigned long long)results[i].samples_consumed));
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
AcquisitionObj_configure_search_raw (AcquisitionObject *self, PyObject *args,
                                     PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]        = { "doppler_bins", "n_noncoh", NULL };
  unsigned long long doppler_bins_raw = 0ULL;
  unsigned long long n_noncoh_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist,
                                    &doppler_bins_raw, &n_noncoh_raw))
    return NULL;
  size_t doppler_bins = (size_t)doppler_bins_raw;
  size_t n_noncoh     = (size_t)n_noncoh_raw;
  int    _rc = acq_configure_search_raw (self->handle, doppler_bins, n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquisitionObj_set_max_peaks (AcquisitionObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  size_t n   = (size_t)n_raw;
  int    _rc = acq_set_max_peaks (self->handle, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_max_peaks failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquisitionObj_set_telemetry (AcquisitionObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "tlm", "prefix", "decim", NULL };
  PyObject     *tlm_obj   = Py_None;
  const char   *prefix    = NULL;
  unsigned long decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Os|k", _kwlist, &tlm_obj,
                                    &prefix, &decim_raw))
    return NULL;
  dp_tlm_t *tlm = NULL;
  if (tlm_obj != Py_None)
    {
      PyObject *tlm_cap = tlm_obj;
      Py_INCREF (tlm_cap);
      if (!PyCapsule_CheckExact (tlm_cap))
        {
          Py_DECREF (tlm_cap);
          tlm_cap = PyObject_GetAttrString (tlm_obj, "_capsule");
          if (!tlm_cap)
            return NULL;
        }
      tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap,
                                              "doppler.telemetry.dp_tlm");
      Py_DECREF (tlm_cap);
      if (!tlm)
        return NULL;
    }
  uint32_t decim = (uint32_t)decim_raw;
  int      _rc   = acq_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquisitionObj_surface (AcquisitionObject *self, PyObject *args,
                        PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &out_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  float *out     = (float *)PyArray_DATA (out_arr);
  size_t out_len = (size_t)PyArray_SIZE (out_arr);
  size_t y       = acq_surface (self->handle, out, out_len);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
AcquisitionObj_surface_doppler_hz (AcquisitionObject *self, PyObject *args,
                                   PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &out_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  double *out     = (double *)PyArray_DATA (out_arr);
  size_t  out_len = (size_t)PyArray_SIZE (out_arr);
  size_t  y       = acq_surface_doppler_hz (self->handle, out, out_len);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
AcquisitionObj_surface_chip_phase (AcquisitionObject *self, PyObject *args,
                                   PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "out", NULL };
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &out_obj))
    return NULL;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_DOUBLE
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
      out_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
  if (!out_arr)
    {
      return NULL;
    }
  double *out     = (double *)PyArray_DATA (out_arr);
  size_t  out_len = (size_t)PyArray_SIZE (out_arr);
  size_t  y       = acq_surface_chip_phase (self->handle, out, out_len);
  Py_DECREF (out_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
AcquisitionObj_state_bytes (AcquisitionObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (acq_state_bytes (self->handle));
}

static PyObject *
AcquisitionObj_get_state (AcquisitionObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = acq_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  acq_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AcquisitionObj_set_state (AcquisitionObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != acq_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (acq_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Acquisition_getprop_max_peaks (AcquisitionObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->max_peaks);
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
  return PyLong_FromUnsignedLongLong ((unsigned long long)((
      self->handle->window_bins * self->handle->coherent_bins)));
}
static PyObject *
Acquisition_getprop_coherent_bins (AcquisitionObject *self,
                                   void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->coherent_bins);
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
Acquisition_getprop_straddle_loss (AcquisitionObject *self,
                                   void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->straddle_loss);
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
static PyObject *
Acquisition_getprop_symbol_rate (AcquisitionObject *self,
                                 void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->symbol_rate);
}
static PyObject *
Acquisition_getprop_epochs_per_symbol (AcquisitionObject *self,
                                       void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->epochs_per_symbol);
}
static PyObject *
Acquisition_getprop_keep_surface (AcquisitionObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->keep_surface);
}
static int
Acquisition_setprop_keep_surface (AcquisitionObject *self, PyObject *value,
                                  void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  int v = 0;
  if (!PyArg_Parse (value, "i", &v))
    return -1;
  self->handle->keep_surface = v;
  return 0;
}
static PyObject *
Acquisition_getprop_surface_rows (AcquisitionObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)((self->handle->n_surf / self->handle->code_bins)));
}
static PyObject *
Acquisition_getprop_surface_at (AcquisitionObject *self,
                                void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->surface_at);
}
static PyObject *
Acquisition_getprop_n_peaks (AcquisitionObject *self,
                             void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_peaks);
}
static PyObject *
Acquisition_getprop_n_held (AcquisitionObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->n_held);
}
static PyObject *
Acquisition_getprop_peak_conc (AcquisitionObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->peak_conc);
}

static PyObject *
Acquisition_getprop_threads (AcquisitionObject *self,
                             void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->threads);
}

static PyObject *
Acquisition_getprop_carrier_freq_hz (AcquisitionObject *self,
                                     void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->carrier_freq_hz);
}

static PyGetSetDef Acquisition_getset[] = {
  { "max_peaks", (getter)Acquisition_getprop_max_peaks, NULL,
    "The peak list's capacity per dwell (1 = the classic gated maximum); set "
    "with set_max_peaks().\n",
    NULL },
  { "code_bins", (getter)Acquisition_getprop_code_bins, NULL,
    "Code-phase hypotheses searched (= sf*spc, one code period).\n", NULL },
  { "doppler_bins", (getter)Acquisition_getprop_doppler_bins, NULL,
    "Native Doppler bins this engine searches: the window-tile count times "
    "the block-coherent depth inside each tile (`coherent_bins`), one uniform "
    "grid of `doppler_res_hz` over the tiled span in FFT-bin order -- what a "
    "hit's `doppler_bin` indexes.\n",
    NULL },
  { "coherent_bins", (getter)Acquisition_getprop_coherent_bins, NULL,
    "The block-coherent depth D inside every window tile (design §2.3): "
    "epochs per block and Doppler rows per tile. 1 without a code-only "
    "window.\n",
    NULL },
  { "sf", (getter)Acquisition_getprop_sf, NULL,
    "Chips per PN segment, inferred from len(code).\n", NULL },
  { "spc", (getter)Acquisition_getprop_spc, NULL,
    "Samples per chip (chip-rate oversample factor).\n", NULL },
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
    "Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over the "
    "straddle priors (slow-time scalloping, intra-segment rotation, "
    "code-phase sample offset - quadrature over uniform priors), matching "
    "what the Monte-Carlo characterization measures rather than the on-grid "
    "best case.\n",
    NULL },
  { "straddle_loss", (getter)Acquisition_getprop_straddle_loss, NULL,
    "Mean amplitude derating of the correlation peak from grid straddle "
    "(slow-time Doppler scalloping x intra-segment rotation x code-phase "
    "sample offset, each averaged over a uniform prior) - a diagnostic "
    "summary; 20*log10(straddle_loss) is the loss in dB. Sizing and "
    "pd_predicted average Pd itself over the priors (Pd at this mean "
    "amplitude would overstate the mean Pd).\n",
    NULL },
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
    "True when pd_predicted < pd -- the search cannot meet the target pd at "
    "this cn0_dbhz and geometry. The engine still builds a best-effort grid "
    "rather than failing; because C cannot raise a Python warning from a "
    "successful create, construction also emits a UserWarning in this case.\n",
    NULL },
  { "symbol_rate", (getter)Acquisition_getprop_symbol_rate, NULL,
    "Continuous data-symbol rate (Hz) this engine was built with -- "
    "diagnostic only, doesn't feed sizing (this engine never coherently "
    "combines regardless).\n",
    NULL },
  { "epochs_per_symbol", (getter)Acquisition_getprop_epochs_per_symbol, NULL,
    "(chip_rate/sf)/symbol_rate -- code epochs per data symbol; 0 when "
    "symbol_rate is 0.\n",
    NULL },
  { "keep_surface", (getter)Acquisition_getprop_keep_surface,
    (setter)Acquisition_setprop_keep_surface,
    "1 keeps every decided dwell's surface for `surface()` (normalised into "
    "the gate's units at each decision); 0 (the default) costs nothing. "
    "`set_surface_sink()` in C sets it.\n",
    NULL },
  { "surface_rows", (getter)Acquisition_getprop_surface_rows, NULL,
    "Rows of the surface `surface()` returns: the Doppler axis in surface "
    "units (tiles x interpolated slow-time rows); its columns are "
    "`code_bins`.\n",
    NULL },
  { "surface_at", (getter)Acquisition_getprop_surface_at, NULL,
    "`samples_consumed` of the dwell whose surface `surface()` returns (0 "
    "until one has been captured).\n",
    NULL },
  { "n_peaks", (getter)Acquisition_getprop_n_peaks, NULL,
    "Picks in the last decided dwell, held twins included.\n", NULL },
  { "n_held", (getter)Acquisition_getprop_n_held, NULL,
    "Picks of the last decided dwell held as same-code-phase twins rather "
    "than listed (design §7.1).\n",
    NULL },
  { "peak_conc", (getter)Acquisition_getprop_peak_conc, NULL,
    "Concentration of the last dwell's strongest peak: the power of its main "
    "lobe (its row and one either side, the exclusion zone's width) over the "
    "total power of its code-phase column across every Doppler row and tile. "
    "Near 1 for a clean single emitter, even one straddling two tiles; about "
    "0.5 when a data transition splits it into twins two or more tiles away; "
    "lower when a coherent block straddles data (design §2.4).\n",
    NULL },
  { "threads", (getter)Acquisition_getprop_threads, NULL,
    "Workers the searcher fans its tiles across, the calling thread included "
    "(design §2.3); 1 = serial. Set with set_threads(); a continuous engine "
    "with more than one tile starts at the machine's online core count.\n",
    NULL },
  { "carrier_freq_hz", (getter)Acquisition_getprop_carrier_freq_hz, NULL,
    "RF carrier the Doppler is physically coupled to, Hz (0.0 = uncoupled); "
    "set with set_carrier_freq_hz().\n",
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

static PyObject *
AcquisitionObj_set_threads (AcquisitionObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "n", NULL };
  int          n         = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "i", _kwlist, &n))
    return NULL;
  int _rc = acq_set_threads (self->handle, n);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_threads failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AcquisitionObj_set_carrier_freq_hz (AcquisitionObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]       = { "carrier_freq_hz", NULL };
  double       carrier_freq_hz = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist,
                                    &carrier_freq_hz))
    return NULL;
  int _rc = acq_set_carrier_freq_hz (self->handle, carrier_freq_hz);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "set_carrier_freq_hz failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AcquisitionObj_methods[] = {
  { "reset", (PyCFunction)AcquisitionObj_reset, METH_NOARGS,
    "Drain the input ring and reset the coherent accumulator.\n"
    "\n"
    "Discards any buffered samples that have not yet completed a frame and\n"
    "clears the non-coherent power accumulator and dwell bookkeeping, so the\n"
    "next push() begins a fresh search from an empty ring. The construction\n"
    "parameters — grid, thresholds, and PN reference — are untouched; only\n"
    "the in-flight streaming state is dropped.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)\n"
    ">>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> _ = a.push(burst[:100])   # a partial frame, buffered mid-stream\n"
    ">>> a.reset()                 # drop it before it can bias a detection\n"
    ">>> a.push(burst)[0][:2]      # (Doppler bin, code phase)\n"
    "(0, 17)\n" },

  { "push", (PyCFunction)AcquisitionObj_push, METH_VARARGS,
    "push(x) -> list[tuple]\n"
    "\n"
    "Stream raw samples; emit one event per CFAR dump above threshold.\n"
    "\n"
    "Buffers x, then for every complete frame applies the slow-time Doppler\n"
    "FFT, correlates against the PN reference, dumps the coherent surface\n"
    "(or, when n_noncoh > 1, accumulates |·|² over n_noncoh looks first),\n"
    "gates the peak on the auto-configured threshold, and appends an\n"
    "acq_result_t. Each event carries the peak's Doppler bin and code phase\n"
    "(the two search axes), its CFAR statistic, and an estimated C/N0 — see\n"
    "acq_result_t.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Raw input, interleaved CF32, n_in complex samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "list[tuple]\n"
    "    Number of events written (0 … max_results).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 doppler_uncertainty=40e3)\n"
    ">>> fs = 1e6 * 4                    # sample rate = chip_rate * spc\n"
    ">>> t = np.arange(a.code_bins * a.n_noncoh)\n"
    ">>> carrier = np.exp(2j * np.pi * (a.doppler_res_hz / fs) * t)\n"
    ">>> sig = (np.tile(np.roll(s0, 17), a.n_noncoh)\n"
    "...        * carrier).astype(np.complex64)\n"
    ">>> a.push(sig)[0][:2]              # (Doppler-window bin, code phase)\n"
    "(1, 17)\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)AcquisitionObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the search grid directly, bypassing both auto-sizing searches --\n"
    "the advanced escape hatch (mirrors\n"
    "Dll.configure_lock_raw/Costas.configure_lock). Resizes every\n"
    "buffer/plan that depends on the grid (the slow-time FFT, the code\n"
    "correlator, the reference, and every per-frame scratch buffer),\n"
    "re-derives the threshold ladder for the pinned grid from the same\n"
    "physics __init__ used, and clears in-flight accumulation (ring\n"
    "contents, the non-coherent power accumulator, dwell bookkeeping) --\n"
    "call between push() calls, never a substitute for one. Raises\n"
    "ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside\n"
    "[1, 256] (the internal non-coherent-look safety-valve ceiling).\n"
    "\n"
    "Resizes every buffer/plan that depends on the grid (the slow-time FFT,\n"
    "the code correlator, the reference, and every per-frame scratch\n"
    "buffer), re-derives the threshold ladder for the pinned grid from the\n"
    "same physics acq_create_burst()/acq_create_continuous() used, and\n"
    "clears in-flight accumulation (ring contents, the non-coherent power\n"
    "accumulator, dwell bookkeeping) — call between push() calls, never a\n"
    "substitute for one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Coherent depth to pin, in `[1, reps]`.\n"
    "n_noncoh : int\n"
    "    Non-coherent look count to pin, in `[1,\n"
    "    ACQ_N_NONCOH_SAFETY_CEILING]`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure_search_raw failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid\n"
    ">>> a.doppler_bins, a.n_noncoh\n"
    "(1, 4)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)\n"
    ">>> a.push(burst)[0][:2]      # detects at the pinned grid\n"
    "(0, 17)\n" },
  { "set_max_peaks", (PyCFunction)(void *)AcquisitionObj_set_max_peaks,
    METH_VARARGS | METH_KEYWORDS,
    "set_max_peaks(n) -> None\n"
    "\n"
    "How many peaks a dwell may report -- the peak list's capacity\n"
    "(docs/design/async-dsss-receiver.md section 7.1). One (the default) is\n"
    "the classic gated maximum. More lists every peak above the same gate,\n"
    "strongest first, with an exclusion zone of one Doppler bin by one chip\n"
    "around each (one emitter's main lobe, so its own shoulders are not the\n"
    "next peak) and the two-epoch rule for a peak at an already-listed code\n"
    "phase (a data transition inside the epoch splits one emitter into twins\n"
    "at its own code phase on other tiles; such a peak is held for one dwell\n"
    "and listed only if it is still there, at the same tile, on the next).\n"
    "Each listed peak is one record from push(), all of a dwell's sharing\n"
    "samples_consumed and noise_est; a held twin takes one of the n slots\n"
    "that dwell but is not reported. The threshold does not change with n.\n"
    "Raises ValueError outside 1..64. Clears the held candidates.\n"
    "\n"
    "One (the default) is the classic detector -- the maximum of the\n"
    "surface, gated. More is the list of docs/design/async-dsss-receiver.md\n"
    "§7.1: every peak above the same gate, strongest first, each with an\n"
    "exclusion zone of one Doppler bin by one chip around it (one emitter's\n"
    "main lobe, so its own shoulders are not the next peak), and the\n"
    "two-epoch rule for a peak at an already-listed code phase -- a data\n"
    "transition inside the epoch splits one emitter into twins at its own\n"
    "code phase on other tiles, so such a peak is held for one dwell and\n"
    "listed only if it was there, at the same tile, on the previous one.\n"
    "Each listed peak is one acq_result_t from acq_push(), all of a dwell's\n"
    "sharing its `samples_consumed` and `noise_est`. A held twin takes a\n"
    "slot of the `n` for that dwell but is not reported. The threshold does\n"
    "not change: a second peak is another draw from the same cells against\n"
    "the same union bound. Clears the held candidates.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    1 … ACQ_MAX_PEAKS.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_max_peaks failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> code = (np.arange(31) * 5 % 2).astype(np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, symbol_rate=1e3,\n"
    "...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)\n"
    ">>> a.max_peaks\n"
    "1\n"
    ">>> a.set_max_peaks(8)\n"
    ">>> a.max_peaks\n"
    "8\n" },
  { "set_telemetry", (PyCFunction)(void *)AcquisitionObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context and register the engine's\n"
    "probes on it (design §2.4).\n"
    "\n"
    "Registers ten probes, emitted once per DECIDED dwell (a coherent dump,\n"
    "or the dwell that completes `n_noncoh` looks) and further thinned by\n"
    "decim: \"<prefix>.stat\" (the dwell's test statistic — the strongest "
    "cell\n"
    "against the CFAR reference, in the units the gate is set in),\n"
    "\"<prefix>.gate\" (that gate: `threshold` on the coherent path, "
    "`eta_nc`\n"
    "on the non-coherent one — plotted together they show exactly where a\n"
    "hit fired), \"<prefix>.noise\" (the CFAR reference `noise_est`),\n"
    "\"<prefix>.peak\" (the strongest cell's raw value), \"<prefix>.row\" "
    "and\n"
    "\"<prefix>.col\" (its native Doppler row and code-phase column — a\n"
    "surface coordinate, not a physical unit; acq_surface_doppler_hz() and\n"
    "acq_surface_chip_phase() convert), \"<prefix>.n_peaks\" (picks in the\n"
    "dwell, held twins included), \"<prefix>.n_held\" (picks held as\n"
    "same-code-phase twins rather than listed, §7.1), \"<prefix>.conc\" (the\n"
    "strongest pick's concentration — see `peak_conc`: its main lobe's power\n"
    "over its whole column's, near 1 for one clean emitter even when it\n"
    "straddles two tiles, about 0.5 when a data transition splits it into\n"
    "twins two or more tiles away, lower still when a coherent block\n"
    "straddles data — the discriminator between one emitter's splatter and a\n"
    "second emitter) and \"<prefix>.hit\" (1 when the gate fired). Passing\n"
    "NULL detaches. Setup path, never hot; the context is borrowed and must\n"
    "outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"acq\" or \"ch0.acq\".\n"
    "decim : int\n"
    "    Emit every decim-th decided dwell; >= 1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_telemetry failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> a.set_telemetry(tlm, \"acq\")\n"
    ">>> sorted(tlm.probe_names)[:3]\n"
    "['acq.col', 'acq.conc', 'acq.gate']\n"
    ">>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)\n"
    ">>> _ = a.push(x)\n"
    ">>> len(tlm.read()) % 10      # ten records per decided dwell\n"
    "0\n" },
  { "surface", (PyCFunction)(void *)AcquisitionObj_surface,
    METH_VARARGS | METH_KEYWORDS,
    "surface(out) -> int\n"
    "\n"
    "The last decided dwell's surface, in the gate's own units.\n"
    "\n"
    "Copies the surface the last dwell was decided on into out, row-major\n"
    "`surface_rows` (Doppler: tiles, or interpolated slow-time rows) by\n"
    "`code_bins` (code phase), every cell divided by the same CFAR reference\n"
    "the gate used — so a cell reads as its own test statistic, to a float\n"
    "rounding (the SIMD build's fast-math may take a reciprocal in this loop\n"
    "and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on\n"
    "the non-coherent path) is a flat plane on a plot. The engine keeps this\n"
    "only while `keep_surface` is set (a caller sets it, or\n"
    "acq_set_surface_sink() does): set it, push, then read. `surface_at`\n"
    "says which dwell it is; a time-decimated record is the caller reading\n"
    "every k-th dwell, or a sink with `decim`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : NDArray[np.float32]\n"
    "    At least `surface_rows * code_bins` floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Cells written (`surface_rows * code_bins`), or 0 when no dwell has\n"
    "    been decided with `keep_surface` set, or out is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> a.keep_surface = 1\n"
    ">>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)\n"
    ">>> _ = a.push(x)\n"
    ">>> s = np.empty(a.surface_rows * a.code_bins, dtype=np.float32)\n"
    ">>> a.surface(s) == s.size\n"
    "True\n"
    ">>> s.reshape(a.surface_rows, a.code_bins).shape == (a.surface_rows, "
    "1022)\n"
    "True\n" },
  { "surface_doppler_hz",
    (PyCFunction)(void *)AcquisitionObj_surface_doppler_hz,
    METH_VARARGS | METH_KEYWORDS,
    "surface_doppler_hz(out) -> int\n"
    "\n"
    "The surface's Doppler axis: the frequency of each row, in Hz.\n"
    "\n"
    "One value per surface row, the fold and scale a hit's `doppler_hz_est`\n"
    "uses (dp_fftfreq_index() times `doppler_res_hz`, on the interpolated\n"
    "grid where the slow-time axis is interpolated), so a plot of\n"
    "acq_surface() carries the same axis a DetectionEvent reports on.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : NDArray[np.float64]\n"
    "    At least `surface_rows` doubles.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Values written (`surface_rows`), or 0 if out is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 doppler_uncertainty=4000.0)\n"
    ">>> f = np.empty(a.surface_rows, dtype=np.float64)\n"
    ">>> a.surface_doppler_hz(f) == a.surface_rows\n"
    "True\n"
    ">>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())\n"
    "True\n" },
  { "surface_chip_phase",
    (PyCFunction)(void *)AcquisitionObj_surface_chip_phase,
    METH_VARARGS | METH_KEYWORDS,
    "surface_chip_phase(out) -> int\n"
    "\n"
    "The surface's code-phase axis: the chip phase of each column.\n"
    "\n"
    "One value per surface column, in chips, the same mapping\n"
    "acq_build_handoff() applies to a hit's `code_phase` — so a plotted peak\n"
    "sits at the chip phase the DetectionEvent would carry.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : NDArray[np.float64]\n"
    "    At least `code_bins` doubles.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Values written (`code_bins`), or 0 if out is too small.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> c = np.empty(a.code_bins, dtype=np.float64)\n"
    ">>> a.surface_chip_phase(c) == a.code_bins\n"
    "True\n"
    ">>> bool(c[0] == 0.0 and c[1] == 510.5)\n"
    "True\n" },
  { "state_bytes", (PyCFunction)AcquisitionObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Acquisition has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AcquisitionObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures exactly the state that evolves as the object runs, so a blob\n"
    "taken now and restored later resumes from this point. Construction\n"
    "parameters are not included: restore into an object built the same way.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the Acquisition has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AcquisitionObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Acquisition has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AcquisitionObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)AcquisitionObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Acquisition be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Acquisition\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AcquisitionObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Acquisition.\n"
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
  { "set_threads", (PyCFunction)(void *)AcquisitionObj_set_threads,
    METH_VARARGS | METH_KEYWORDS,
    "set_threads(n) -> None\n"
    "\n"
    "Set how many threads the searcher fans its tiles across (design\n"
    "§2.3: a roll per thread on persistent workers).\n"
    "\n"
    "A continuous engine is created with a pool of the machine's online\n"
    "cores when it has more than one tile; a burst engine, and a single-tile\n"
    "one, run serially. This sets the count: 0 auto-selects the online core\n"
    "count, 1 runs everything on the calling thread, n runs on n workers\n"
    "(the caller included). The workers are created here, once, and parked\n"
    "between pushes; nothing is created per push. The surface is\n"
    "bit-identical at every count -- the tiles are independent after the one\n"
    "forward transform and each writes its own rows -- so this changes the\n"
    "cost of a push and nothing about its result. Setup path, never hot; not\n"
    "while another thread is inside push().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Thread count; 0 = online cores, 1 = serial.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_threads failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 doppler_uncertainty=4000.0)\n"
    ">>> a.threads >= 1               # a pool, sized to the machine\n"
    "True\n"
    ">>> a.set_threads(1)\n"
    ">>> a.threads\n"
    "1\n" },
  { "set_carrier_freq_hz",
    (PyCFunction)(void *)AcquisitionObj_set_carrier_freq_hz,
    METH_VARARGS | METH_KEYWORDS,
    "set_carrier_freq_hz(carrier_freq_hz) -> None\n"
    "\n"
    "Couple the code clock to the carrier: the chip rate dilates by\n"
    "doppler_hz / carrier_freq_hz, and the engine accounts for it -- every\n"
    "window tile's epochs are shifted along the code axis by the drift the\n"
    "tile's own frequency implies before the slow-time transform (inside a\n"
    "coherent block of D epochs), and the hand-off advances the hit's code\n"
    "phase by the drift over half the dwell (design section 12.11, 12.12).\n"
    "Config, not running state: not in the state blob, so a resumed engine\n"
    "wants it set again. 0.0 (the default) = uncoupled, the engine as it ran\n"
    "without it. Raises ValueError for a negative or non-finite value.\n"
    "\n"
    "A physically-coupled Doppler moves the code as well as the carrier --\n"
    "100 chips/s at 20 ppm of 5 Mcps -- and the engine's two long\n"
    "integrations both smear over it (doppler#1256, #1254):\n"
    "\n"
    "- **Inside a coherent block** of D epochs every tile's epoch\n"
    "  correlations are shifted along the code axis by the drift the tile's\n"
    "  own frequency implies, `f_tile / carrier` chips per chip, aligned to\n"
    "  the block's middle, before the slow-time transform (a linear phase on\n"
    "  each epoch's product, exact to a fraction of a sample). Measured at\n"
    "  SPEC's 20 ppm with D = 154 (3.1 chips of drift across the block):\n"
    "  without it the block's peak is 13 dB down and 3 chips wide and the\n"
    "  depth detects nothing at 34 dB-Hz; with it the block reads as a still\n"
    "  one.\n"
    "- **The hand-off** (acq_build_handoff()) advances the hit's code phase\n"
    "  by the drift over half the dwell -- the non-coherent sum's peak is\n"
    "  the phase at the dwell's middle, the seed is wanted at its end: 0.9\n"
    "  chip at the 40 dB-Hz floor, past a refine loop's pull-in.\n"
    "\n"
    "Config, not running state: it is not in the state blob, so a resumed\n"
    "engine wants it set again by its holder, as at create. Default 0.0\n"
    "(uncoupled) is the engine exactly as it ran without it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "carrier_freq_hz : float\n"
    "    RF carrier, Hz; 0.0 = uncoupled.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_carrier_freq_hz failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> a = Acquisition(code, spc=2, chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)\n"
    ">>> a.carrier_freq_hz\n"
    "0.0\n"
    ">>> a.set_carrier_freq_hz(2.5e9)\n"
    ">>> a.carrier_freq_hz\n"
    "2500000000.0\n" },
  { NULL }
};

static PyTypeObject AcquisitionObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.Acquisition",
  .tp_basicsize                           = sizeof (AcquisitionObject),
  .tp_dealloc                             = (destructor)AcquisitionObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a continuous-mode acquisition engine: always wideband\n"
    "window-tiling, with a block-coherent depth inside the tiles when the\n"
    "waveform has a pure-code window.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.uint8]\n"
    "    PN chips (0/1), length code_len.\n"
    "spc : int, default 4\n"
    "    Samples per chip (>= 1).\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate in Hz (> 0).\n"
    "symbol_rate : float, default 1000.0\n"
    "    Continuous data-symbol rate in Hz; <= 0 means no known clock.\n"
    "    Diagnostic only (exposed via acq_state_t::epochs_per_symbol), "
    "doesn't\n"
    "    feed sizing: this engine never coherently combines regardless of "
    "the\n"
    "    data-modulation clock.\n"
    "cn0_dbhz : float, default 50.0\n"
    "    Carrier-to-noise density in dB-Hz (> 0).\n"
    "doppler_uncertainty : float, default 0.0\n"
    "    One-sided Doppler search half-range in Hz; 0 uses the full native "
    "span\n"
    "    +/- chip_rate/(2*sf) (still window-tiled, at window_bins=1).\n"
    "pfa : float, default 1e-3\n"
    "    Target system (max-of-N) false-alarm probability (0,1).\n"
    "pd : float, default 0.9\n"
    "    Target detection probability (0,1).\n"
    "noise_mode : Literal[\"mean\", \"median\", \"min\", \"max\"], default "
    "\"mean\"\n"
    "    CFAR mode index: 0=mean, 1=median, 2=min, 3=max.\n"
    "code_only_epochs : int, default 1\n"
    "    Whole pure-code epochs the waveform's data-free window holds at any\n"
    "    chip phase (floor(W_symbols * chips_per_symbol / sf) - 1; design "
    "§2.1).\n"
    "    1 = no window: a coherent depth of 1.\n"
    "doppler_rate : float, default 0.0\n"
    "    Doppler rate in Hz/s the coherent depth is bounded against (the "
    "drift\n"
    "    over one block stays inside half a slow-time row); 0 leaves the "
    "window\n"
    "    as the only bound.\n"
    "\n"
    "Warns\n"
    "-----\n"
    "UserWarning\n"
    "    Emitted after construction when ``underpowered`` holds: "
    "``Acquisition\n"
    "    is under-powered: pd_predicted < pd at this cn0_dbhz. Raise cn0_dbhz "
    "or\n"
    "    narrow doppler_uncertainty.``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Acquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)\n"
    ">>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> a.push(burst)[0][:2]    # detects (Doppler-window bin, code phase)\n"
    "(0, 17)\n"
    ">>> a.coherent_bins            # no window given: one epoch\n"
    "1\n"
    ">>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 code_only_epochs=7)\n"
    ">>> b.coherent_bins            # (7 + 1) // 2: a whole block fits\n"
    "4\n",
  .tp_methods = AcquisitionObj_methods,
  .tp_getset  = Acquisition_getset,
  .tp_new     = AcquisitionObj_new,
  .tp_init    = (initproc)AcquisitionObj_init,
};
