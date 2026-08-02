/*
 * dsss_ext_burst_acq.c — BurstAcquisition type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* BurstAcquisitionObject — wraps burst_acq_state_t *       */
/* ======================================================== */

#include "burst_acq/burst_acq_core.h"

typedef struct
{
  PyObject_HEAD burst_acq_state_t *handle;
} BurstAcquisitionObject;

static void
BurstAcquisitionObj_dealloc (BurstAcquisitionObject *self)
{
  if (self->handle)
    burst_acq_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
BurstAcquisitionObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BurstAcquisitionObject *self
      = (BurstAcquisitionObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
BurstAcquisitionObj_init (BurstAcquisitionObject *self, PyObject *args,
                          PyObject *kwds)
{
  static char *kwlist[] = { "code",      "reps",     "spc",
                            "chip_rate", "cn0_dbhz", "doppler_uncertainty",
                            "pfa",       "pd",       "noise_mode",
                            NULL };
  PyObject    *code_obj = NULL;
  unsigned long long reps_raw            = 1;
  unsigned long long spc_raw             = 4;
  double             chip_rate           = 1000000.0;
  double             cn0_dbhz            = 50.0;
  double             doppler_uncertainty = 0.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  const char        *noise_mode_str      = "mean";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKddddds", kwlist,
                                    &code_obj, &reps_raw, &spc_raw, &chip_rate,
                                    &cn0_dbhz, &doppler_uncertainty, &pfa, &pd,
                                    &noise_mode_str))
    return -1;
  size_t reps       = (size_t)reps_raw;
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
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle = burst_acq_create ((const uint8_t *)PyArray_DATA (code_arr),
                                   code_len, reps, spc, chip_rate, cn0_dbhz,
                                   doppler_uncertainty, pfa, pd, noise_mode);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "burst_acq_create returned NULL");
      return -1;
    }
  /* Hand-patch (sacred fragment): the ONLY non-declarative line in this
   * file. objects/acq.toml declares the identical warning with a `warnings`
   * block (gh-481), but `warnings.condition` must be a bare C identifier
   * naming a bool field on the state struct, and burst_acq_state_t holds
   * nothing but the engine pointer -- so the reach every property here makes
   * through `engine->` via `expr` is exactly what the condition needs and
   * cannot have. Filed upstream; delete this and add the `warnings` block to
   * objects/burst_acq.toml once the condition accepts an expression.
   *
   * C cannot raise a Python warning, so surface an under-powered search
   * here: the auto-config still built a best-effort grid (bounded by the
   * internal non-coherent-look safety valve), it just cannot reach the
   * requested pd. */
  if (self->handle->engine->underpowered)
    {
      if (PyErr_WarnEx (PyExc_UserWarning,
                        "BurstAcquisition is under-powered: pd_predicted < "
                        "pd at this reps/cn0_dbhz. Raise reps or cn0_dbhz, "
                        "or narrow doppler_uncertainty.",
                        1)
          < 0)
        return -1;
    }
  return 0;
}

