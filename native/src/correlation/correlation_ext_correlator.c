/* jm:generated correlation_ext_correlator.c */
/*
 * correlation_ext_correlator.c — Correlator type for the correlation module.
 *
 * Included by correlation_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in correlation_ext_correlator_extra.c.
 * Do NOT compile this file directly — only correlation_ext.c is compiled.
 */
/* ======================================================== */
/* CorrelatorObject — wraps dp_correlator_state_t *       */
/* ======================================================== */

#include "doppler/correlator/correlator_core.h"

typedef struct
{
  PyObject_HEAD dp_correlator_state_t *handle;
  dp_correlator_state_t *_jm_parked;      /* gh-2187: freed by tp_dealloc */
  jm_inflight_t          _jm_inflight;    /* gh-2272: GIL-free calls running */
  PyObject *_jm_retain_set_telemetry_tlm; /* keeps tlm's owner alive */
} CorrelatorObject;

static void
CorrelatorObj_dealloc (CorrelatorObject *self)
{
  /* gh-2187: destroy() parks the state here instead of
     freeing it under the views this object lent; they are
     gone now, so the free is now. */
  if (self->handle)
    dp_correlator_destroy (self->handle);
  if (self->_jm_parked)
    dp_correlator_destroy (self->_jm_parked);
  jm_inflight_free (&self->_jm_inflight);
  Py_XDECREF (self->_jm_retain_set_telemetry_tlm);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CorrelatorObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  CorrelatorObject *self = (CorrelatorObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CorrelatorObj_init (CorrelatorObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]             = { "ref",
                                              "framing",
                                              "doppler_rows",
                                              "detect",
                                              "dwell",
                                              "ny_out",
                                              "nx_out",
                                              "col_out",
                                              "threshold",
                                              "noise_lo",
                                              "noise_hi",
                                              "noise_mode",
                                              "sample_rate",
                                              "chip_rate",
                                              "symbol_rate",
                                              "reps",
                                              "cn0_dbhz",
                                              "doppler_uncertainty",
                                              "pfa",
                                              "pd",
                                              "doppler_rate",
                                              "rate_hypotheses",
                                              "code_only_epochs",
                                              "nthreads",
                                              NULL };
  PyObject          *ref_obj              = NULL;
  const char        *framing_str          = "none";
  PyObject          *doppler_rows_obj     = NULL;
  unsigned long long doppler_rows_raw     = 0;
  int                detect               = 0;
  PyObject          *dwell_obj            = NULL;
  unsigned long long dwell_raw            = 1;
  PyObject          *ny_out_obj           = NULL;
  unsigned long long ny_out_raw           = 0;
  PyObject          *nx_out_obj           = NULL;
  unsigned long long nx_out_raw           = 0;
  int                col_out              = -1;
  float              threshold            = 0.0f;
  PyObject          *noise_lo_obj         = NULL;
  unsigned long long noise_lo_raw         = 0;
  PyObject          *noise_hi_obj         = NULL;
  unsigned long long noise_hi_raw         = (unsigned long long)-1ULL;
  const char        *noise_mode_str       = "mean";
  double             sample_rate          = 0.0;
  double             chip_rate            = 0.0;
  double             symbol_rate          = 0.0;
  PyObject          *reps_obj             = NULL;
  unsigned long long reps_raw             = 1;
  double             cn0_dbhz             = DP_CORRELATOR_CN0_NONE;
  double             doppler_uncertainty  = 0.0;
  double             pfa                  = 1e-3;
  double             pd                   = 0.9;
  double             doppler_rate         = 0.0;
  PyObject          *rate_hypotheses_obj  = NULL;
  unsigned long long rate_hypotheses_raw  = 1;
  PyObject          *code_only_epochs_obj = NULL;
  unsigned long long code_only_epochs_raw = 1;
  int                nthreads             = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O|sOiOOOifOOsdddOdddddOOi", kwlist, &ref_obj,
          &framing_str, &doppler_rows_obj, &detect, &dwell_obj, &ny_out_obj,
          &nx_out_obj, &col_out, &threshold, &noise_lo_obj, &noise_hi_obj,
          &noise_mode_str, &sample_rate, &chip_rate, &symbol_rate, &reps_obj,
          &cn0_dbhz, &doppler_uncertainty, &pfa, &pd, &doppler_rate,
          &rate_hypotheses_obj, &code_only_epochs_obj, &nthreads))
    return -1;
  int framing = 0;
  if (strcmp (framing_str, "none") == 0)
    framing = 0;
  else if (strcmp (framing_str, "continuous") == 0)
    framing = 1;
  else if (strcmp (framing_str, "burst") == 0)
    framing = 2;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "framing must be one of \"none\", \"continuous\", "
                    "\"burst\", got '%s'",
                    framing_str);
      return -1;
    }
  if (doppler_rows_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (doppler_rows_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      doppler_rows_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((doppler_rows_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || doppler_rows_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "doppler_rows: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t doppler_rows = (size_t)doppler_rows_raw;
  if (dwell_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (dwell_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      dwell_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((dwell_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || dwell_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "dwell: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t dwell = (size_t)dwell_raw;
  if (ny_out_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (ny_out_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      ny_out_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((ny_out_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || ny_out_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "ny_out: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t ny_out = (size_t)ny_out_raw;
  if (nx_out_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (nx_out_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      nx_out_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((nx_out_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || nx_out_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "nx_out: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t nx_out = (size_t)nx_out_raw;
  if (noise_lo_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (noise_lo_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      noise_lo_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((noise_lo_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || noise_lo_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "noise_lo: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t noise_lo = (size_t)noise_lo_raw;
  if (noise_hi_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (noise_hi_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      noise_hi_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((noise_hi_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || noise_hi_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "noise_hi: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t noise_hi   = (size_t)noise_hi_raw;
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
  if (reps_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (reps_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      reps_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((reps_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || reps_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "reps: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t reps = (size_t)reps_raw;
  if (rate_hypotheses_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (rate_hypotheses_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      rate_hypotheses_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((rate_hypotheses_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || rate_hypotheses_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "rate_hypotheses: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t rate_hypotheses = (size_t)rate_hypotheses_raw;
  if (code_only_epochs_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (code_only_epochs_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      code_only_epochs_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((code_only_epochs_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || code_only_epochs_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "code_only_epochs: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         code_only_epochs = (size_t)code_only_epochs_raw;
  const char    *_why             = NULL;
  PyArrayObject *ref_arr
      = jm_array_arg (ref_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "ref");
  if (!ref_arr)
    {
      return -1;
    }
  if (PyArray_NDIM (ref_arr) != 1 && PyArray_NDIM (ref_arr) != 2)
    {
      PyErr_SetString (PyExc_ValueError, "ref must be a 1-D or 2-D array");
      Py_DECREF (ref_arr);
      return -1;
    }
  size_t ref_dim0
      = PyArray_NDIM (ref_arr) == 1 ? 1 : (size_t)PyArray_DIM (ref_arr, 0);
  size_t ref_dim1 = (size_t)PyArray_DIM (ref_arr, PyArray_NDIM (ref_arr) - 1);
  self->handle    = dp_correlator_create (
      (const float _Complex *)PyArray_DATA (ref_arr), ref_dim0, ref_dim1,
      framing, doppler_rows, detect, dwell, ny_out, nx_out, col_out, threshold,
      noise_lo, noise_hi, noise_mode, sample_rate, chip_rate, symbol_rate,
      reps, cn0_dbhz, doppler_uncertainty, pfa, pd, doppler_rate,
      rate_hypotheses, code_only_epochs, nthreads, &_why);
  Py_DECREF (ref_arr);
  if (!self->handle)
    {
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "Correlator: refused, or out of memory "
                         "(docs/design/correlator-python.md section 17: a "
                         "value the flavor would not honor is refused)");
      return -1;
    }
  if (self->handle->search && self->handle->search->underpowered)
    {
      if (PyErr_WarnEx (PyExc_UserWarning,
                        "Correlator: the search cannot meet pd at this "
                        "cn0_dbhz and geometry (pd_predicted < pd, or "
                        "pd_burst < pd for a burst). It still builds a "
                        "best-effort grid. Raise cn0_dbhz, narrow "
                        "doppler_uncertainty, lower pd, or give a burst more "
                        "repetitions.",
                        1)
          < 0)
        return -1;
    }
  return 0;
}

static PyObject *
CorrelatorObj_reset (CorrelatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_correlator_reset (self->handle);
  Py_RETURN_NONE;
}

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static const char *const _enum_Correlator_correlator_flavor[] = {
  "1d",          "2d",         "delay_doppler", "detector_1d",
  "detector_2d", "continuous", "burst",         NULL,
};
static const int _enum_Correlator_correlator_flavor_c[] = {
  DP_CORRELATOR_1D,
  DP_CORRELATOR_2D,
  DP_CORRELATOR_DELAY_DOPPLER,
  DP_CORRELATOR_DETECTOR_1D,
  DP_CORRELATOR_DETECTOR_2D,
  DP_CORRELATOR_SEARCH_CONTINUOUS,
  DP_CORRELATOR_SEARCH_BURST,
};
static inline const char *
_enum_Correlator_correlator_flavor_name (long v)
{
  for (int j = 0; j < 7; j++)
    if (_enum_Correlator_correlator_flavor_c[j] == v)
      return _enum_Correlator_correlator_flavor[j];
  return NULL;
}

static PyObject *
CorrelatorObj_execute_max_out (CorrelatorObject *self,
                               PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_correlator_execute_max_out (self->handle));
}

static PyObject *
CorrelatorObj_execute (CorrelatorObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_correlator_execute_max_out (self->handle);
      size_t _min_cap = _omax ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why  = NULL;
      size_t      n_out = dp_correlator_execute (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap, &_why);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "execute: only a map (framing='none', "
                             "detect=False) executes; Correlator.flavor "
                             "says which this is");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Correlator.execute: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_max_out (self->handle);
  if (!_cap)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Correlator.execute: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why  = NULL;
  size_t      n_out = dp_correlator_execute (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap, &_why);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute: only a map (framing='none', "
                         "detect=False) executes; Correlator.flavor says "
                         "which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Correlator.execute: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
CorrelatorObj_execute_all_max_out (CorrelatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_correlator_execute_all_max_out (self->handle, (size_t)n_in));
}

static PyObject *
CorrelatorObj_execute_all (CorrelatorObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t             n     = PyArray_SIZE (in_arr);
  dp_correlator_state_t *state = self->handle;
  size_t                 _cols = (size_t)((state->n_out ? state->n_out : 1));
  if (_cols == 0)
    {
      PyErr_SetString (PyExc_RuntimeError,
                       "Correlator.execute_all: out_cols evaluated to 0");
      Py_DECREF (in_arr);
      return NULL;
    }
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_NDIM (out_arr) != 2
          || (size_t)PyArray_DIM (out_arr, 1) != _cols)
        {
          PyErr_Format (
              PyExc_ValueError,
              "out must be a 2-D array with %zu columns, shape (rows, %zu)",
              _cols, _cols);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax
          = dp_correlator_execute_all_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why = NULL;
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float _Complex *_ng1 = (float _Complex *)PyArray_DATA (out_arr);
      size_t          n_out;
      void           *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_correlator_execute_all (_jm_h, _ng0, (size_t)n, _ng1, _cap,
                                           &_why);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "execute_all: only a map (framing='none', "
                             "detect=False) runs a batch; "
                             "Correlator.flavor says which this is");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Correlator.execute_all: wrote %zu elements into a "
                        "buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp _odim = (npy_intp)n_out;
      if ((size_t)_odim % _cols != 0)
        {
          PyErr_Format (PyExc_RuntimeError,
                        "Correlator.execute_all: kernel returned %zu "
                        "elements, not a whole number of %zu-wide rows",
                        (size_t)_odim, _cols);
          Py_DECREF (out_arr);
          return NULL;
        }
      npy_intp  _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
      PyObject *_oview = PyArray_SimpleNewFromData (2, _odims, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_all_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.execute_all: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out
        = dp_correlator_execute_all (_jm_h, _ng0, (size_t)n, _d0, _cap, &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute_all: only a map (framing='none', "
                         "detect=False) runs a batch; Correlator.flavor "
                         "says which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Correlator.execute_all: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  npy_intp _odim = (npy_intp)n_out;
  if ((size_t)_odim % _cols != 0)
    {
      PyErr_Format (PyExc_RuntimeError,
                    "Correlator.execute_all: kernel returned %zu elements, "
                    "not a whole number of %zu-wide rows",
                    (size_t)_odim, _cols);
      Py_DECREF (arr0);
      return NULL;
    }
  npy_intp     _odims[2] = { _odim / (npy_intp)_cols, (npy_intp)_cols };
  PyArray_Dims _rs0      = { _odims, 2 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
CorrelatorObj_execute_coherent_max_out (CorrelatorObject *self,
                                        PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_correlator_execute_coherent_max_out (self->handle));
}

static PyObject *
CorrelatorObj_execute_coherent (CorrelatorObject *self, PyObject *args,
                                PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_correlator_execute_coherent_max_out (self->handle);
      size_t _min_cap = _omax ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why = NULL;
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float _Complex *_ng1 = (float _Complex *)PyArray_DATA (out_arr);
      size_t          n_out;
      void           *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_correlator_execute_coherent (_jm_h, _ng0, (size_t)n, _ng1,
                                                _cap, &_why);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "execute_coherent: only a map "
                             "(framing='none', detect=False) runs a batch; "
                             "Correlator.flavor says which this is");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Correlator.execute_coherent: wrote %zu elements into "
                        "a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_coherent_max_out (self->handle);
  if (!_cap)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.execute_coherent: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_execute_coherent (_jm_h, _ng0, (size_t)n, _d0, _cap,
                                            &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute_coherent: only a map (framing='none', "
                         "detect=False) runs a batch; Correlator.flavor "
                         "says which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Correlator.execute_coherent: wrote %zu elements into a "
                    "buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
CorrelatorObj_execute_power_sum_max_out (CorrelatorObject *self,
                                         PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_correlator_execute_power_sum_max_out (self->handle));
}

static PyObject *
CorrelatorObj_execute_power_sum (CorrelatorObject *self, PyObject *args,
                                 PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_correlator_execute_power_sum_max_out (self->handle);
      size_t _min_cap = _omax ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      const char *_why = NULL;
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      float *_ng1 = (float *)PyArray_DATA (out_arr);
      size_t n_out;
      void  *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_correlator_execute_power_sum (_jm_h, _ng0, (size_t)n, _ng1,
                                                 _cap, &_why);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "execute_power_sum: only a map "
                             "(framing='none', detect=False) runs a batch; "
                             "Correlator.flavor says which this is");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Correlator.execute_power_sum: wrote %zu elements "
                        "into a buffer of %zu",
                        (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_power_sum_max_out (self->handle);
  if (!_cap)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.execute_power_sum: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_execute_power_sum (_jm_h, _ng0, (size_t)n, _d0, _cap,
                                             &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute_power_sum: only a map (framing='none', "
                         "detect=False) runs a batch; Correlator.flavor "
                         "says which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Correlator.execute_power_sum: wrote %zu elements into a "
                    "buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
CorrelatorObj_execute_power_max (CorrelatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_power_max_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.execute_power_max: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0 || !arr1)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_DECREF (in_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  uint32_t *_d1 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_execute_power_max (_jm_h, _ng0, (size_t)n, _d0, _d1,
                                             _cap, &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute_power_max: only a map (framing='none', "
                         "detect=False) runs a batch; Correlator.flavor "
                         "says which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      PyErr_Format (PyExc_RuntimeError,
                    "Correlator.execute_power_max: wrote %zu elements into a "
                    "buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (2, arr0, arr1);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v1);
  PyObject *result = PyTuple_Pack (2, arr0, arr1);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  return result;
}

static PyObject *
CorrelatorObj_execute_peaks (CorrelatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &in_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_execute_peaks_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.execute_peaks: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr2  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr3  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0 || !arr1 || !arr2 || !arr3)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_XDECREF (arr2);
      Py_XDECREF (arr3);
      Py_DECREF (in_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  uint32_t *_d1 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  uint32_t *_d2 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr2);
  float    *_d3 = (float *)PyArray_DATA ((PyArrayObject *)arr3);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_execute_peaks (_jm_h, _ng0, (size_t)n, _d0, _d1, _d2,
                                         _d3, _cap, &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "execute_peaks: only a map (framing='none', "
                         "detect=False) runs a batch; Correlator.flavor "
                         "says which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      PyErr_Format (
          PyExc_RuntimeError,
          "Correlator.execute_peaks: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (4, arr0, arr1, arr2, arr3);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v1);
  PyArray_Dims _rs2 = { &_odim, 1 };
  PyObject *v2 = PyArray_Resize ((PyArrayObject *)arr2, &_rs2, 0, NPY_CORDER);
  if (!v2)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v2);
  PyArray_Dims _rs3 = { &_odim, 1 };
  PyObject *v3 = PyArray_Resize ((PyArrayObject *)arr3, &_rs3, 0, NPY_CORDER);
  if (!v3)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      return NULL;
    }
  Py_DECREF (v3);
  PyObject *result = PyTuple_Pack (4, arr0, arr1, arr2, arr3);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  Py_DECREF (arr2);
  Py_DECREF (arr3);
  return result;
}

static PyObject *
CorrelatorObj_bank_power_max (CorrelatorObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "x", "L", "g_min", "g_max", "rates", NULL };
  PyObject          *x_obj     = NULL;
  PyObject          *L_obj     = NULL;
  unsigned long long L_raw     = 0ULL;
  int                g_min     = 0;
  int                g_max     = 0;
  PyObject          *rates_obj = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOiiO", _kwlist, &x_obj,
                                    &L_obj, &g_min, &g_max, &rates_obj))
    return NULL;
  if (L_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (L_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      L_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((L_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || L_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "L: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t         L = (size_t)L_raw;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject *rates_arr    = jm_array_arg (rates_obj, NPY_COMPLEX64,
                                              NPY_ARRAY_C_CONTIGUOUS, "rates");
  if (!rates_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float _Complex *rates
      = (const float _Complex *)PyArray_DATA (rates_arr);
  size_t rates_len = (size_t)PyArray_SIZE (rates_arr);
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_correlator_bank_power_max_max_out (
      self->handle, (size_t)PyArray_SIZE (x_arr));
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.bank_power_max: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0 || !arr1)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  uint32_t *_d1 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int         _status = 0;
  const char *_why    = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t n_out;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_bank_power_max (_jm_h, x, x_len, L, g_min, g_max,
                                          rates, rates_len, _d0, _d1, _cap,
                                          &_status, &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  Py_DECREF (rates_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      switch (_status)
        {
        case DP_ERR_MEMORY:
          if (_why)
            PyErr_SetString (PyExc_MemoryError, _why);
          else
            PyErr_SetString (PyExc_MemoryError,
                             "bank_power_max: out of memory");
          return NULL;
        default:
          break;
        }
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "bank_power_max: only a map (framing='none', "
                         "detect=False) has a bank; Correlator.flavor says "
                         "which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      PyErr_Format (
          PyExc_RuntimeError,
          "Correlator.bank_power_max: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (2, arr0, arr1);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      return NULL;
    }
  Py_DECREF (v1);
  PyObject *result = PyTuple_Pack (2, arr0, arr1);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  return result;
}

static PyObject *
CorrelatorObj_bank_detect (CorrelatorObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "x",           "L_d",       "L_r",   "g_min", "g_max",
          "t_candidate", "t_confirm", "rates", NULL };
  PyObject          *x_obj       = NULL;
  PyObject          *L_d_obj     = NULL;
  unsigned long long L_d_raw     = 0ULL;
  PyObject          *L_r_obj     = NULL;
  unsigned long long L_r_raw     = 0ULL;
  int                g_min       = 0;
  int                g_max       = 0;
  float              t_candidate = 0.0f;
  float              t_confirm   = 0.0f;
  PyObject          *rates_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOOiiffO", _kwlist, &x_obj,
                                    &L_d_obj, &L_r_obj, &g_min, &g_max,
                                    &t_candidate, &t_confirm, &rates_obj))
    return NULL;
  if (L_d_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (L_d_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      L_d_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((L_d_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || L_d_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "L_d: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t L_d = (size_t)L_d_raw;
  if (L_r_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (L_r_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      L_r_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((L_r_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || L_r_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "L_r: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t         L_r = (size_t)L_r_raw;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
  PyArrayObject *rates_arr    = jm_array_arg (rates_obj, NPY_COMPLEX64,
                                              NPY_ARRAY_C_CONTIGUOUS, "rates");
  if (!rates_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  const float _Complex *rates
      = (const float _Complex *)PyArray_DATA (rates_arr);
  size_t rates_len = (size_t)PyArray_SIZE (rates_arr);
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_correlator_bank_detect_max_out (
      self->handle, (size_t)PyArray_SIZE (x_arr));
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.bank_detect: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  PyObject *arr1  = PyArray_SimpleNew (1, &_adim, NPY_INT32);
  PyObject *arr2  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr3  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  PyObject *arr4  = PyArray_SimpleNew (1, &_adim, NPY_UINT32);
  if (!arr0 || !arr1 || !arr2 || !arr3 || !arr4)
    {
      Py_XDECREF (arr0);
      Py_XDECREF (arr1);
      Py_XDECREF (arr2);
      Py_XDECREF (arr3);
      Py_XDECREF (arr4);
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      return NULL;
    }
  float    *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  int32_t  *_d1 = (int32_t *)PyArray_DATA ((PyArrayObject *)arr1);
  uint32_t *_d2 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr2);
  uint32_t *_d3 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr3);
  uint32_t *_d4 = (uint32_t *)PyArray_DATA ((PyArrayObject *)arr4);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      Py_DECREF (x_arr);
      Py_DECREF (rates_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int         _status = 0;
  const char *_why    = NULL;
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t n_out;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_bank_detect (
        _jm_h, x, x_len, L_d, L_r, g_min, g_max, t_candidate, t_confirm, rates,
        rates_len, _d0, _d1, _d2, _d3, _d4, _cap, &_status, &_why);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  Py_DECREF (rates_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      switch (_status)
        {
        case DP_ERR_MEMORY:
          if (_why)
            PyErr_SetString (PyExc_MemoryError, _why);
          else
            PyErr_SetString (PyExc_MemoryError, "bank_detect: out of memory");
          return NULL;
        default:
          break;
        }
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "bank_detect: only a map (framing='none', "
                         "detect=False) has a bank; Correlator.flavor says "
                         "which this is");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      PyErr_Format (
          PyExc_RuntimeError,
          "Correlator.bank_detect: wrote %zu elements into a buffer of %zu",
          (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      PyObject *_exact = PyTuple_Pack (5, arr0, arr1, arr2, arr3, arr4);
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return _exact;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return NULL;
    }
  Py_DECREF (v0);
  PyArray_Dims _rs1 = { &_odim, 1 };
  PyObject *v1 = PyArray_Resize ((PyArrayObject *)arr1, &_rs1, 0, NPY_CORDER);
  if (!v1)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return NULL;
    }
  Py_DECREF (v1);
  PyArray_Dims _rs2 = { &_odim, 1 };
  PyObject *v2 = PyArray_Resize ((PyArrayObject *)arr2, &_rs2, 0, NPY_CORDER);
  if (!v2)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return NULL;
    }
  Py_DECREF (v2);
  PyArray_Dims _rs3 = { &_odim, 1 };
  PyObject *v3 = PyArray_Resize ((PyArrayObject *)arr3, &_rs3, 0, NPY_CORDER);
  if (!v3)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return NULL;
    }
  Py_DECREF (v3);
  PyArray_Dims _rs4 = { &_odim, 1 };
  PyObject *v4 = PyArray_Resize ((PyArrayObject *)arr4, &_rs4, 0, NPY_CORDER);
  if (!v4)
    {
      Py_DECREF (arr0);
      Py_DECREF (arr1);
      Py_DECREF (arr2);
      Py_DECREF (arr3);
      Py_DECREF (arr4);
      return NULL;
    }
  Py_DECREF (v4);
  PyObject *result = PyTuple_Pack (5, arr0, arr1, arr2, arr3, arr4);
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  Py_DECREF (arr2);
  Py_DECREF (arr3);
  Py_DECREF (arr4);
  return result;
}

static PyObject *
CorrelatorObj_configure_search_raw (CorrelatorObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]        = { "doppler_bins", "n_noncoh", NULL };
  PyObject          *doppler_bins_obj = NULL;
  unsigned long long doppler_bins_raw = 0ULL;
  PyObject          *n_noncoh_obj     = NULL;
  unsigned long long n_noncoh_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO", _kwlist,
                                    &doppler_bins_obj, &n_noncoh_obj))
    return NULL;
  if (doppler_bins_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (doppler_bins_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      doppler_bins_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((doppler_bins_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || doppler_bins_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (
              PyExc_OverflowError,
              "doppler_bins: %S is out of range for size_t [0, %llu]", _ix,
              (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t doppler_bins = (size_t)doppler_bins_raw;
  if (n_noncoh_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_noncoh_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_noncoh_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_noncoh_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_noncoh_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n_noncoh: %S is out of range for size_t [0, %llu]",
                        _ix, (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n_noncoh = (size_t)n_noncoh_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int _rc = dp_correlator_configure_search_raw (self->handle, doppler_bins,
                                                n_noncoh, &_why);
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_UNSUPPORTED:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (
                PyExc_ValueError,
                "configure_search_raw: only a search (framing='continuous' or "
                "'burst') has a grid to pin; Correlator.flavor says which "
                "this is");
          return NULL;
        case DP_ERR_INVALID:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "configure_search_raw: doppler_bins must lie in "
                             "1..reps and n_noncoh "
                             "in 1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, "
                             "and non-coherent looks "
                             "need rate_hypotheses 1");
          return NULL;
        case DP_ERR_MEMORY:
          if (_why)
            PyErr_SetString (PyExc_MemoryError, _why);
          else
            PyErr_SetString (PyExc_MemoryError,
                             "configure_search_raw: rebuilding the grid could "
                             "not allocate; the "
                             "engine keeps its prior grid");
          return NULL;
        default:
          break;
        }
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                      "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_set_max_peaks (CorrelatorObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  PyObject          *n_obj     = NULL;
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &n_obj))
    return NULL;
  if (n_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t n = (size_t)n_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int         _rc  = dp_correlator_set_max_peaks (self->handle, n, &_why);
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_UNSUPPORTED:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "set_max_peaks: only a search "
                             "(framing='continuous' or 'burst') lists "
                             "peaks; Correlator.flavor says which this is");
          return NULL;
        case DP_ERR_INVALID:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (
                PyExc_ValueError,
                "set_max_peaks: n must lie in 1..DP_CORRELATOR_MAX_PEAKS");
          return NULL;
        default:
          break;
        }
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_max_peaks failed",
                      (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_set_carrier_freq_hz (CorrelatorObject *self, PyObject *args,
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_correlator_set_carrier_freq_hz (self->handle, carrier_freq_hz);
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_UNSUPPORTED:
          PyErr_SetString (PyExc_ValueError,
                           "set_carrier_freq_hz: only the continuous search "
                           "(framing='continuous') takes a carrier; "
                           "Correlator.flavor says which "
                           "this is");
          return NULL;
        case DP_ERR_INVALID:
          PyErr_SetString (PyExc_ValueError,
                           "set_carrier_freq_hz: must be 0 (no carrier) or "
                           "finite and above "
                           "sample_rate / 2");
          return NULL;
        case DP_ERR_MEMORY:
          PyErr_SetString (
              PyExc_MemoryError,
              "set_carrier_freq_hz: rebuilding the delay-Doppler map for the "
              "new "
              "carrier could not allocate; the engine is unchanged");
          return NULL;
        default:
          break;
        }
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "set_carrier_freq_hz failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_set_threads (CorrelatorObject *self, PyObject *args,
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_correlator_set_threads (self->handle, n);
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_UNSUPPORTED:
          PyErr_SetString (PyExc_ValueError,
                           "set_threads: only a search (framing='continuous' "
                           "or 'burst') sizes "
                           "its pool after create; a map or detector takes "
                           "nthreads at create");
          return NULL;
        default:
          break;
        }
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_threads failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_set_telemetry (CorrelatorObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "tlm", "prefix", "decim", NULL };
  PyObject    *tlm_obj   = Py_None;
  const char  *prefix    = NULL;
  long long    decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Os|L", _kwlist, &tlm_obj,
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
            {
              if (!PyErr_ExceptionMatches (PyExc_AttributeError))
                return NULL;
              PyErr_Clear ();
              PyErr_Format (PyExc_TypeError,
                            "tlm must be the doppler.telemetry.dp_tlm capsule"
                            " or an object exposing it as ._capsule,"
                            " not %s",
                            Py_TYPE (tlm_obj)->tp_name);
              return NULL;
            }
        }
      tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap,
                                              "doppler.telemetry.dp_tlm");
      Py_DECREF (tlm_cap);
      if (!tlm)
        return NULL;
    }
  if (decim_raw < 0 || decim_raw > (long long)UINT32_MAX)
    {
      PyErr_Format (PyExc_OverflowError,
                    "decim: %lld is out of range for uint32_t [%lld, %lld]",
                    (long long)decim_raw, (long long)0, (long long)UINT32_MAX);
      return NULL;
    }
  uint32_t decim = (uint32_t)decim_raw;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  const char *_why = NULL;
  int         _rc
      = dp_correlator_set_telemetry (self->handle, tlm, prefix, decim, &_why);
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_UNSUPPORTED:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (
                PyExc_ValueError,
                "set_telemetry: only the continuous search "
                "(framing='continuous') is "
                "instrumented; Correlator.flavor says which this is");
          return NULL;
        case DP_ERR_INVALID:
          if (_why)
            PyErr_SetString (PyExc_ValueError, _why);
          else
            PyErr_SetString (PyExc_ValueError,
                             "set_telemetry: refused, nothing attached: the "
                             "prefix joined to each "
                             "probe name must fit DP_TLM_NAME_MAX, decim must "
                             "be at least 1, and "
                             "the context must have room for all ten probes");
          return NULL;
        case DP_ERR_MEMORY:
          if (_why)
            PyErr_SetString (PyExc_MemoryError, _why);
          else
            PyErr_SetString (PyExc_MemoryError,
                             "set_telemetry: the attach's scratch context "
                             "could not be allocated; "
                             "nothing is attached");
          return NULL;
        default:
          break;
        }
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                      (long long)_rc);
      return NULL;
    }
  /* retain: the kernel keeps `tlm`, so this object keeps its owner
     until the next call replaces it or None detaches it. */
  if (tlm_obj != Py_None)
    Py_INCREF (tlm_obj);
  Py_XSETREF (self->_jm_retain_set_telemetry_tlm,
              tlm_obj != Py_None ? tlm_obj : NULL);
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_surface (CorrelatorObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *out_arr = jm_array_arg (
      out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  float *out     = (float *)PyArray_DATA (out_arr);
  size_t out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_correlator_surface (self->handle, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "surface: only the continuous search "
                       "(framing='continuous') has it; Correlator.flavor "
                       "says which this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_surface_doppler_hz (CorrelatorObject *self, PyObject *args,
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
  PyArrayObject *out_arr
      = jm_array_arg (out_obj, NPY_DOUBLE,
                      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  double *out     = (double *)PyArray_DATA (out_arr);
  size_t  out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_correlator_surface_doppler_hz (self->handle, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "surface_doppler_hz: only the continuous search "
                       "(framing='continuous') has it; Correlator.flavor "
                       "says which this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_surface_chip_phase (CorrelatorObject *self, PyObject *args,
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
  PyArrayObject *out_arr
      = jm_array_arg (out_obj, NPY_DOUBLE,
                      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  double *out     = (double *)PyArray_DATA (out_arr);
  size_t  out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_correlator_surface_chip_phase (self->handle, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "surface_chip_phase: only the continuous search "
                       "(framing='continuous') has it; Correlator.flavor "
                       "says which this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_surface_complex (CorrelatorObject *self, PyObject *args,
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
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr
      = jm_array_arg (out_obj, NPY_COMPLEX64,
                      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  float _Complex *out     = (float _Complex *)PyArray_DATA (out_arr);
  size_t          out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_correlator_surface_complex (self->handle, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "surface_complex: only a search (framing='continuous' "
                       "or 'burst') has it; Correlator.flavor says which "
                       "this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_block_prompt (CorrelatorObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "tile", "col", "out", NULL };
  PyObject          *tile_obj  = NULL;
  unsigned long long tile_raw  = 0ULL;
  PyObject          *col_obj   = NULL;
  unsigned long long col_raw   = 0ULL;
  PyObject          *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OOO", _kwlist, &tile_obj,
                                    &col_obj, &out_obj))
    return NULL;
  if (tile_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (tile_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      tile_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((tile_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || tile_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "tile: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t tile = (size_t)tile_raw;
  if (col_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (col_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      col_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((col_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || col_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "col: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }
  size_t col = (size_t)col_raw;
  /* Require the exact dtype AND C-contiguity — either mismatch makes
   * the marshal write into a temp copy, not the caller's buffer. */
  if (!PyArray_Check (out_obj)
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr
      = jm_array_arg (out_obj, NPY_COMPLEX64,
                      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  float _Complex *out     = (float _Complex *)PyArray_DATA (out_arr);
  size_t          out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y
      = dp_correlator_block_prompt (self->handle, tile, col, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "block_prompt: only the continuous search "
                       "(framing='continuous') has it; Correlator.flavor "
                       "says which this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_block_raw (CorrelatorObject *self, PyObject *args,
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
      || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
      || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
      || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
    {
      PyErr_SetString (PyExc_TypeError, "out must be a writable, C-contiguous"
                                        " ndarray of the output dtype");
      return NULL;
    }
  PyArrayObject *out_arr
      = jm_array_arg (out_obj, NPY_COMPLEX64,
                      NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
  if (!out_arr)
    {
      return NULL;
    }
  float _Complex *out     = (float _Complex *)PyArray_DATA (out_arr);
  size_t          out_len = (size_t)PyArray_SIZE (out_arr);
  if (!self->handle)
    {
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t y = dp_correlator_block_raw (self->handle, out, out_len);
  Py_DECREF (out_arr);
  if (y == (SIZE_MAX))
    {
      PyErr_SetString (PyExc_ValueError,
                       "block_raw: only the continuous search "
                       "(framing='continuous') has it; Correlator.flavor "
                       "says which this is");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
CorrelatorObj_push_max_out (CorrelatorObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n_in = 0;
  if (!PyArg_ParseTuple (args, "n", &n_in))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      dp_correlator_push_max_out (self->handle, (size_t)n_in));
}

static PyArray_Descr *CorrelatorObj_push_dtype = NULL;

/* The record's numpy dtype, built from the compiler's own layout:
   offsetof/sizeof, never numpy's packing rules, so a padded
   struct cannot silently read every row after the first from the
   wrong bytes. */
static PyArray_Descr *
CorrelatorObj_push_get_dtype (void)
{
  PyObject      *names = NULL, *formats = NULL;
  PyObject      *offsets = NULL, *spec = NULL;
  PyArray_Descr *out = NULL;
  if (CorrelatorObj_push_dtype)
    {
      Py_INCREF (CorrelatorObj_push_dtype);
      return CorrelatorObj_push_dtype;
    }
  names = Py_BuildValue ("[ssssssssss]", "row", "col", "peak_mag", "noise_est",
                         "test_stat", "cn0_dbhz_est", "samples_consumed",
                         "chip_phase", "doppler_rate_hz_s", "test_stat_peak");
  if (!names)
    goto done;
  formats = PyList_New (10);
  if (!formats)
    goto done;
  PyList_SET_ITEM (formats, 0, (PyObject *)PyArray_DescrFromType (NPY_UINTP));
  PyList_SET_ITEM (formats, 1, (PyObject *)PyArray_DescrFromType (NPY_UINTP));
  PyList_SET_ITEM (formats, 2, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  PyList_SET_ITEM (formats, 3, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  PyList_SET_ITEM (formats, 4, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  PyList_SET_ITEM (formats, 5, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  PyList_SET_ITEM (formats, 6, (PyObject *)PyArray_DescrFromType (NPY_UINT64));
  PyList_SET_ITEM (formats, 7, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 8, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 9, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  offsets = Py_BuildValue (
      "[nnnnnnnnnn]", (Py_ssize_t)offsetof (dp_correlator_hit_t, row),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, col),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, peak_mag),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, noise_est),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, test_stat),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, cn0_dbhz_est),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, samples_consumed),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, chip_phase),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, doppler_rate_hz_s),
      (Py_ssize_t)offsetof (dp_correlator_hit_t, test_stat_peak));
  if (!offsets)
    goto done;
  spec = Py_BuildValue ("{s:O,s:O,s:O,s:n}", "names", names, "formats",
                        formats, "offsets", offsets, "itemsize",
                        (Py_ssize_t)sizeof (dp_correlator_hit_t));
  if (!spec)
    goto done;
  if (!PyArray_DescrConverter (spec, &out))
    out = NULL;
done:
  Py_XDECREF (names);
  Py_XDECREF (formats);
  Py_XDECREF (offsets);
  Py_XDECREF (spec);
  if (out)
    {
      CorrelatorObj_push_dtype = out;
      Py_INCREF (CorrelatorObj_push_dtype);
    }
  return out;
}

static PyObject *
CorrelatorObj_push (CorrelatorObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact record dtype AND C-contiguity. Compared with
       * EquivTypes against the generated descr: a structured array's type
       * num is NPY_VOID, so a scalar enum cannot say what layout is
       * wanted, and coercing to one would silently reinterpret the
       * caller's buffer. */
      {
        PyArray_Descr *_want = CorrelatorObj_push_get_dtype ();
        if (!_want)
          {
            Py_DECREF (in_arr);
            return NULL;
          }
        if (!PyArray_Check (out_obj)
            || !PyArray_EquivTypes (PyArray_DESCR ((PyArrayObject *)out_obj),
                                    _want)
            || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
            || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
          {
            PyErr_Format (
                PyExc_TypeError,
                "out must be a writable, C-contiguous ndarray of"
                " dtype %R, not %R",
                _want,
                PyArray_Check (out_obj)
                    ? (PyObject *)PyArray_DESCR ((PyArrayObject *)out_obj)
                    : Py_None);
            Py_DECREF (_want);
            Py_DECREF (in_arr);
            return NULL;
          }
        Py_DECREF (_want);
      }
      PyArrayObject *out_arr = (PyArrayObject *)out_obj;
      Py_INCREF (out_arr);
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_correlator_push_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      const float _Complex *_ng0
          = (const float _Complex *)PyArray_DATA (in_arr);
      dp_correlator_hit_t *_ng1
          = (dp_correlator_hit_t *)PyArray_DATA (out_arr);
      size_t n_out;
      void  *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_correlator_push (_jm_h, _ng0, (size_t)n, _ng1, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "push: only a detector or a search pushes; a map "
                           "does not detect (build one with detect=True, or "
                           "framing='continuous' or 'burst')");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Correlator.push: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      npy_intp       _odim   = (npy_intp)n_out;
      PyArray_Descr *_vdescr = CorrelatorObj_push_get_dtype ();
      if (!_vdescr)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyObject *_oview
          = PyArray_NewFromDescr (&PyArray_Type, _vdescr, 1, &_odim, NULL,
                                  PyArray_DATA (out_arr), 0, NULL);
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_correlator_push_max_out (self->handle, (size_t)n);
  (void)_need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Correlator.push: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp       _adim  = (npy_intp)_adim_need;
  PyArray_Descr *_descr = CorrelatorObj_push_get_dtype ();
  if (!_descr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  PyObject *arr0 = PyArray_NewFromDescr (&PyArray_Type, _descr, 1, &_adim,
                                         NULL, NULL, 0, NULL);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  dp_correlator_hit_t *_d0
      = (dp_correlator_hit_t *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  const float _Complex *_ng0 = (const float _Complex *)PyArray_DATA (in_arr);
  size_t                n_out;
  void                 *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_correlator_push (_jm_h, _ng0, (size_t)n, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "push: only a detector or a search pushes; a map does "
                       "not detect (build one with detect=True, or "
                       "framing='continuous' or 'burst')");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Correlator.push: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
CorrelatorObj_state_bytes (CorrelatorObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_correlator_state_bytes (self->handle));
}

static PyObject *
CorrelatorObj_get_state (CorrelatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_correlator_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_correlator_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CorrelatorObj_set_state (CorrelatorObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg)
      != dp_correlator_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  int _rc = dp_correlator_set_state (self->handle, PyBytes_AS_STRING (arg));
  if (_rc != 0)
    {
      switch (_rc)
        {
        case DP_ERR_MEMORY:
          PyErr_SetString (
              PyExc_MemoryError,
              "set_state: the blob's carrier needs the delay-Doppler map "
              "rebuilt, "
              "and the rebuild could not allocate; the engine is unchanged");
          return NULL;
        default:
          break;
        }
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Correlator_getprop_flavor (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long        _v = (long)(dp_correlator_flavor (self->handle));
  const char *_s = _enum_Correlator_correlator_flavor_name (_v);
  if (!_s)
    {
      PyErr_Format (
          PyExc_ValueError,
          "flavor holds correlator_flavor value %ld, which is none of"
          " its choices",
          _v);
      return NULL;
    }
  return PyUnicode_FromString (_s);
}
static PyObject *
Correlator_getprop_last_corr (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_correlator_last_corr_status (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "last_corr: only a detector (framing='none', "
                    "detect=True) has it; Correlator.flavor says which this "
                    "is",
                    (long long)_rc);
      return NULL;
    }
  if (!self->handle->_last_corr_valid)
    Py_RETURN_NONE;
  size_t _dim_need = (size_t)(self->handle->n);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (
          PyExc_OverflowError,
          "Correlator.last_corr: output of %zu elements is too large",
          _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *arr  = PyArray_SimpleNewFromData (1, &_dim, NPY_COMPLEX64,
                                              (void *)(self->handle->det_out));
  if (!arr)
    return NULL;
  Py_INCREF (self);
  if (PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self) < 0)
    {
      Py_DECREF (self);
      Py_DECREF (arr);
      return NULL;
    }
  return arr;
}
static PyObject *
Correlator_getprop_keep_surface (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _got = 0;
  int _rc  = dp_correlator_get_keep_surface (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "keep_surface: only the continuous search "
                    "(framing='continuous') keeps a surface",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_got);
}
static int
Correlator_setprop_keep_surface (CorrelatorObject *self, PyObject *value,
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  int _rc = dp_correlator_set_keep_surface (self->handle, v);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "keep_surface: only the continuous search "
                    "(framing='continuous') keeps a surface",
                    (long long)_rc);
      return -1;
    }
  return 0;
}
static PyObject *
Correlator_getprop_ny (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_ny (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "ny: only a map or a detector (framing='none') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_nx (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_nx (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "nx: only a map or a detector (framing='none') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_ny_out (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_ny_out (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "ny_out: only a map (framing='none', detect=False) has "
                    "it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_nx_out (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_nx_out (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "nx_out: only the 1-D or 2-D map (framing='none', "
                    "detect=False, doppler_rows=0) has it; Correlator.flavor "
                    "says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_col_out (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _got = 0;
  int _rc  = dp_correlator_get_col_out (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "col_out: only the 1-D or 2-D map (framing='none', "
                    "detect=False, doppler_rows=0) has it; Correlator.flavor "
                    "says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_got);
}
static PyObject *
Correlator_getprop_doppler_step (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_doppler_step (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "doppler_step: only the delay-Doppler map (doppler_rows "
                    "> 0) has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_n (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_n (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "n: only the 1-D map or a detector (detect=True) has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_dwell (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_dwell (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "dwell: only a map or a detector (framing='none') has "
                    "it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_count (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_count (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "count: only a map or a detector (framing='none') has "
                    "it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_ring_cap (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_ring_cap (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "ring_cap: only a detector (detect=True) or a search "
                    "(framing='continuous' or 'burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_noise_lo (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_noise_lo (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "noise_lo: only a detector (detect=True) or a search "
                    "(framing='continuous' or 'burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_noise_hi (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_noise_hi (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "noise_hi: only a detector (detect=True) or a search "
                    "(framing='continuous' or 'burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_threshold (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float _got = 0.0f;
  int   _rc  = dp_correlator_get_threshold (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "threshold: only a detector (detect=True) or a search "
                    "(framing='continuous' or 'burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble ((double)_got);
}
static PyObject *
Correlator_getprop_max_peaks (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_max_peaks (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "max_peaks: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_code_bins (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_code_bins (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "code_bins: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_doppler_bins (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_doppler_bins (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "doppler_bins: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_n_noncoh (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_n_noncoh (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "n_noncoh: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_eta (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float _got = 0.0f;
  int   _rc  = dp_correlator_get_eta (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "eta: only a search (framing='continuous' or 'burst') "
                    "has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble ((double)_got);
}
static PyObject *
Correlator_getprop_eta_nc (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float _got = 0.0f;
  int   _rc  = dp_correlator_get_eta_nc (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "eta_nc: only a search (framing='continuous' or 'burst') "
                    "has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble ((double)_got);
}
static PyObject *
Correlator_getprop_pfa_cell (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_pfa_cell (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "pfa_cell: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_pd_predicted (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_pd_predicted (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "pd_predicted: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_straddle_loss (CorrelatorObject *self,
                                  void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_straddle_loss (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "straddle_loss: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_sample_rate (CorrelatorObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_sample_rate (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "sample_rate: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_cn0_dbhz (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_cn0_dbhz (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "cn0_dbhz: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_doppler_span_hz (CorrelatorObject *self,
                                    void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_doppler_span_hz (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "doppler_span_hz: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_doppler_res_hz (CorrelatorObject *self,
                                   void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_doppler_res_hz (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "doppler_res_hz: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_pd (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_pd (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "pd: only a search (framing='continuous' or 'burst') has "
                    "it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_underpowered (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  bool _got = 0;
  int  _rc  = dp_correlator_get_underpowered (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "underpowered: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyBool_FromLong ((long)(_got));
}
static PyObject *
Correlator_getprop_threads (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _got = 0;
  int _rc  = dp_correlator_get_threads (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "threads: only a search (framing='continuous' or "
                    "'burst') has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_got);
}
static PyObject *
Correlator_getprop_lag_bins (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_lag_bins (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "lag_bins: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_coherent_bins (CorrelatorObject *self,
                                  void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_coherent_bins (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "coherent_bins: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_sf (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_sf (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "sf: only the continuous search (framing='continuous') "
                    "has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_chip_rate (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_chip_rate (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "chip_rate: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_symbol_rate (CorrelatorObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_symbol_rate (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "symbol_rate: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_epochs_per_symbol (CorrelatorObject *self,
                                      void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_epochs_per_symbol (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "epochs_per_symbol: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_n_peaks (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_n_peaks (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "n_peaks: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_n_held (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_n_held (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "n_held: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_peak_conc (CorrelatorObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float _got = 0.0f;
  int   _rc  = dp_correlator_get_peak_conc (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "peak_conc: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble ((double)_got);
}
static PyObject *
Correlator_getprop_carrier_freq_hz (CorrelatorObject *self,
                                    void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_carrier_freq_hz (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "carrier_freq_hz: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_surface_rows (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_surface_rows (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "surface_rows: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_surface_at (CorrelatorObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_surface_at (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "surface_at: only the continuous search "
                    "(framing='continuous') has it; Correlator.flavor says "
                    "which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_reps (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_reps (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "reps: only the burst search (framing='burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}
static PyObject *
Correlator_getprop_pd_burst (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_pd_burst (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "pd_burst: only the burst search (framing='burst') has "
                    "it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_psl_db (CorrelatorObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_psl_db (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "psl_db: only the burst search (framing='burst') has it; "
                    "Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_doppler_rate (CorrelatorObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double _got = 0.0;
  int    _rc  = dp_correlator_get_doppler_rate (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "doppler_rate: only the burst search (framing='burst') "
                    "has it; Correlator.flavor says which this is",
                    (long long)_rc);
      return NULL;
    }
  return PyFloat_FromDouble (_got);
}
static PyObject *
Correlator_getprop_rate_hypotheses (CorrelatorObject *self,
                                    void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _got = 0;
  int    _rc  = dp_correlator_get_rate_hypotheses (self->handle, &_got);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_AttributeError, "%s (rc=%lld)",
                    "rate_hypotheses: only the burst search "
                    "(framing='burst') has it; Correlator.flavor says which "
                    "this is",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)_got);
}

static PyGetSetDef Correlator_getset[] = {
  { "flavor", (getter)Correlator_getprop_flavor, NULL,
    "Which flavor the knobs built: '1d', '2d' or 'delay_doppler' (maps), "
    "'detector_1d' or 'detector_2d', 'continuous' or 'burst' (searches). Ask "
    "it before reading a member only some flavors serve.\n",
    NULL },
  { "last_corr", (getter)Correlator_getprop_last_corr, NULL,
    "A detector's correlation vector from the most recent push() that "
    "produced a result (None before that, and after reset() or set_state()); "
    "AttributeError on any flavor but a detector. A zero-copy view into a "
    "buffer the detector reuses every push(): copy it before the next push() "
    "to keep it.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_last_corr_status`` returns non-zero\n"
    "    before the view is built. The exception message is ``last_corr:\n"
    "    only a detector (framing='none', detect=True) has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "keep_surface", (getter)Correlator_getprop_keep_surface,
    (setter)Correlator_setprop_keep_surface,
    "1 keeps every decided dwell's surface for surface() (normalized into the "
    "gate's units); 0 (the default) costs nothing. The continuous search "
    "only: elsewhere a read or a write raises AttributeError.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_keep_surface`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``keep_surface: only the continuous search (framing='continuous')\n"
    "    keeps a surface``, with the return code appended.\n"
    "AttributeError\n"
    "    On assignment, if ``dp_correlator_set_keep_surface`` refuses the\n"
    "    value by returning non-zero. The exception message is\n"
    "    ``keep_surface: only the continuous search (framing='continuous')\n"
    "    keeps a surface``, with the return code appended.\n",
    NULL },
  { "ny", (getter)Correlator_getprop_ny, NULL,
    "Rows of the reference: 1 for a 1-D one; the delay-Doppler map's Doppler "
    "rows (doppler_rows).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_ny`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``ny: only a map or a\n"
    "    detector (framing='none') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "nx", (getter)Correlator_getprop_nx, NULL,
    "Samples a reference row: the correlation's native lag count.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_nx`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``nx: only a map or a\n"
    "    detector (framing='none') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "ny_out", (getter)Correlator_getprop_ny_out, NULL,
    "Rows a dump writes: ny unless padded (ny_out), which interpolates the "
    "Doppler or row axis.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_ny_out`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``ny_out: only a map\n"
    "    (framing='none', detect=False) has it; Correlator.flavor says which\n"
    "    this is``, with the return code appended.\n",
    NULL },
  { "nx_out", (getter)Correlator_getprop_nx_out, NULL,
    "Lags a dump writes per row: nx unless padded (nx_out), which "
    "interpolates the lag axis. The one padding name at any row count.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_nx_out`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``nx_out: only the 1-D\n"
    "    or 2-D map (framing='none', detect=False, doppler_rows=0) has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "col_out", (getter)Correlator_getprop_col_out, NULL,
    "The one lag a dump emits per row, or -1 for every lag.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_col_out`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``col_out: only the\n"
    "    1-D or 2-D map (framing='none', detect=False, doppler_rows=0) has\n"
    "    it; Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "doppler_step", (getter)Correlator_getprop_doppler_step, NULL,
    "Doppler bin width of the delay-Doppler map, cycles per sample.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_doppler_step`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``doppler_step: only the delay-Doppler map (doppler_rows > 0) has\n"
    "    it; Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "n", (getter)Correlator_getprop_n, NULL,
    "Frame length in samples: the reference's length, ny * nx.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_n`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``n: only the 1-D map or\n"
    "    a detector (detect=True) has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "dwell", (getter)Correlator_getprop_dwell, NULL,
    "Integration depth: a dump every `dwell` frames.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_dwell`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``dwell: only a map or a\n"
    "    detector (framing='none') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "count", (getter)Correlator_getprop_count, NULL,
    "Frames accumulated toward the next dump (0 ... dwell - 1).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_count`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``count: only a map or a\n"
    "    detector (framing='none') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "ring_cap", (getter)Correlator_getprop_ring_cap, NULL,
    "Input ring capacity in complex samples.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_ring_cap`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``ring_cap: only a\n"
    "    detector (detect=True) or a search (framing='continuous' or\n"
    "    'burst') has it; Correlator.flavor says which this is``, with the\n"
    "    return code appended.\n",
    NULL },
  { "noise_lo", (getter)Correlator_getprop_noise_lo, NULL,
    "First noise (CFAR reference) bin, inclusive.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_noise_lo`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``noise_lo: only a\n"
    "    detector (detect=True) or a search (framing='continuous' or\n"
    "    'burst') has it; Correlator.flavor says which this is``, with the\n"
    "    return code appended.\n",
    NULL },
  { "noise_hi", (getter)Correlator_getprop_noise_hi, NULL,
    "Last noise (CFAR reference) bin, inclusive.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_noise_hi`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``noise_hi: only a\n"
    "    detector (detect=True) or a search (framing='continuous' or\n"
    "    'burst') has it; Correlator.flavor says which this is``, with the\n"
    "    return code appended.\n",
    NULL },
  { "threshold", (getter)Correlator_getprop_threshold, NULL,
    "The gate on the test statistic: a detector's as given (0 fires on every "
    "dump); a search's derived from pfa (the coherent path's gate).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_threshold`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``threshold:\n"
    "    only a detector (detect=True) or a search (framing='continuous' or\n"
    "    'burst') has it; Correlator.flavor says which this is``, with the\n"
    "    return code appended.\n",
    NULL },
  { "max_peaks", (getter)Correlator_getprop_max_peaks, NULL,
    "The peak list's capacity per dwell (1 = the classic gated maximum); set "
    "with set_max_peaks().\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_max_peaks`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``max_peaks:\n"
    "    only a search (framing='continuous' or 'burst') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "code_bins", (getter)Correlator_getprop_code_bins, NULL,
    "One period of the reference in samples: the length of every forward "
    "transform and of a raw epoch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_code_bins`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``code_bins:\n"
    "    only a search (framing='continuous' or 'burst') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "doppler_bins", (getter)Correlator_getprop_doppler_bins, NULL,
    "Doppler hypotheses searched, window tiles times the coherent depth: the "
    "count to fold a hit's row over (bin_to_signed(row, doppler_bins) * "
    "doppler_res_hz is the signed Doppler).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_doppler_bins`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``doppler_bins: only a search (framing='continuous' or 'burst') has\n"
    "    it; Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "n_noncoh", (getter)Correlator_getprop_n_noncoh, NULL,
    "Non-coherent looks per detection (1 = pure coherent).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_n_noncoh`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``n_noncoh: only a\n"
    "    search (framing='continuous' or 'burst') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "eta", (getter)Correlator_getprop_eta, NULL,
    "Raw per-cell Rayleigh amplitude threshold.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_eta`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``eta: only a search\n"
    "    (framing='continuous' or 'burst') has it; Correlator.flavor says\n"
    "    which this is``, with the return code appended.\n",
    NULL },
  { "eta_nc", (getter)Correlator_getprop_eta_nc, NULL,
    "Non-coherent CFAR threshold (order-N_nc Marcum).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_eta_nc`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``eta_nc: only a\n"
    "    search (framing='continuous' or 'burst') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "pfa_cell", (getter)Correlator_getprop_pfa_cell, NULL,
    "Sidak per-cell false-alarm probability over the searched cells.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_pfa_cell`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``pfa_cell: only a\n"
    "    search (framing='continuous' or 'burst') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "pd_predicted", (getter)Correlator_getprop_pd_predicted, NULL,
    "Predicted Pd at cn0_dbhz and the chosen grid, averaged over the straddle "
    "priors (slow-time scalloping, intra-segment rotation, code-phase sample "
    "offset): what the Monte-Carlo characterization measures, not the on-grid "
    "best case.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_pd_predicted`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``pd_predicted: only a search (framing='continuous' or 'burst') has\n"
    "    it; Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "straddle_loss", (getter)Correlator_getprop_straddle_loss, NULL,
    "Mean amplitude derating of the correlation peak from grid straddle, a "
    "diagnostic; 20*log10(straddle_loss) is the loss in dB.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_straddle_loss`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``straddle_loss: only a search (framing='continuous' or 'burst')\n"
    "    has it; Correlator.flavor says which this is``, with the return\n"
    "    code appended.\n",
    NULL },
  { "sample_rate", (getter)Correlator_getprop_sample_rate, NULL,
    "Sample rate the search runs at, Hz: the constructor's sample_rate.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_sample_rate`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``sample_rate:\n"
    "    only a search (framing='continuous' or 'burst') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "cn0_dbhz", (getter)Correlator_getprop_cn0_dbhz, NULL,
    "Carrier-to-noise density the search was sized for, dB-Hz; NaN for no "
    "design point (the burst search only).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_cn0_dbhz`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``cn0_dbhz: only a\n"
    "    search (framing='continuous' or 'burst') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "doppler_span_hz", (getter)Correlator_getprop_doppler_span_hz, NULL,
    "Native unambiguous Doppler half-range, Hz: sample_rate / (2 * "
    "code_bins).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_doppler_span_hz`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``doppler_span_hz: only a search (framing='continuous' or 'burst')\n"
    "    has it; Correlator.flavor says which this is``, with the return\n"
    "    code appended.\n",
    NULL },
  { "doppler_res_hz", (getter)Correlator_getprop_doppler_res_hz, NULL,
    "Doppler bin width, Hz.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_doppler_res_hz`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``doppler_res_hz: only a search (framing='continuous' or 'burst')\n"
    "    has it; Correlator.flavor says which this is``, with the return\n"
    "    code appended.\n",
    NULL },
  { "pd", (getter)Correlator_getprop_pd, NULL,
    "Target detection probability.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_pd`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``pd: only a search\n"
    "    (framing='continuous' or 'burst') has it; Correlator.flavor says\n"
    "    which this is``, with the return code appended.\n",
    NULL },
  { "underpowered", (getter)Correlator_getprop_underpowered, NULL,
    "True when the search cannot meet pd at cn0_dbhz and its geometry: "
    "pd_predicted < pd for the continuous search, pd_burst < pd for the burst "
    "search. It still builds a best-effort grid, and construction warns "
    "(UserWarning).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_underpowered`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``underpowered: only a search (framing='continuous' or 'burst') has\n"
    "    it; Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "threads", (getter)Correlator_getprop_threads, NULL,
    "Workers the search fans its tiles across, the calling thread included; 1 "
    "= serial. Set with set_threads().\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_threads`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``threads: only a\n"
    "    search (framing='continuous' or 'burst') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "lag_bins", (getter)Correlator_getprop_lag_bins, NULL,
    "Code-phase columns of the surface: code_bins unless code_bins is not "
    "7-smooth, then the next power of two (an exact interpolation). "
    "surface_chip_phase() gives each column in chips.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_lag_bins`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``lag_bins: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "coherent_bins", (getter)Correlator_getprop_coherent_bins, NULL,
    "The block-coherent depth D inside every window tile: epochs per block "
    "and Doppler rows per tile. 1 without a code-only window.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_coherent_bins`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``coherent_bins: only the continuous search (framing='continuous')\n"
    "    has it; Correlator.flavor says which this is``, with the return\n"
    "    code appended.\n",
    NULL },
  { "sf", (getter)Correlator_getprop_sf, NULL,
    "Chips per code period: len(ref).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_sf`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``sf: only the continuous\n"
    "    search (framing='continuous') has it; Correlator.flavor says which\n"
    "    this is``, with the return code appended.\n",
    NULL },
  { "chip_rate", (getter)Correlator_getprop_chip_rate, NULL,
    "Chip rate, Hz.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_chip_rate`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``chip_rate:\n"
    "    only the continuous search (framing='continuous') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "symbol_rate", (getter)Correlator_getprop_symbol_rate, NULL,
    "Data-symbol rate the search was built with, Hz (0 = no known data "
    "clock): a diagnostic, never sizing.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_symbol_rate`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``symbol_rate:\n"
    "    only the continuous search (framing='continuous') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "epochs_per_symbol", (getter)Correlator_getprop_epochs_per_symbol, NULL,
    "(chip_rate / sf) / symbol_rate: code epochs per data symbol; 0 when "
    "symbol_rate is 0.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_epochs_per_symbol`` returns\n"
    "    non-zero instead of writing a value. The exception message is\n"
    "    ``epochs_per_symbol: only the continuous search\n"
    "    (framing='continuous') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "n_peaks", (getter)Correlator_getprop_n_peaks, NULL,
    "Picks in the last decided dwell, held twins included.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_n_peaks`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``n_peaks: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "n_held", (getter)Correlator_getprop_n_held, NULL,
    "Picks of the last decided dwell held as same-code-phase twins rather "
    "than listed (docs/design/async-dsss-receiver.md section 7.1).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_n_held`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``n_held: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``, with the return code appended.\n",
    NULL },
  { "peak_conc", (getter)Correlator_getprop_peak_conc, NULL,
    "Concentration of the last dwell's strongest peak: its main lobe's power "
    "over its code-phase column's, across every Doppler row and tile. Near 1 "
    "for a clean single emitter; about 0.5 when a data transition splits it "
    "into twins (docs/design/async-dsss-receiver.md section 2.4).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_peak_conc`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``peak_conc:\n"
    "    only the continuous search (framing='continuous') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "carrier_freq_hz", (getter)Correlator_getprop_carrier_freq_hz, NULL,
    "RF carrier the Doppler is coupled to, Hz (0.0 = uncoupled); set with "
    "set_carrier_freq_hz().\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_carrier_freq_hz`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``carrier_freq_hz: only the continuous search\n"
    "    (framing='continuous') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "surface_rows", (getter)Correlator_getprop_surface_rows, NULL,
    "Rows of the surface surface() returns: the Doppler axis in surface "
    "units; its columns are lag_bins.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_surface_rows`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``surface_rows: only the continuous search (framing='continuous')\n"
    "    has it; Correlator.flavor says which this is``, with the return\n"
    "    code appended.\n",
    NULL },
  { "surface_at", (getter)Correlator_getprop_surface_at, NULL,
    "samples_consumed of the dwell whose surface surface() returns (0 until "
    "one has been captured).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_surface_at`` returns non-zero\n"
    "    instead of writing a value. The exception message is ``surface_at:\n"
    "    only the continuous search (framing='continuous') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "reps", (getter)Correlator_getprop_reps, NULL,
    "Preamble repetitions a burst holds: the coherence ceiling.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_reps`` returns non-zero instead of\n"
    "    writing a value. The exception message is ``reps: only the burst\n"
    "    search (framing='burst') has it; Correlator.flavor says which this\n"
    "    is``, with the return code appended.\n",
    NULL },
  { "pd_burst", (getter)Correlator_getprop_pd_burst, NULL,
    "Predicted Pd of one burst of reps repetitions at cn0_dbhz: the number "
    "the sizer meets pd with. NaN with no design cn0_dbhz or with n_noncoh > "
    "1.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_pd_burst`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``pd_burst: only the\n"
    "    burst search (framing='burst') has it; Correlator.flavor says which\n"
    "    this is``, with the return code appended.\n",
    NULL },
  { "psl_db", (getter)Correlator_getprop_psl_db, NULL,
    "The preamble's peak sidelobe level, dB: its periodic autocorrelation's "
    "largest lag outside the mainlobe, relative to the peak (-inf for a "
    "perfect sequence such as Zadoff-Chu).\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_psl_db`` returns non-zero instead\n"
    "    of writing a value. The exception message is ``psl_db: only the\n"
    "    burst search (framing='burst') has it; Correlator.flavor says which\n"
    "    this is``, with the return code appended.\n",
    NULL },
  { "doppler_rate", (getter)Correlator_getprop_doppler_rate, NULL,
    "Doppler rate the coherent depth is bounded against, Hz/s (0 = no bound); "
    "with rate_hypotheses > 1, the span of the hypotheses.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_doppler_rate`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``doppler_rate: only the burst search (framing='burst') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { "rate_hypotheses", (getter)Correlator_getprop_rate_hypotheses, NULL,
    "Doppler-rate hypotheses searched: as given, or the engine's choice when "
    "built with 0; 1 is none.\n"
    "\n"
    "Raises\n"
    "------\n"
    "AttributeError\n"
    "    On read, if ``dp_correlator_get_rate_hypotheses`` returns non-zero\n"
    "    instead of writing a value. The exception message is\n"
    "    ``rate_hypotheses: only the burst search (framing='burst') has it;\n"
    "    Correlator.flavor says which this is``, with the return code\n"
    "    appended.\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
CorrelatorObj_destroy (CorrelatorObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      /* gh-2187: the views this object lent point into the
         state; the free waits for tp_dealloc, and every method
         refuses from here on. */
      self->_jm_parked = self->handle;
      self->handle     = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CorrelatorObj_enter (CorrelatorObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CorrelatorObj_exit (CorrelatorObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      /* gh-2187: the views this object lent point into the
         state; the free waits for tp_dealloc, and every method
         refuses from here on. */
      self->_jm_parked = self->handle;
      self->handle     = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CorrelatorObj_methods[] = {
  { "reset", (PyCFunction)CorrelatorObj_reset, METH_NOARGS,
    "Restart the correlator's running state, keeping how it was built.\n"
    "\n"
    "What each flavor drops; the reference, its spectrum, the FFT plans and\n"
    "every knob stay:\n"
    "\n"
    "- a map: zeroes the accumulator and the integration counter, a fresh\n"
    "  dwell cycle;\n"
    "- a detector: that, plus the carry (the partial frame held between\n"
    "  pushes) and the last dump's fields, so it matches a fresh detector\n"
    "  built with the same arguments; last_corr reads None until the next\n"
    "  dump;\n"
    "- a search: drains the input ring and clears the non-coherent power\n"
    "  accumulator and the dwell bookkeeping, so the next push() begins a\n"
    "  fresh search; the grid and thresholds are untouched.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Correlator(ref=ref, dwell=3)\n"
    ">>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))\n"
    ">>> c.count\n"
    "1\n"
    ">>> c.reset()\n"
    ">>> c.count\n"
    "0\n" },

  { "execute", (PyCFunction)(void *)CorrelatorObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray | None\n"
    "\n"
    "Correlate one 2-D frame and optionally dump the coherent\n"
    "accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with\n"
    "ref_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷\n"
    "ny*nx). Accumulating in the frequency domain and inverting once is\n"
    "exactly the per-frame inverse summed, by linearity of the IFFT — valid\n"
    "because the dwell is **coherent** (a complex sum); a non-coherent\n"
    "(magnitude) integration could not defer the inverse.\n"
    "\n"
    "**Layout.** A frame is ny*nx complex samples in row-major order: row i\n"
    "is samples i*nx .. (i+1)*nx - 1. For the 2-D map that is a (ny, nx)\n"
    "image; for the delay-Doppler map it is the raw stream, ny code periods\n"
    "of nx = code_len samples back to back. The map a dump produces is flat,\n"
    "n_out = ny_out * nx_out values (ny values with col_out), also\n"
    "row-major: reshape it to (ny_out, nx_out). On the delay-Doppler map,\n"
    "axis 0 is Doppler (centered, zero Doppler on row ny_out / 2) and axis 1\n"
    "is delay (the lag, in samples).\n"
    "\n"
    "**Ownership.** In Python, a call that dumps returns a new array when no\n"
    "`out=` is given, and a view of `out=` when one is (a 1-D array holding\n"
    "at least n_out values; it is written on a dump only, and the next dump\n"
    "overwrites it). A call that does not dump returns None and touches\n"
    "neither.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input frame, flat row-major CF32, length ny*nx.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer for the correlation map (CF32, length n_out:\n"
    "    ny_out*nx_out, or ny with col_out); written only on a dump call.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64] | None\n"
    "    Python: the map on a dump, None otherwise; a frame that is not\n"
    "    ny*nx samples is REFUSED with nothing read, written or counted, as\n"
    "    a ValueError (#1925). C: n_out on a dump (or max_out if smaller), 0\n"
    "    otherwise, and SIZE_MAX for the refused frame. None when the C call\n"
    "    writes no output.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute: only a map (framing='none', detect=False) executes;\n"
    "    Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Correlator(ref=ref, dwell=2)\n"
    ">>> x = np.ones((2, 2), dtype=np.complex64)\n"
    ">>> c.execute(x) is None   # frame 1 — no dump\n"
    "True\n"
    ">>> c.execute(x).tolist()  # frame 2 — dump\n"
    "[(2+0j), (2+0j), (2+0j), (2+0j)]\n" },
  { "execute_max_out", (PyCFunction)CorrelatorObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Maximum output samples per execute call\n"
    "(dp_correlator_state_t::n_out -- ny_out*nx_out normally, or ny when a\n"
    "single dp_correlator_state_t::col_out was selected).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n" },
  { "execute_all", (PyCFunction)(void *)CorrelatorObj_execute_all,
    METH_VARARGS | METH_KEYWORDS,
    "execute_all(x, out) -> ndarray\n"
    "\n"
    "Every map of a batch, unreduced: row h of the (H, n_out) result is\n"
    "the map of frame h (in C, map h is at `out + h * n_out`).\n"
    "\n"
    "Python: `x` is the H frames back to back, a flat array of H * ny * nx\n"
    "samples (H >= 1): frame h is the samples from h * ny * nx up to, not\n"
    "including, (h + 1) * ny * nx. With no `out=` each call returns a new\n"
    "array; with `out=` it returns a view of it, and the next call into the\n"
    "same `out=` overwrites that view. `out=` is a 2-D complex64 array of\n"
    "shape (rows, n_out) with at least H rows (a shorter one is refused);\n"
    "the result is its first H rows. The object's dwell is untouched.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    H frames back to back (CF32).\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    H * n_out values (CF32).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Python: the (H, n_out) array; a refused batch (empty, not a whole\n"
    "    number of frames, or an `out=` with fewer than H rows) is a\n"
    "    ValueError with nothing written. C: H * n_out, or SIZE_MAX for the\n"
    "    refused batch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute_all: only a map (framing='none', detect=False) runs a\n"
    "    batch; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> maps = dd.execute_all(frames)\n"
    ">>> maps.shape\n"
    "(2, 256)\n"
    ">>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay\n"
    "[3, 11]\n"
    ">>> dd.count                    # the dwell never moved\n"
    "0\n" },
  { "execute_all_max_out", (PyCFunction)CorrelatorObj_execute_all_max_out,
    METH_VARARGS,
    "execute_all_max_out(n_in) -> int\n"
    "\n"
    "Samples dp_correlator_execute_all() writes for n_in input samples: H\n"
    "* n_out, H = n_in / (ny * nx).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Input samples about to be passed. 0 on a detector or search flavor,\n"
    "    which execute_all() refuses.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The capacity `execute_all` needs for n_in samples: H * n_out values\n"
    "    (C: SIZE_MAX if that overflows).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32\n"
    "512\n" },
  { "execute_coherent", (PyCFunction)(void *)CorrelatorObj_execute_coherent,
    METH_VARARGS | METH_KEYWORDS,
    "execute_coherent(x, out) -> ndarray\n"
    "\n"
    "The coherent sum of a batch's maps, inverted once.\n"
    "\n"
    "Python: `x` is the H frames back to back, a flat array of H * ny * nx\n"
    "samples (H >= 1).\n"
    "\n"
    "The frames' product spectra are summed and the sum is inverted, as a\n"
    "dwell does: the result is bit-identical to what a fresh object with\n"
    "dwell = H dumps for the same frames, and equal to the sum of the H maps\n"
    "up to rounding. The object's own dwell is untouched.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    H frames back to back (CF32).\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    n_out values (CF32).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Python: the one map, a new (n_out,) array; a refused batch is a\n"
    "    ValueError with nothing written. C: n_out, or SIZE_MAX for the\n"
    "    refused batch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute_coherent: only a map (framing='none', detect=False) runs\n"
    "    a batch; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> x = np.tile(code, ny)\n"
    ">>> total = dd.execute_coherent(np.concatenate([x, -x]))\n"
    ">>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel\n"
    "True\n" },
  { "execute_coherent_max_out",
    (PyCFunction)CorrelatorObj_execute_coherent_max_out, METH_NOARGS,
    "execute_coherent_max_out() -> int\n"
    "\n"
    "Samples dp_correlator_execute_coherent() writes: n_out.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_coherent_max_out()\n"
    "256\n" },
  { "execute_power_sum", (PyCFunction)(void *)CorrelatorObj_execute_power_sum,
    METH_VARARGS | METH_KEYWORDS,
    "execute_power_sum(x, out) -> ndarray\n"
    "\n"
    "The power of a batch's maps, summed cell by cell: each output cell\n"
    "is the sum over h of that cell's `|map_h|^2`, summed in h order in\n"
    "float.\n"
    "\n"
    "Python: `x` is the H frames back to back, a flat array of H * ny * nx\n"
    "samples (H >= 1).\n"
    "\n"
    "Unlike the coherent sum, phase does not cancel: frames x and -x sum to\n"
    "twice one map's power, where their coherent sum is zero.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    H frames back to back (CF32).\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    n_out powers (float).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    Python: the (n_out,) float32 powers, a new array; a refused batch\n"
    "    is a ValueError with nothing written. C: n_out, or SIZE_MAX for the\n"
    "    refused batch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute_power_sum: only a map (framing='none', detect=False) runs\n"
    "    a batch; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> x = np.tile(code, ny)\n"
    ">>> power = dd.execute_power_sum(np.concatenate([x, -x]))\n"
    ">>> one = np.abs(dd.execute_all(x)[0]) ** 2\n"
    ">>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel\n"
    "True\n" },
  { "execute_power_sum_max_out",
    (PyCFunction)CorrelatorObj_execute_power_sum_max_out, METH_NOARGS,
    "execute_power_sum_max_out() -> int\n"
    "\n"
    "Powers dp_correlator_execute_power_sum() writes: n_out.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> dd.execute_power_sum_max_out()\n"
    "256\n" },
  { "execute_power_max", (PyCFunction)CorrelatorObj_execute_power_max,
    METH_VARARGS,
    "execute_power_max(x) -> tuple[ndarray, ndarray]\n"
    "\n"
    "The largest power over a batch's maps, cell by cell, and the map it\n"
    "came from.\n"
    "\n"
    "Python: `x` is the H frames back to back, a flat array of H * ny * nx\n"
    "samples (H >= 1).\n"
    "\n"
    "Each cell of out is the largest `|map_h|^2` that cell takes over the\n"
    "maps, and the same cell of out1 is the h it came from. A tie goes to\n"
    "the lowest h: a later map replaces the running maximum only when it is\n"
    "strictly greater.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    H frames back to back (CF32); H at most 2^32.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.uint32]]\n"
    "    Python: the tuple (power, index), each (n_out,): float32 powers and\n"
    "    uint32 map indices, always new arrays (this call takes no `out=`);\n"
    "    a refused batch, or H above 2**32, is a ValueError. C: n_out, or\n"
    "    SIZE_MAX with nothing written.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute_power_max: only a map (framing='none', detect=False) runs\n"
    "    a batch; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> power, index = dd.execute_power_max(frames)\n"
    ">>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks\n"
    "(0, 1)\n" },
  { "execute_peaks", (PyCFunction)CorrelatorObj_execute_peaks, METH_VARARGS,
    "execute_peaks(x) -> tuple[ndarray, ndarray, ndarray, ndarray]\n"
    "\n"
    "One record per map of a batch: its peak power, where the peak is,\n"
    "and its mean power.\n"
    "\n"
    "Python: `x` is the H frames back to back, a flat array of H * ny * nx\n"
    "samples (H >= 1).\n"
    "\n"
    "Entry h of each output describes map h: out its largest `|map|^2`, out1\n"
    "and out2 the row and column of that cell (the first in row-major order\n"
    "on a tie), and out3 the mean `|map|^2` over its n_out cells. A map is\n"
    "ny_out x nx_out; with col_out it is ny x 1, and the column reported is\n"
    "col_out. The peak-to-mean of each map is out over out3, entry by entry:\n"
    "a statistic of each map, which the same statistic of a pointwise\n"
    "maximum over maps does not give.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    H frames back to back (CF32).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.uint32], NDArray[np.uint32], "
    "NDArray[np.float32]]\n"
    "    Python: the tuple (peak, row, col, mean), each (H,): float32 peak\n"
    "    power, uint32 row and column, float32 mean power, always new arrays\n"
    "    (this call takes no `out=`); a refused batch is a ValueError. C: H,\n"
    "    or SIZE_MAX with nothing written.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``execute_peaks: only a map (framing='none', detect=False) runs a\n"
    "    batch; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx = 8, 32\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> frames = np.concatenate([np.tile(np.roll(code, d), ny)\n"
    "...                          for d in (3, 11)])   # delays 3 and 11\n"
    ">>> peak, row, col, mean = dd.execute_peaks(frames)\n"
    ">>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2\n"
    "([4, 4], [3, 11])\n"
    ">>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean\n"
    "True\n" },
  { "bank_power_max", (PyCFunction)(void *)CorrelatorObj_bank_power_max,
    METH_VARARGS | METH_KEYWORDS,
    "bank_power_max(x, L, g_min, g_max, rates) -> tuple[ndarray, ndarray]\n"
    "\n"
    "The strongest power at every cell over a bank, and which hypothesis\n"
    "it came from.\n"
    "\n"
    "dp_correlator_bank_fold() with the `POWER_MAX` reducer: at each cell,\n"
    "out holds the largest |map_h|^2 over every hypothesis h, and out1 that\n"
    "h, `(g - g_min) * R + r`, the lowest h on a tie. Every argument and\n"
    "refusal is dp_correlator_bank_fold()'s; in addition max_out below\n"
    "`n_out` is refused with nothing written.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    One frame, x_len samples.\n"
    "L : int\n"
    "    Fractions per whole bin; at least 1.\n"
    "g_min : int\n"
    "    Lowest carrier, in units of `1 / (L * nx)` cycles/sample.\n"
    "g_max : int\n"
    "    Highest carrier; at least g_min.\n"
    "rates : npt.NDArray[np.complex64]\n"
    "    Per-period weights, R vectors of ny, or empty (NULL with rates_len\n"
    "    0 in C) for one unit vector.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.uint32]]\n"
    "    Python: the tuple (power, index), each (n_out,): float32 powers and\n"
    "    uint32 hypothesis indices, always new arrays (this call takes no\n"
    "    `out=`); out of memory is a MemoryError, any other refusal a\n"
    "    ValueError. C: `n_out`, or SIZE_MAX with nothing written.\n"
    "\n"
    "Raises\n"
    "------\n"
    "MemoryError\n"
    "    If the C call refuses and reports ``DP_ERR_MEMORY`` in ``*status``.\n"
    "    The exception message is the reason the C call gives, or\n"
    "    ``bank_power_max: out of memory`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``bank_power_max: only a map (framing='none', detect=False) has a\n"
    "    bank; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples\n"
    ">>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)\n"
    ">>> m = np.arange(ny * nx)\n"
    ">>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off\n"
    "...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> no_rates = np.empty(0, np.complex64)\n"
    ">>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), "
    "-4..4\n"
    ">>> cell = int(np.argmax(pw))\n"
    ">>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)\n"
    "(5, 3)\n" },
  { "bank_detect", (PyCFunction)(void *)CorrelatorObj_bank_detect,
    METH_VARARGS | METH_KEYWORDS,
    "bank_detect(x, L_d, L_r, g_min, g_max, t_candidate, t_confirm, rates) -> "
    "tuple[ndarray, ndarray, ndarray, ndarray, ndarray]\n"
    "\n"
    "Detect on a coarse carrier grid, refine each candidate at its own\n"
    "cell on a fine one, and keep what confirms.\n"
    "\n"
    "The bank searches on L_d and resolves on L_r\n"
    "(docs/design/corr2d-hypotheses.md section 6):\n"
    "\n"
    "1. **Detect.** dp_correlator_bank_power_max() over carriers g_min ..\n"
    "   g_max on the `1 / (L_d nx)` grid, with every rate.\n"
    "2. **Candidates.** Every cell whose power exceeds t_candidate and is at\n"
    "   least each of its 8 neighbors, both axes circular (a delay lag and a\n"
    "   Doppler bin both wrap).\n"
    "3. **Refine.** For a candidate found by coarse carrier g_d and rate r,\n"
    "   the map value at its cell under every fine carrier g_r from `m g_d -\n"
    "   (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers between\n"
    "   its coarse neighbors, on the `1 / (L_r nx)` grid. On the fast path\n"
    "   and the delay-Doppler map with a native delay axis each is a\n"
    "   time-domain correlation at the cell's lag and a slow-time DFT at its\n"
    "   Doppler row, in double, with no FFT: `2 ny nx` multiply-adds a\n"
    "   carrier. Any other object makes that hypothesis's map and reads the\n"
    "   cell. The best g_r wins, the lowest on a tie.\n"
    "4. **Confirm.** A candidate whose refined power exceeds t_confirm is a\n"
    "   record.\n"
    "\n"
    "Records come out in cell order (row-major), one entry of each output\n"
    "per record: out the refined power, out1 g_r on the fine grid, out2 the\n"
    "rate r, out3 the row and out4 the column, the cell as execute() lays\n"
    "out the map. Measured at 64 x 512 against an oracle that corrects the\n"
    "exact offset, L_d = 2 with L_r = 4 holds L = 4's sensitivity within\n"
    "0.16 dB, for about half the bank (the design page). L_d = L_r is the\n"
    "one-stage bank. Thresholds are powers in the map's own units; the\n"
    "caller sets them for its noise, as for any detector.\n"
    "\n"
    "As dp_correlator_bank_fold(); g_min and g_max are on the coarse grid.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "L_d : int\n"
    "    The search grid; at least 1.\n"
    "L_r : int\n"
    "    The resolution grid; a multiple of L_d.\n"
    "g_min : int\n"
    "    Input.\n"
    "g_max : int\n"
    "    Input.\n"
    "t_candidate : float\n"
    "    Candidate threshold, on the coarse power.\n"
    "t_confirm : float\n"
    "    Confirm threshold, on the refined power.\n"
    "rates : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "tuple[NDArray[np.float32], NDArray[np.int32], NDArray[np.uint32], "
    "NDArray[np.uint32], NDArray[np.uint32]]\n"
    "    Python: the tuple (power, g, rate, row, col), one entry per\n"
    "    confirmed record (possibly none): float32 refined power, int32\n"
    "    refined carrier, uint32 rate, row and column, always new arrays\n"
    "    (this call takes no `out=`); out of memory is a MemoryError, and\n"
    "    anything else below a ValueError. C: the record count (0 included),\n"
    "    or SIZE_MAX with nothing written for anything\n"
    "    dp_correlator_bank_fold() refuses, `L_d == 0`, an L_r that L_d does\n"
    "    not divide, a refined carrier past int32, max_out below `n_out`, or\n"
    "    memory for the detector's work or the refine's scratch.\n"
    "\n"
    "Raises\n"
    "------\n"
    "MemoryError\n"
    "    If the C call refuses and reports ``DP_ERR_MEMORY`` in ``*status``.\n"
    "    The exception message is the reason the C call gives, or\n"
    "    ``bank_detect: out of memory`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is the reason the C call gives, or\n"
    "    ``bank_detect: only a map (framing='none', detect=False) has a\n"
    "    bank; Correlator.flavor says which this is`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx, d = 8, 32, 5\n"
    ">>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)\n"
    ">>> m = np.arange(ny * nx)\n"
    ">>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2\n"
    "...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)\n"
    ">>> dd = Correlator(code, doppler_rows=ny)\n"
    ">>> no_rates = np.empty(0, np.complex64)\n"
    ">>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)\n"
    ">>> t = 0.5 * coarse.max()\n"
    ">>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,\n"
    "...                                           no_rates)\n"
    ">>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid\n"
    "(1, 5, 5)\n"
    ">>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered\n"
    "1.233\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)CorrelatorObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the search grid directly, bypassing both auto-sizing searches —\n"
    "the advanced escape hatch (mirrors Dll's/Costas's\n"
    "configure_lock_raw()).\n"
    "\n"
    "Resizes every buffer/plan that depends on the grid (the slow-time FFT,\n"
    "the code correlator, the reference, and every per-frame scratch\n"
    "buffer), re-derives the threshold ladder for the pinned grid from the\n"
    "same physics\n"
    "dp_correlator_create_burst()/dp_correlator_create_continuous() used,\n"
    "and clears in-flight accumulation (ring contents, the non-coherent\n"
    "power accumulator, dwell bookkeeping) — call between push() calls,\n"
    "never a substitute for one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Coherent depth to pin, in `[1, reps]`.\n"
    "n_noncoh : int\n"
    "    Non-coherent look count to pin, in `[1,\n"
    "    DP_CORRELATOR_N_NONCOH_SAFETY_CEILING]`.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message\n"
    "    is the reason the C call gives, or ``configure_search_raw: only a\n"
    "    search (framing='continuous' or 'burst') has a grid to pin;\n"
    "    Correlator.flavor says which this is`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_INVALID``. The exception message is\n"
    "    the reason the C call gives, or ``configure_search_raw:\n"
    "    doppler_bins must lie in 1..reps and n_noncoh in\n"
    "    1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, and non-coherent looks\n"
    "    need rate_hypotheses 1`` when it gives none.\n"
    "MemoryError\n"
    "    If the C call returns ``DP_ERR_MEMORY``. The exception message is\n"
    "    the reason the C call gives, or ``configure_search_raw: rebuilding\n"
    "    the grid could not allocate; the engine keeps its prior grid`` when\n"
    "    it gives none.\n"
    "ValueError\n"
    "    If the C call returns any other non-zero status. The exception\n"
    "    message is the reason the C call gives, or ``configure_search_raw\n"
    "    failed``, with the return code appended (gh-869), when it gives\n"
    "    none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=4e6, "
    "chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid\n"
    ">>> a.doppler_bins, a.n_noncoh\n"
    "(1, 4)\n"
    ">>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)\n"
    ">>> h = a.push(burst)\n"
    ">>> int(h[\"row\"][0]), int(h[\"col\"][0])      # detects at the pinned "
    "grid\n"
    "(0, 18)\n" },
  { "set_max_peaks", (PyCFunction)(void *)CorrelatorObj_set_max_peaks,
    METH_VARARGS | METH_KEYWORDS,
    "set_max_peaks(n) -> None\n"
    "\n"
    "How many peaks a dwell may report: the peak list's capacity.\n"
    "\n"
    "One (the default) is the classic detector -- the maximum of the\n"
    "surface, gated. More is the list of docs/design/async-dsss-receiver.md\n"
    "§7.1: every peak above the same gate, strongest first, each with an\n"
    "exclusion zone of one Doppler bin by the reference's first\n"
    "autocorrelation null around it (one chip for a PN code; one emitter's\n"
    "main lobe, so its own shoulders are not the next peak), and the\n"
    "two-epoch rule for a peak at an already-listed code phase -- a data\n"
    "transition inside the epoch splits one emitter into twins at its own\n"
    "code phase on other tiles, so such a peak is held for one dwell and\n"
    "listed only if it was there, at the same tile, on the previous one.\n"
    "Each listed peak is one dp_correlator_hit_t from dp_correlator_push(),\n"
    "all of a dwell's sharing its `samples_consumed` and `noise_est`. A held\n"
    "twin takes a slot of the `n` for that dwell but is not reported. The\n"
    "threshold does not change: a second peak is another draw from the same\n"
    "cells against the same union bound. Clears the held candidates.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    1 … DP_CORRELATOR_MAX_PEAKS.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message\n"
    "    is the reason the C call gives, or ``set_max_peaks: only a search\n"
    "    (framing='continuous' or 'burst') lists peaks; Correlator.flavor\n"
    "    says which this is`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_INVALID``. The exception message is\n"
    "    the reason the C call gives, or ``set_max_peaks: n must lie in\n"
    "    1..DP_CORRELATOR_MAX_PEAKS`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns any other non-zero status. The exception\n"
    "    message is the reason the C call gives, or ``set_max_peaks\n"
    "    failed``, with the return code appended (gh-869), when it gives\n"
    "    none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> code = (np.arange(31) * 5 % 2).astype(np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, symbol_rate=1e3,\n"
    "...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)\n"
    ">>> a.max_peaks\n"
    "1\n"
    ">>> a.set_max_peaks(8)\n"
    ">>> a.max_peaks\n"
    "8\n" },
  { "set_carrier_freq_hz",
    (PyCFunction)(void *)CorrelatorObj_set_carrier_freq_hz,
    METH_VARARGS | METH_KEYWORDS,
    "set_carrier_freq_hz(carrier_freq_hz) -> None\n"
    "\n"
    "Couple the code clock to the carrier: the chip rate dilates by\n"
    "`doppler_hz / carrier_freq_hz`, and the engine accounts for it.\n"
    "\n"
    "A physically-coupled Doppler moves the code as well as the carrier --\n"
    "100 chips/s at 20 ppm of 5 Mcps -- and the engine's two long\n"
    "integrations both smear over it (doppler#1256, #1254):\n"
    "\n"
    "- **Inside a coherent block** of D epochs every tile's epoch\n"
    "  correlations are shifted along the code axis by the drift the tile's\n"
    "  own frequency implies, `f_tile / carrier` chips per chip, aligned to\n"
    "  the block's middle (a linear phase on each epoch's product). The one\n"
    "  delay-Doppler map applies it at the block's end, read on a slow-time\n"
    "  grid 4 times finer and rounded. The bound is 0.09 dB, the worst over\n"
    "  33 tones and three dilations (docs/design/acq-surface.md §18.1); the\n"
    "  engine measures 0.020-0.026 dB on its equivalence test's carrier\n"
    "  scenes. dp_correlator_block_prompt() applies it exactly. A change of\n"
    "  carrier mid-block applies to the whole block. Measured at the\n"
    "  specification's 20 ppm with D = 154 (3.1 chips of drift across the\n"
    "  block): without it the block's peak is 13 dB down and 3 chips wide\n"
    "  and the depth detects nothing at 34 dB-Hz; with it the block reads as\n"
    "  a still one.\n"
    "- **The hand-off** (dp_correlator_build_handoff()) advances the hit's\n"
    "  code phase by the drift over half the dwell -- the non-coherent sum's\n"
    "  peak is the phase at the dwell's middle, the seed is wanted at its\n"
    "  end: 0.9 chip at the 40 dB-Hz floor, past a refine loop's pull-in.\n"
    "\n"
    "A mutator's value (#2022): it rides in the state blob (v8), and a\n"
    "restore takes it through this setter's rebuild, so a resumed engine\n"
    "carries it on. Setting it rebuilds the map's fine grid and every tile's\n"
    "ramp tables when they move (the regrid that re-applies the thread\n"
    "request). Default 0.0 (uncoupled) is the engine exactly as it ran\n"
    "without it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "carrier_freq_hz : float\n"
    "    RF carrier, Hz; 0.0 = uncoupled. Refused (DP_ERR_UNSUPPORTED) on\n"
    "    every flavor but the continuous search\n"
    "    (docs/design/correlator-python.md section 18).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message\n"
    "    is ``set_carrier_freq_hz: only the continuous search\n"
    "    (framing='continuous') takes a carrier; Correlator.flavor says\n"
    "    which this is``.\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_INVALID``. The exception message is\n"
    "    ``set_carrier_freq_hz: must be 0 (no carrier) or finite and above\n"
    "    sample_rate / 2``.\n"
    "MemoryError\n"
    "    If the C call returns ``DP_ERR_MEMORY``. The exception message is\n"
    "    ``set_carrier_freq_hz: rebuilding the delay-Doppler map for the new\n"
    "    carrier could not allocate; the engine is unchanged``.\n"
    "ValueError\n"
    "    If the C call returns any other non-zero status. The exception\n"
    "    message is ``set_carrier_freq_hz failed``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import Gold\n"
    ">>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=1e7, "
    "chip_rate=5e6, symbol_rate=2700.0,\n"
    "...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)\n"
    ">>> a.carrier_freq_hz\n"
    "0.0\n"
    ">>> a.set_carrier_freq_hz(2.5e9)\n"
    ">>> a.carrier_freq_hz\n"
    "2500000000.0\n" },
  { "set_threads", (PyCFunction)(void *)CorrelatorObj_set_threads,
    METH_VARARGS | METH_KEYWORDS,
    "set_threads(n) -> None\n"
    "\n"
    "Set how many threads the searcher fans its tiles across\n"
    "(docs/design/acq-surface.md §13: a roll per thread on persistent\n"
    "workers).\n"
    "\n"
    "A continuous engine is created with a pool of the machine's online\n"
    "cores when it has more than one tile; a burst engine, and a single-tile\n"
    "one, run serially. This sets the count: 0 auto-selects the online core\n"
    "count, 1 runs everything on the calling thread, n runs on n workers\n"
    "(the caller included) -- but never more than `window_bins`: every fan\n"
    "hands out one tile a task, so a worker past that only wakes and waits.\n"
    "The request is kept, and every regrid re-applies it, so an engine\n"
    "narrowed to one tile and widened again runs on min(n, tiles), not on\n"
    "the 1 the narrow grid ran on. The workers are created here, once, and\n"
    "parked between pushes; nothing is created per push. The surface is\n"
    "bit-identical at every count -- the tiles are independent after the one\n"
    "forward transform and each writes its own rows -- so this changes the\n"
    "cost of a push and nothing about its result. Setup path, never hot; not\n"
    "while another thread is inside push().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Thread count; 0 = online cores, 1 = serial. Refused\n"
    "    (DP_ERR_UNSUPPORTED) on a map or detector flavor (no search\n"
    "    record).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message\n"
    "    is ``set_threads: only a search (framing='continuous' or 'burst')\n"
    "    sizes its pool after create; a map or detector takes nthreads at\n"
    "    create``.\n"
    "ValueError\n"
    "    If the C call returns any other non-zero status. The exception\n"
    "    message is ``set_threads failed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 doppler_uncertainty=4000.0)\n"
    ">>> a.threads >= 1               # at most one worker a tile\n"
    "True\n"
    ">>> a.set_threads(1)\n"
    ">>> a.threads\n"
    "1\n" },
  { "set_telemetry", (PyCFunction)(void *)CorrelatorObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> None\n"
    "\n"
    "Attach (or detach) a telemetry context and register the engine's\n"
    "probes on it (docs/design/async-dsss-receiver.md §2.4).\n"
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
    "surface coordinate, not a physical unit;\n"
    "dp_correlator_surface_doppler_hz() and\n"
    "dp_correlator_surface_chip_phase() convert), \"<prefix>.n_peaks\" "
    "(picks\n"
    "in the dwell, held twins included), \"<prefix>.n_held\" (picks held as\n"
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
    "    Emit every decim-th decided dwell; >= 1. Refused\n"
    "    (DP_ERR_UNSUPPORTED) on every flavor but the continuous search\n"
    "    (docs/design/correlator-python.md section 18).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message\n"
    "    is the reason the C call gives, or ``set_telemetry: only the\n"
    "    continuous search (framing='continuous') is instrumented;\n"
    "    Correlator.flavor says which this is`` when it gives none.\n"
    "ValueError\n"
    "    If the C call returns ``DP_ERR_INVALID``. The exception message is\n"
    "    the reason the C call gives, or ``set_telemetry: refused, nothing\n"
    "    attached: the prefix joined to each probe name must fit\n"
    "    DP_TLM_NAME_MAX, decim must be at least 1, and the context must\n"
    "    have room for all ten probes`` when it gives none.\n"
    "MemoryError\n"
    "    If the C call returns ``DP_ERR_MEMORY``. The exception message is\n"
    "    the reason the C call gives, or ``set_telemetry: the attach's\n"
    "    scratch context could not be allocated; nothing is attached`` when\n"
    "    it gives none.\n"
    "ValueError\n"
    "    If the C call returns any other non-zero status. The exception\n"
    "    message is the reason the C call gives, or ``set_telemetry\n"
    "    failed``, with the return code appended (gh-869), when it gives\n"
    "    none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> a.set_telemetry(tlm, \"acq\")\n"
    ">>> sorted(tlm.probe_names)[:3]\n"
    "['acq.col', 'acq.conc', 'acq.gate']\n"
    ">>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)\n"
    ">>> _ = a.push(x)\n"
    ">>> len(tlm.read()) % 10      # ten records per decided dwell\n"
    "0\n" },
  { "surface", (PyCFunction)(void *)CorrelatorObj_surface,
    METH_VARARGS | METH_KEYWORDS,
    "surface(out) -> int\n"
    "\n"
    "The last decided dwell's surface, in the gate's own units.\n"
    "\n"
    "Copies the surface the last dwell was decided on into out, row-major\n"
    "`surface_rows` (Doppler: tiles, or interpolated slow-time rows) by\n"
    "`lag_bins` (code phase), every cell divided by the same CFAR reference\n"
    "the gate used — so a cell reads as its own test statistic, to a float\n"
    "rounding (the SIMD build's fast-math may take a reciprocal in this loop\n"
    "and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on\n"
    "the non-coherent path) is a flat plane on a plot.\n"
    "\n"
    "**A surface is kept while `keep_surface` is set OR a sink is attached**\n"
    "(dp_correlator_set_surface_sink()); this is the one statement of that\n"
    "rule. Set either, push, then read. `surface_at` says which dwell it is;\n"
    "a time-decimated record is the caller reading every k-th dwell, or a\n"
    "sink with `decim`.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float32]\n"
    "    At least `surface_rows * lag_bins` floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Cells written (`surface_rows * lag_bins`), or 0 when no dwell has\n"
    "    been decided while a surface was kept, or out is too small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``surface: only the continuous\n"
    "    search (framing='continuous') has it; Correlator.flavor says which\n"
    "    this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> a.keep_surface = 1\n"
    ">>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)\n"
    ">>> _ = a.push(x)\n"
    ">>> s = np.empty(a.surface_rows * a.lag_bins, dtype=np.float32)\n"
    ">>> a.surface(s) == s.size\n"
    "True\n"
    ">>> a.code_bins, a.lag_bins  # 1022 is 2 x 7 x 73: read on 1024 lags\n"
    "(1022, 1024)\n"
    ">>> s.reshape(a.surface_rows, a.lag_bins).shape == (a.surface_rows, "
    "1024)\n"
    "True\n" },
  { "surface_doppler_hz",
    (PyCFunction)(void *)CorrelatorObj_surface_doppler_hz,
    METH_VARARGS | METH_KEYWORDS,
    "surface_doppler_hz(out) -> int\n"
    "\n"
    "The surface's Doppler axis: the frequency of each row, in Hz.\n"
    "\n"
    "One value per surface row, the fold and scale a hit's `doppler_hz_est`\n"
    "uses (dp_fftfreq_index() times `doppler_res_hz`, on the interpolated\n"
    "grid where the slow-time axis is interpolated), so a plot of\n"
    "dp_correlator_surface() carries the same axis a DetectionEvent reports\n"
    "on.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float64]\n"
    "    At least `surface_rows` doubles.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Values written (`surface_rows`), or 0 if out is too small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``surface_doppler_hz: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, cn0_dbhz=50.0,\n"
    "...                 doppler_uncertainty=4000.0)\n"
    ">>> f = np.empty(a.surface_rows, dtype=np.float64)\n"
    ">>> a.surface_doppler_hz(f) == a.surface_rows\n"
    "True\n"
    ">>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())\n"
    "True\n" },
  { "surface_chip_phase",
    (PyCFunction)(void *)CorrelatorObj_surface_chip_phase,
    METH_VARARGS | METH_KEYWORDS,
    "surface_chip_phase(out) -> int\n"
    "\n"
    "The surface's code-phase axis: the chip phase of each column.\n"
    "\n"
    "One value per surface column, in chips, the same mapping\n"
    "dp_correlator_build_handoff() applies to a hit's `col` — so a plotted\n"
    "peak sits at the chip phase the DetectionEvent would carry.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.float64]\n"
    "    At least `lag_bins` doubles.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Values written (`lag_bins`), or 0 if out is too small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``surface_chip_phase: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(\n"
    "...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=2e6, "
    "chip_rate=1e6, cn0_dbhz=50.0)\n"
    ">>> c = np.empty(a.lag_bins, dtype=np.float64)\n"
    ">>> a.surface_chip_phase(c) == a.lag_bins\n"
    "True\n"
    ">>> bool(c[0] == 0.0 and c[1] == 511 - 511 / 1024)\n"
    "True\n" },
  { "surface_complex", (PyCFunction)(void *)CorrelatorObj_surface_complex,
    METH_VARARGS | METH_KEYWORDS,
    "surface_complex(out) -> int\n"
    "\n"
    "The last decided dwell's surface, complex: amplitude and carrier\n"
    "phase per cell, before the magnitude the gate reads.\n"
    "\n"
    "Copies the coherent sum the last dwell was decided on into out,\n"
    "row-major `surface_rows` x `lag_bins` like dp_correlator_surface(), in\n"
    "the correlation's own units rather than the gate's. A cell's phase is\n"
    "the carrier at the block's middle, relative to its tile's centre; its\n"
    "neighbours along the code axis are complex early and late arms, so a\n"
    "tracker can form the coherent discriminator `Re(conj(P) (L - E)) /\n"
    "|P|^2`, which the magnitude surface cannot\n"
    "(docs/design/async-dsss-receiver-measurements.md §12.21). Coherent path\n"
    "only: a non-coherent dwell (`n_noncoh > 1`) is a power sum with no\n"
    "phase, and reads 0.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.complex64]\n"
    "    At least `surface_rows * lag_bins` complex floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Cells written (`surface_rows * lag_bins`), or 0 when no dwell has\n"
    "    been decided, the path is non-coherent, or out is too small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``surface_complex: only a search\n"
    "    (framing='continuous' or 'burst') has it; Correlator.flavor says\n"
    "    which this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> a = Correlator(ref, framing=\"continuous\", sample_rate=4e6, "
    "chip_rate=1e6, cn0_dbhz=70.0)\n"
    ">>> a.n_noncoh                 # one look: the dwell is the coherent "
    "dump\n"
    "1\n"
    ">>> _ = a.push(np.roll(s0, 17).astype(np.complex64))\n"
    ">>> sc = np.empty(a.surface_rows * a.lag_bins, dtype=np.complex64)\n"
    ">>> a.surface_complex(sc) == sc.size\n"
    "True\n"
    ">>> int(np.argmax(np.abs(sc)) % a.lag_bins)   # the peak's lag column\n"
    "18\n" },
  { "block_prompt", (PyCFunction)(void *)CorrelatorObj_block_prompt,
    METH_VARARGS | METH_KEYWORDS,
    "block_prompt(tile, col, out) -> int\n"
    "\n"
    "One cell's column of the last whole block: the per-epoch complex\n"
    "correlations at a code phase, the despread stream at epoch rate.\n"
    "\n"
    "The block-coherent engine gathers every tile's correlation row for\n"
    "`coherent_bins` epochs before its slow-time transform (file doc,\n"
    "docs/design/acq-surface.md §4). This copies the `coherent_bins` values\n"
    "at column col of tile tile into out, in epoch order: each epoch's\n"
    "complex prompt at that code phase, rolled to the tile's centre\n"
    "frequency and shifted to the block's middle by the tile's code-rate\n"
    "hypothesis, so the phase is continuous along the column for an emitter\n"
    "at the tile's centre and rotates at its offset from it. At an emitter's\n"
    "cell this is what a despreader produces, one value per epoch, phase\n"
    "included (docs/design/async-dsss-receiver-measurements.md §12.21).\n"
    "Valid once a block is whole, until the next epoch is pushed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tile : int\n"
    "    Tile index, `0 … window_bins-1` (native FFT order, the order the\n"
    "    surface's rows are cut in).\n"
    "col : int\n"
    "    Code-phase column, `0 … lag_bins-1`.\n"
    "out : npt.NDArray[np.complex64]\n"
    "    At least `coherent_bins` complex floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Values written (`coherent_bins`), or 0 at `coherent_bins == 1` (no\n"
    "    block is gathered), while a block is partial, for an index out of\n"
    "    range, or when out is too small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``block_prompt: only the\n"
    "    continuous search (framing='continuous') has it; Correlator.flavor\n"
    "    says which this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> b = Correlator(ref, framing=\"continuous\", sample_rate=4e6, "
    "chip_rate=1e6, cn0_dbhz=60.0,\n"
    "...                 doppler_uncertainty=40e3, code_only_epochs=7)\n"
    ">>> b.coherent_bins                    # (7 + 1) // 2\n"
    "4\n"
    ">>> blk = np.tile(np.roll(s0, 17), "
    "b.coherent_bins).astype(np.complex64)\n"
    ">>> _ = b.push(blk)\n"
    ">>> p = np.empty(b.coherent_bins, dtype=np.complex64)\n"
    ">>> b.block_prompt(0, 17, p) == b.coherent_bins\n"
    "True\n"
    ">>> bool(np.abs(p).min() > 0.99 * np.abs(p).max())  # every epoch's "
    "prompt\n"
    "True\n"
    ">>> b.block_prompt(0, 17, np.empty(1, dtype=np.complex64))  # too small\n"
    "0\n" },
  { "block_raw", (PyCFunction)(void *)CorrelatorObj_block_raw,
    METH_VARARGS | METH_KEYWORDS,
    "block_raw(out) -> int\n"
    "\n"
    "The last whole block's raw samples, as pushed.\n"
    "\n"
    "Copies the `coherent_bins * code_bins` samples the block-coherent\n"
    "engine gathered for its last whole block into out, epoch by epoch in\n"
    "stream order — the samples dp_correlator_push() consumed, untouched.\n"
    "Kept for the tile-edge re-ask (search_resolve_tile_alias()); exposed so\n"
    "a tracker can re-correlate them at any code phase, rate or symbol\n"
    "boundary the engine's own grid does not have — a symbol-rate despreader\n"
    "at the tracked timing runs on exactly this\n"
    "(docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a\n"
    "block is whole, until the next epoch is pushed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "out : npt.NDArray[np.complex64]\n"
    "    At least `coherent_bins * code_bins` complex floats.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Samples written (`coherent_bins * code_bins`), or 0 at\n"
    "    `coherent_bins == 1`, while a block is partial, or when out is too\n"
    "    small.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``block_raw: only the continuous\n"
    "    search (framing='continuous') has it; Correlator.flavor says which\n"
    "    this is``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1,\n"
    "...                      length=5).generate(31)).astype(np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(\n"
    "...     np.complex64)\n"
    ">>> ref = np.where(code & 1, -1, 1).astype(np.complex64)\n"
    ">>> b = Correlator(ref, framing=\"continuous\", sample_rate=4e6, "
    "chip_rate=1e6, cn0_dbhz=60.0,\n"
    "...                 doppler_uncertainty=40e3, code_only_epochs=7)\n"
    ">>> blk = np.tile(np.roll(s0, 17), "
    "b.coherent_bins).astype(np.complex64)\n"
    ">>> _ = b.push(blk)\n"
    ">>> raw = np.empty(b.coherent_bins * b.code_bins, dtype=np.complex64)\n"
    ">>> b.block_raw(raw) == raw.size\n"
    "True\n"
    ">>> bool(np.array_equal(raw, blk))      # the samples as pushed\n"
    "True\n" },
  { "push", (PyCFunction)(void *)CorrelatorObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x, out) -> ndarray\n"
    "\n"
    "Stream an arbitrary-length CF32 chunk through a detector or a search\n"
    "flavor; return its detections.\n"
    "\n"
    "Takes the input in order and frames it. A detector runs each complete\n"
    "n-sample frame (n = ny*nx) through the correlator and, on every dump,\n"
    "computes the test statistic peak_mag / noise_est: each dump that passes\n"
    "the threshold is one hit, row and col its cell (on one row, col is the\n"
    "lag). A search gates each decided dwell on its CFAR threshold and lists\n"
    "up to max_peaks hits a dwell, each with its Doppler bin (row), code\n"
    "phase (col), C/N0 estimate and hand-off fields (dp_correlator_hit_t). A\n"
    "chunk that ends mid-frame keeps the rest as the carry, so the hits do\n"
    "not depend on how the stream is split into calls.\n"
    "\n"
    "In Python the hits are one structured array of dp_correlator_hit_t\n"
    "records, a field per member, sized from dp_correlator_push_max_out(): a\n"
    "push never loses input to a full list. A map flavor refuses it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    CF32 input chunk of arbitrary length.\n"
    "out : npt.NDArray[Any] | None\n"
    "    Optional pre-allocated output buffer. When given, the result is\n"
    "    written into it and the returned array is a view of exactly the\n"
    "    samples produced; when omitted, a fresh array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Python: the hits, a structured array, empty when no dump or dwell\n"
    "    passed the gate. C: the number of hits written to result, or\n"
    "    SIZE_MAX (nothing taken, nothing changed) when state is a map\n"
    "    flavor.\n"
    "\n"
    "    One row per `dp_correlator_hit_t` record, with fields:\n"
    "\n"
    "    row : int\n"
    "        Row of the peak: 0 at one row; a search flavor's Doppler bin (0 "
    "… coherent_bins-1), or, in wideband mode, a bin of the uniform grid of "
    "window_bins*coherent_bins bins (window_bins tiles of coherent_bins each) "
    "— see the search flavors' section.\n"
    "    col : int\n"
    "        Column of the peak: the lag, at one row; a search flavor's code "
    "phase (0 … lag_bins-1), a lag of col * code_bins / lag_bins samples.\n"
    "    peak_mag : float\n"
    "        max |R| (linear magnitude, not power).\n"
    "    noise_est : float\n"
    "        Noise estimate aggregated over noise_lo..noise_hi.\n"
    "    test_stat : float\n"
    "        peak_mag / noise_est; 0 if noise_est is not > 0. A search "
    "flavor's is scaled by sqrt(2*n_noncoh) once non-coherent looks are "
    "combined.\n"
    "    cn0_dbhz_est : float\n"
    "        Estimated carrier-to-noise density (dB-Hz), backed out of "
    "test_stat via the same C/N0 <-> per-sample-amplitude-SNR relationship "
    "used to size the engine (see dp_correlator_create_burst()/ "
    "dp_correlator_create_continuous()). Tracks the true C/N0 while receiver "
    "AWGN dominates the CFAR noise estimate; saturates at the code's own "
    "autocorrelation-sidelobe floor once the true C/N0 exceeds what this "
    "code/geometry can resolve — a real ceiling, not a fault.\n"
    "    samples_consumed : int\n"
    "        st->samples_consumed at the exact moment this hit was appended — "
    "the raw sample offset (since this engine's own stream start) this "
    "detection's epoch ended at. The per-hit anchor a caller needs to derive "
    "a precise timestamp (dp_sample_clock_stamp_at(), timing/timing_core.h) "
    "instead of reusing one message-level timestamp for every hit — a single "
    "push() call spanning multiple epochs can emit several hits at different "
    "sample offsets. Acquisition itself stays wall-clock-agnostic; this is "
    "the raw material a caller with a real clock converts.\n"
    "    chip_phase : float\n"
    "        The code's own instantaneous phase at the hand-off instant, in "
    "chips, Dll's convention -- the seed a code loop wants for this hit. The "
    "same value dp_correlator_build_handoff() reports as "
    "dp_correlator_handoff_t::chip_phase: the inverted lag advanced by the "
    "dwell's dilation when a carrier is set "
    "(dp_correlator_set_carrier_freq_hz()). For an engine built by "
    "dp_correlator_create_burst(), one chip is one template sample, so "
    "chip_phase is in template samples over code_bins: a caller at s samples "
    "per chip divides by s. The burst search refuses a carrier setter, so its "
    "hits carry no dwell advance. Appended last so positional initialisers do "
    "not shift.\n"
    "    doppler_rate_hz_s : float\n"
    "        The Doppler rate, Hz/s, of the rate hypothesis that won this "
    "hit's dwell (dp_correlator_create_burst()'s rate_hypotheses); 0 without "
    "hypotheses, and 0 when the dwell's block is one or two epochs, which "
    "cannot see a rate (every hypothesis is then the same surface). A "
    "hypothesis, not a fit, and a reliable one only from a block WHOLLY "
    "inside the preamble and well above the design point. Then it names the "
    "hypothesis nearest the carrier's rate -- within half a spacing, "
    "`doppler_rate/(n - 1)` -- every time (validate_acq_rate_pd's "
    "reported-rate rows, N = 9, D = 10, design -13 dB, 23 dB over it: 119/119 "
    "at true rate 0, 121/121 at +R). Below that it is not: at the design "
    "point only 30% (rate 0) and 66% (+R) of the full-block hits name it, "
    "adjacent hypotheses differing by at most pi/4 of phase at the block's "
    "edges so that noise picks among neighbours; and about three hits in four "
    "come from dwells STRADDLING the preamble's edges, which often name +-R "
    "(39% of all hits at +10 dB-Hz name the nearest at true rate 0, 51% at "
    "+R). DETECTION is unaffected -- the surface that wins is a good one "
    "whichever neighbour it is. Treat the value as a seed, not a measurement, "
    "at the C/N0 the engine was sized for. With `max_peaks > 1` one "
    "hypothesis's surface serves the whole dwell's list, so every hit of a "
    "dwell wears the winner's rate. The Doppler the hit reports is the "
    "carrier's at the middle of the dwell's block. Appended last so "
    "positional initialisers do not shift.\n"
    "    test_stat_peak : float\n"
    "        test_stat as the emitter would read on its own lag: the straddle "
    "a delay between two lag columns costs, put back. Estimated from the "
    "larger of the hit's two neighbouring columns under the mainlobe the "
    "engine models (a triangle `shape.zone` samples wide, as for a chip "
    "code): a centre c and neighbour n one column, s samples, apart give (c + "
    "n) / (2 - s / zone), exact on that triangle, clamped between c and c / "
    "(1 - s / (2 zone)) -- never below what was read, never past the worst "
    "straddle. Noise lifts the neighbour, so at low statistics it reads high, "
    "within that clamp. For a bound on what the emitter's sidelobes can reach "
    "(a sidelobe does not lose its straddle with the peak); detection reads "
    "test_stat. Appended last, like the field above.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``push: only a detector or a\n"
    "    search pushes; a map does not detect (build one with detect=True,\n"
    "    or framing='continuous' or 'burst')``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> det = Correlator(ref=ref, detect=1, dwell=1, noise_lo=1, "
    "noise_hi=15,\n"
    "...                  noise_mode=\"mean\", threshold=0.0)\n"
    ">>> h = det.push(np.ones((4, 4), dtype=np.complex64))[0]\n"
    ">>> row, col = int(h[\"row\"]), int(h[\"col\"])\n"
    ">>> peak, noise, stat = (float(h[f]) for f in (\"peak_mag\", "
    "\"noise_est\",\n"
    "...                                            \"test_stat\"))\n"
    ">>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)\n"
    "(0, 0, 1.0, 1.0, 1.0)\n"
    ">>> det1 = Correlator(ref=ref[0], detect=1, dwell=1, noise_lo=1, "
    "noise_hi=3,\n"
    "...                 noise_mode=\"mean\", threshold=0.0)\n"
    ">>> h = det1.push(np.ones(4, dtype=np.complex64))[0]\n"
    ">>> lag = int(h[\"col\"])               # one row: the lag is the "
    "column\n"
    ">>> peak, noise, stat = (float(h[f]) for f in (\"peak_mag\", "
    "\"noise_est\",\n"
    "...                                            \"test_stat\"))\n"
    ">>> lag, round(peak, 4), round(noise, 4), round(stat, 4)\n"
    "(0, 1.0, 1.0, 1.0)\n"
    "\n"
    "A burst search over a PN preamble, 4 samples a chip, finds the burst's\n"
    "delay; any repeated preamble works, a Zadoff-Chu sequence included:\n"
    "\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1, "
    "length=5).generate(31),\n"
    "...                   np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), "
    "4).astype(np.complex64)\n"
    ">>> b = Correlator(s0, framing=\"burst\", reps=8, sample_rate=4e6,\n"
    "...                cn0_dbhz=60.0)\n"
    ">>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]\n"
    ">>> int(h[\"row\"]), int(h[\"col\"])     # (Doppler bin, delay in "
    "samples)\n"
    "(0, 17)\n"
    ">>> k = np.arange(127)\n"
    ">>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / "
    "127).astype(np.complex64)\n"
    ">>> z = Correlator(zc, framing=\"burst\", reps=8, sample_rate=1.0)\n"
    ">>> z.code_bins                      # samples a repetition\n"
    "127\n"
    ">>> h = z.push(np.tile(np.roll(zc, 40), 10))[0]\n"
    ">>> int(h[\"row\"]), int(h[\"col\"])\n"
    "(0, 40)\n" },
  { "push_max_out", (PyCFunction)CorrelatorObj_push_max_out, METH_VARARGS,
    "push_max_out(n_in) -> int\n"
    "\n"
    "The most hits dp_correlator_push() can return for n_in more samples:\n"
    "a result array this long is never short.\n"
    "\n"
    "The frames n_in completes, counting the carry the last push left, times\n"
    "what one frame can make: one hit for a detector (one per dump),\n"
    "`max_peaks` for a search (a frame that ends a dwell lists up to that\n"
    "many). A bound, not a count: most frames end no dump. 0 on a map\n"
    "flavor, which push() refuses. SIZE_MAX if the product would overflow.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n_in : int\n"
    "    Samples the next push() will be given.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    The bound, in hits.\n" },
  { "state_bytes", (PyCFunction)CorrelatorObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Correlator has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CorrelatorObj_get_state, METH_NOARGS,
    "Serialize this object's mutable state to bytes.\n"
    "\n"
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
    "\n"
    "The blob is opaque and always `state_bytes()` long. Its layout is an\n"
    "implementation detail of the C core and is not a stable format across\n"
    "builds.\n"
    "\n"
    "Raises ``RuntimeError`` if the Correlator has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CorrelatorObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()`, the exception listed under Raises\n"
    "if the core rejects it, and ``RuntimeError`` if the Correlator has\n"
    "already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` long.\n"
    "\n"
    "Raises\n"
    "------\n"
    "MemoryError\n"
    "    If the C call returns ``DP_ERR_MEMORY``. The exception message is\n"
    "    ``set_state: the blob's carrier needs the delay-Doppler map\n"
    "    rebuilt, and the rebuild could not allocate; the engine is\n"
    "    unchanged``.\n"
    "ValueError\n"
    "    If the C core rejects the blob with any other non-zero status. The\n"
    "    exception message is ``set_state rejected the blob``.\n" },
  { "destroy", (PyCFunction)CorrelatorObj_destroy, METH_NOARGS,
    "Close the object; the memory goes with its last user.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this, or use the object as a context manager,\n"
    "to end its use at a definite point.\n"
    "\n"
    "Views this object handed out keep the memory they point into, as does a\n"
    "call still running on another thread: it is freed when the last of\n"
    "them, and this object, are gone, not here (gh-2187). Idempotent:\n"
    "calling it again does nothing. Every other method raises\n"
    "``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)CorrelatorObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Correlator be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Correlator\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CorrelatorObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Correlator.\n"
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
  { NULL, NULL, 0, NULL }
};

static PyTypeObject CorrelatorObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.correlation.Correlator",
  .tp_basicsize                           = sizeof (CorrelatorObject),
  .tp_dealloc                             = (destructor)CorrelatorObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Build the flavor the knobs name: the one constructor the Python class\n"
    "`Correlator` calls.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ref : NDArray[np.complex64]\n"
    "    Row-major ny x nx reference, CF32; borrowed.\n"
    "    A 1-D array is read as one row, shape (1, n).\n"
    "framing : Literal[\"none\", \"continuous\", \"burst\"], default "
    "\"none\"\n"
    "    'none' for maps or detections from whole frames; 'continuous' or\n"
    "    'burst' for a search over a stream.\n"
    "    One of ``\"none\"``, ``\"continuous\"``, ``\"burst\"``.\n"
    "doppler_rows : int, default 0\n"
    "    Doppler rows of a delay-Doppler map: the code periods one frame "
    "holds.\n"
    "    0 for no map.\n"
    "detect : int, default 0\n"
    "    1 for detections from push() (framing='none'); a search always "
    "detects.\n"
    "dwell : int, default 1\n"
    "    Frames integrated a dump; default 1.\n"
    "ny_out : int, default 0\n"
    "    Padded Doppler rows; 0 (default) for native.\n"
    "nx_out : int, default 0\n"
    "    Padded lags; 0 (default) for native.\n"
    "col_out : int, default -1\n"
    "    The one lag to emit; -1 (default) for every lag.\n"
    "threshold : float, default 0.0\n"
    "    Detection threshold on peak / noise; default 0.\n"
    "noise_lo : int, default 0\n"
    "    First noise bin; default 0.\n"
    "noise_hi : int\n"
    "    Last noise bin; default SIZE_MAX, the last bin.\n"
    "noise_mode : Literal[\"mean\", \"median\", \"min\", \"max\"], default "
    "\"mean\"\n"
    "    How the noise bins are aggregated into the floor the test statistic\n"
    "    divides by (a detector), or the CFAR reference's statistic (a "
    "search).\n"
    "    One of ``\"mean\"``, ``\"median\"``, ``\"min\"``, ``\"max\"``.\n"
    "sample_rate : float, default 0.0\n"
    "    Samples a second; a search needs it (0 is not given). 1.0 for\n"
    "    normalized units.\n"
    "chip_rate : float, default 0.0\n"
    "    Chips a second; the continuous search needs it (0 is not given).\n"
    "symbol_rate : float, default 0.0\n"
    "    Data symbols a second, for the continuous search's diagnostic; 0 is "
    "no\n"
    "    known data clock.\n"
    "reps : int, default 1\n"
    "    Preamble repetitions a burst holds (the burst search).\n"
    "cn0_dbhz : float\n"
    "    Design-point C/N0, dB-Hz; default DP_CORRELATOR_CN0_NONE (NaN, "
    "none).\n"
    "    The continuous search needs a finite one.\n"
    "doppler_uncertainty : float, default 0.0\n"
    "    Half-width of the Doppler search, Hz; default 0.\n"
    "pfa : float, default 1e-3\n"
    "    False-alarm probability a search; default 1e-3.\n"
    "pd : float, default 0.9\n"
    "    Detection probability at the design point; default 0.9.\n"
    "doppler_rate : float, default 0.0\n"
    "    Doppler rate bound, Hz/s; default 0.\n"
    "rate_hypotheses : int, default 1\n"
    "    Rate hypotheses of the burst search; default 1.\n"
    "code_only_epochs : int, default 1\n"
    "    Code-only epochs of the continuous search; default 1.\n"
    "nthreads : int, default 1\n"
    "    Worker threads of a map or detector; default 1. A search sizes its "
    "own\n"
    "    pool and refuses any other value here: its count is set after "
    "create\n"
    "    with dp_correlator_set_threads().\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is the reason the\n"
    "    constructor gives, or ``Correlator: refused, or out of memory\n"
    "    (docs/design/correlator-python.md section 17: a value the flavor "
    "would\n"
    "    not honor is refused)`` when it gives none.\n"
    "\n"
    "Warns\n"
    "-----\n"
    "UserWarning\n"
    "    Emitted after construction when ``self->handle->search &&\n"
    "    self->handle->search->underpowered`` holds: ``Correlator: the "
    "search\n"
    "    cannot meet pd at this cn0_dbhz and geometry (pd_predicted < pd, or\n"
    "    pd_burst < pd for a burst). It still builds a best-effort grid. "
    "Raise\n"
    "    cn0_dbhz, narrow doppler_uncertainty, lower pd, or give a burst "
    "more\n"
    "    repetitions.``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.correlation import Correlator\n"
    ">>> ref = np.zeros(8, np.complex64); ref[0] = 1.0\n"
    ">>> Correlator(ref).flavor                      # a 1-D map\n"
    "'1d'\n"
    ">>> Correlator(ref, detect=1).flavor            # a streaming detector\n"
    "'detector_1d'\n"
    ">>> Correlator(np.ones((4, 8), np.complex64)).flavor\n"
    "'2d'\n"
    ">>> Correlator(ref, doppler_rows=4).flavor      # a delay-Doppler map\n"
    "'delay_doppler'\n"
    ">>> Correlator(ref).execute(ref).shape          # one dump of nx lags\n"
    "(8,)\n"
    ">>> from doppler.wfm import PN, mls_poly\n"
    ">>> code = np.asarray(PN(poly=mls_poly(5), seed=1, "
    "length=5).generate(31),\n"
    "...                   np.uint8)\n"
    ">>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), "
    "4).astype(np.complex64)\n"
    ">>> b = Correlator(s0, framing=\"burst\", reps=8, sample_rate=4e6,\n"
    "...                cn0_dbhz=60.0)       # a burst search: 8 repetitions\n"
    ">>> b.flavor\n"
    "'burst'\n"
    ">>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]\n"
    ">>> int(h[\"row\"]), int(h[\"col\"])     # (Doppler bin, delay in "
    "samples)\n"
    "(0, 17)\n",
  .tp_methods = CorrelatorObj_methods,
  .tp_getset  = Correlator_getset,
  .tp_new     = CorrelatorObj_new,
  .tp_init    = (initproc)CorrelatorObj_init,
};