static PyObject *
BurstAcquisitionObj_reset (BurstAcquisitionObject *self,
                           PyObject               *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  burst_acq_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
BurstAcquisitionObj_push (BurstAcquisitionObject *self, PyObject *args)
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
    n_out = burst_acq_push (self->handle, _ng0, n_in, results, 64);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *lst = PyList_New ((Py_ssize_t)n_out);
  if (!lst)
    return NULL;
  for (size_t i = 0; i < n_out; i++)
    {
      PyObject *tup = Py_BuildValue (
          "(KKffffK)", (unsigned long long)results[i].doppler_bin,
          (unsigned long long)results[i].code_phase, results[i].peak_mag,
          results[i].noise_est, results[i].test_stat, results[i].cn0_dbhz_est,
          (unsigned long long)results[i].samples_consumed);
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
BurstAcquisitionObj_configure_search_raw (BurstAcquisitionObject *self,
                                          PyObject *args, PyObject *kwds)
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
  int    _rc
      = burst_acq_configure_search_raw (self->handle, doppler_bins, n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_search_raw failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BurstAcquisitionObj_state_bytes (BurstAcquisitionObject *self,
                                 PyObject               *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_acq_state_bytes (self->handle));
}

static PyObject *
BurstAcquisitionObj_get_state (BurstAcquisitionObject *self,
                               PyObject               *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = burst_acq_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  burst_acq_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
BurstAcquisitionObj_set_state (BurstAcquisitionObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != burst_acq_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (burst_acq_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
BurstAcquisition_getprop_code_bins (BurstAcquisitionObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->code_bins);
}
static PyObject *
BurstAcquisition_getprop_doppler_bins (BurstAcquisitionObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)((self->handle->engine->window_bins > 1)
                               ? self->handle->engine->window_bins
                               : self->handle->engine->coherent_bins));
}
static PyObject *
BurstAcquisition_getprop_sf (BurstAcquisitionObject *self,
                             void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->sf);
}
static PyObject *
BurstAcquisition_getprop_spc (BurstAcquisitionObject *self,
                              void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->spc);
}
static PyObject *
BurstAcquisition_getprop_reps (BurstAcquisitionObject *self,
                               void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->reps);
}
static PyObject *
BurstAcquisition_getprop_n_noncoh (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->n_noncoh);
}
static PyObject *
BurstAcquisition_getprop_ring_cap (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->ring_cap);
}
static PyObject *
BurstAcquisition_getprop_noise_lo (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->noise_lo);
}
static PyObject *
BurstAcquisition_getprop_noise_hi (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->engine->noise_hi);
}
static PyObject *
BurstAcquisition_getprop_threshold (BurstAcquisitionObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->engine->threshold);
}
static PyObject *
BurstAcquisition_getprop_eta (BurstAcquisitionObject *self,
                              void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->engine->eta);
}
static PyObject *
BurstAcquisition_getprop_eta_nc (BurstAcquisitionObject *self,
                                 void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)self->handle->engine->eta_nc);
}
static PyObject *
BurstAcquisition_getprop_pfa_cell (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->pfa_cell);
}
static PyObject *
BurstAcquisition_getprop_pd_predicted (BurstAcquisitionObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->pd_predicted);
}
static PyObject *
BurstAcquisition_getprop_straddle_loss (BurstAcquisitionObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->straddle_loss);
}
static PyObject *
BurstAcquisition_getprop_fs (BurstAcquisitionObject *self,
                             void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->fs);
}
static PyObject *
BurstAcquisition_getprop_chip_rate (BurstAcquisitionObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->chip_rate);
}
static PyObject *
BurstAcquisition_getprop_cn0_dbhz (BurstAcquisitionObject *self,
                                   void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->cn0_dbhz);
}
static PyObject *
BurstAcquisition_getprop_doppler_span_hz (BurstAcquisitionObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->doppler_span_hz);
}
static PyObject *
BurstAcquisition_getprop_doppler_res_hz (BurstAcquisitionObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->doppler_res_hz);
}
static PyObject *
BurstAcquisition_getprop_pd (BurstAcquisitionObject *self,
                             void                   *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->engine->pd);
}
static PyObject *
BurstAcquisition_getprop_underpowered (BurstAcquisitionObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->engine->underpowered));
}

static PyGetSetDef BurstAcquisition_getset[] = {
  { "code_bins", (getter)BurstAcquisition_getprop_code_bins, NULL,
    "Code-phase hypotheses searched (= sf*spc, one code period).\n", NULL },
  { "doppler_bins", (getter)BurstAcquisition_getprop_doppler_bins, NULL,
    "Coherent depth chosen: the slow-time FFT length in code reps (<= reps), "
    "unless doppler_uncertainty exceeds the native span, in which case this "
    "reports the wideband window-tile count instead (coherent depth forced to "
    "1 -- see acq_core.h's file doc comment).\n",
    NULL },
  { "sf", (getter)BurstAcquisition_getprop_sf, NULL,
    "Chips per PN segment, inferred from len(code).\n", NULL },
  { "spc", (getter)BurstAcquisition_getprop_spc, NULL,
    "Samples per chip (chip-rate oversample factor).\n", NULL },
  { "reps", (getter)BurstAcquisition_getprop_reps, NULL,
    "Max coherent code repetitions (the coherence ceiling).\n", NULL },
  { "n_noncoh", (getter)BurstAcquisition_getprop_n_noncoh, NULL,
    "Non-coherent looks per detection (1 = pure coherent).\n", NULL },
  { "ring_cap", (getter)BurstAcquisition_getprop_ring_cap, NULL,
    "Input ring capacity in complex samples.\n", NULL },
  { "noise_lo", (getter)BurstAcquisition_getprop_noise_lo, NULL,
    "First CFAR reference bin (inclusive).\n", NULL },
  { "noise_hi", (getter)BurstAcquisition_getprop_noise_hi, NULL,
    "Last CFAR reference bin (inclusive).\n", NULL },
  { "threshold", (getter)BurstAcquisition_getprop_threshold, NULL,
    "CFAR gate on the test statistic (coherent path).\n", NULL },
  { "eta", (getter)BurstAcquisition_getprop_eta, NULL,
    "Raw per-cell Rayleigh amplitude threshold.\n", NULL },
  { "eta_nc", (getter)BurstAcquisition_getprop_eta_nc, NULL,
    "Non-coherent CFAR threshold (order-N_nc Marcum).\n", NULL },
  { "pfa_cell", (getter)BurstAcquisition_getprop_pfa_cell, NULL,
    "Bonferroni per-cell false-alarm probability over the searched cells.\n",
    NULL },
  { "pd_predicted", (getter)BurstAcquisition_getprop_pd_predicted, NULL,
    "Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over the "
    "straddle priors (slow-time scalloping, intra-segment rotation, "
    "code-phase sample offset - quadrature over uniform priors), matching "
    "what the Monte-Carlo characterization measures rather than the on-grid "
    "best case.\n",
    NULL },
  { "straddle_loss", (getter)BurstAcquisition_getprop_straddle_loss, NULL,
    "Mean amplitude derating of the correlation peak from grid straddle "
    "(slow-time Doppler scalloping x intra-segment rotation x code-phase "
    "sample offset, each averaged over a uniform prior) - a diagnostic "
    "summary; 20*log10(straddle_loss) is the loss in dB. Sizing and "
    "pd_predicted average Pd itself over the priors (Pd at this mean "
    "amplitude would overstate the mean Pd).\n",
    NULL },
  { "fs", (getter)BurstAcquisition_getprop_fs, NULL,
    "Sample rate (Hz) = chip_rate * spc.\n", NULL },
  { "chip_rate", (getter)BurstAcquisition_getprop_chip_rate, NULL,
    "Chip rate (Hz).\n", NULL },
  { "cn0_dbhz", (getter)BurstAcquisition_getprop_cn0_dbhz, NULL,
    "Carrier-to-noise density used to size the search (dB-Hz).\n", NULL },
  { "doppler_span_hz", (getter)BurstAcquisition_getprop_doppler_span_hz, NULL,
    "Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz.\n",
    NULL },
  { "doppler_res_hz", (getter)BurstAcquisition_getprop_doppler_res_hz, NULL,
    "Doppler bin width = chip_rate/(sf*doppler_bins) Hz.\n", NULL },
  { "pd", (getter)BurstAcquisition_getprop_pd, NULL,
    "Target detection probability.\n", NULL },
  { "underpowered", (getter)BurstAcquisition_getprop_underpowered, NULL,
    "True when pd_predicted < pd -- the search cannot meet the target pd at "
    "this cn0_dbhz and geometry. The engine still builds a best-effort grid "
    "rather than failing; because C cannot raise a Python warning from a "
    "successful create, construction also emits a UserWarning in this case.\n",
    NULL },
  { NULL }
};

static PyObject *
BurstAcquisitionObj_destroy (BurstAcquisitionObject *self,
                             PyObject               *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      burst_acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
BurstAcquisitionObj_enter (BurstAcquisitionObject *self,
                           PyObject               *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
BurstAcquisitionObj_exit (BurstAcquisitionObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      burst_acq_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef BurstAcquisitionObj_methods[] = {
  { "reset", (PyCFunction)BurstAcquisitionObj_reset, METH_NOARGS,
    "Drain the input ring and reset the coherent accumulator." },

  { "push", (PyCFunction)BurstAcquisitionObj_push, METH_VARARGS,
    "push(x) -> list[tuple]\n"
    "\n"
    "Stream raw samples; emit one event per CFAR dump above threshold.\n"
    "\n"
    "Forwards to acq_push() on the embedded engine (see its doc comment in\n"
    "acq_core.h for the framing/CFAR mechanics). Each event carries the\n"
    "peak's Doppler bin and code phase (the two search axes), its CFAR\n"
    "statistic, and an estimated C/N0 — see acq_result_t.\n"
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
    ">>> from doppler.dsss import BurstAcquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), "
    "4).astype(np.complex64)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)\n"
    ">>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,\n"
    "...                      cn0_dbhz=50.0)\n"
    ">>> b.push(burst)[0][:2]      # (Doppler bin, code phase)\n"
    "(0, 17)\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)BurstAcquisitionObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> int\n"
    "\n"
    "Pin the search grid directly, bypassing both auto-sizing searches -- the "
    "advanced escape hatch (mirrors "
    "Dll.configure_lock_raw/Costas.configure_lock). Resizes every buffer/plan "
    "that depends on the grid (the slow-time FFT, the code correlator, the "
    "reference, and every per-frame scratch buffer), re-derives the threshold "
    "ladder for the pinned grid from the same physics __init__ used, and "
    "clears in-flight accumulation (ring contents, the non-coherent power "
    "accumulator, dwell bookkeeping) -- call between push() calls, never a "
    "substitute for one. Raises ValueError if doppler_bins is outside [1, "
    "reps] or n_noncoh is outside [1, 256] (the internal non-coherent-look "
    "safety-valve ceiling).\n"
    "\n"
    "Forwards to acq_configure_search_raw() on the embedded engine (see its\n"
    "doc comment in acq_core.h): resizes every grid-dependent buffer/plan,\n"
    "re-derives the threshold ladder for the pinned grid, and clears\n"
    "in-flight accumulation — call between push() calls, never a substitute\n"
    "for one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Coherent depth to pin, in `[1, reps]`.\n"
    "n_noncoh : int\n"
    "    Non-coherent look count to pin, in `[1,\n"
    "    ACQ_N_NONCOH_SAFETY_CEILING]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstAcquisition\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), "
    "4).astype(np.complex64)\n"
    ">>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,\n"
    "...                      cn0_dbhz=50.0)\n"
    ">>> b.configure_search_raw(doppler_bins=4, n_noncoh=2)  # pin the grid\n"
    ">>> b.doppler_bins, b.n_noncoh\n"
    "(4, 2)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 8).astype(np.complex64)\n"
    ">>> b.push(burst)[0][:2]      # detects at the pinned grid\n"
    "(0, 17)\n" },
  { "state_bytes", (PyCFunction)BurstAcquisitionObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the BurstAcquisitionObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)BurstAcquisitionObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the BurstAcquisitionObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)BurstAcquisitionObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the BurstAcquisitionObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)BurstAcquisitionObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on "
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does "
    "nothing.\n"
    "Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)BurstAcquisitionObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a BurstAcq be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BurstAcq\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)BurstAcquisitionObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the BurstAcq.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL }
};

static PyTypeObject BurstAcquisitionObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.BurstAcquisition",
  .tp_basicsize                           = sizeof (BurstAcquisitionObject),
  .tp_dealloc = (destructor)BurstAcquisitionObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a burst-mode acquisition engine (forwards to acq_create_burst() "
    "-- see its doc comment in acq_core.h for the full physics).\n",
  .tp_methods = BurstAcquisitionObj_methods,
  .tp_getset  = BurstAcquisition_getset,
  .tp_new     = BurstAcquisitionObj_new,
  .tp_init    = (initproc)BurstAcquisitionObj_init,
};
