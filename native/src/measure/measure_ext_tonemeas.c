/*
 * measure_ext_tonemeas.c — ToneMeasure type for the measure module.
 *
 * Included by measure_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only measure_ext.c is compiled.
 */
/* ======================================================== */
/* ToneMeasureObject — wraps tonemeas_state_t *       */
/* ======================================================== */

#include "tonemeas/tonemeas_core.h"

typedef struct
{
  PyObject_HEAD tonemeas_state_t *handle;
  float *_spectrum_dbfs_buf;     /* pre-allocated output for spectrum_dbfs */
  size_t _spectrum_dbfs_buf_cap; /* allocated capacity for spectrum_dbfs */
  void **_spectrum_dbfs_retired; /* gh-219 deferred free */
  size_t _spectrum_dbfs_retired_n;
  size_t _spectrum_dbfs_retired_cap;
} ToneMeasureObject;

static void
ToneMeasureObj_dealloc (ToneMeasureObject *self)
{
  if (self->handle)
    tonemeas_destroy (self->handle);
  free (self->_spectrum_dbfs_buf);
  for (size_t _i = 0; _i < self->_spectrum_dbfs_retired_n; _i++)
    free (self->_spectrum_dbfs_retired[_i]);
  free (self->_spectrum_dbfs_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ToneMeasureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ToneMeasureObject *self = (ToneMeasureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ToneMeasureObj_init (ToneMeasureObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[] = { "n",          "fs",   "n_harmonics",
                                  "full_scale", "bits", "dynamic_range_db",
                                  "dc_guard",   NULL };
  unsigned long long n_raw    = 8192;
  double             fs       = 1.0;
  unsigned long long n_harmonics_raw  = 8;
  double             full_scale       = 1.0;
  unsigned long long bits_raw         = 0;
  double             dynamic_range_db = 0.0;
  unsigned long long dc_guard_raw     = 0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "|KdKdKdK", kwlist, &n_raw, &fs, &n_harmonics_raw,
          &full_scale, &bits_raw, &dynamic_range_db, &dc_guard_raw))
    return -1;
  size_t n           = (size_t)n_raw;
  size_t n_harmonics = (size_t)n_harmonics_raw;
  size_t bits        = (size_t)bits_raw;
  size_t dc_guard    = (size_t)dc_guard_raw;
  self->handle       = tonemeas_create (n, fs, n_harmonics, full_scale, bits,
                                        dynamic_range_db, dc_guard);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "tonemeas_create returned NULL");
      return -1;
    }
  {
    size_t _max = tonemeas_spectrum_dbfs_max_out (self->handle);
    if (_max)
      {
        self->_spectrum_dbfs_buf = malloc (_max * sizeof (float));
        if (!self->_spectrum_dbfs_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_spectrum_dbfs_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
ToneMeasureObj_reset (ToneMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  tonemeas_reset (self->handle);
  Py_RETURN_NONE;
}

static PyStructSequence_Field ToneMeasureObj_analyze_fields[] = {
  { "snr", NULL },
  { "sinad", NULL },
  { "thd", NULL },
  { "thd_pct", NULL },
  { "thd_n", NULL },
  { "sfdr_dbc", NULL },
  { "sfdr_dbfs", NULL },
  { "enob", NULL },
  { "enob_fs", NULL },
  { "noise_floor_dbfs", NULL },
  { "fund_freq", NULL },
  { "fund_dbfs", NULL },
  { "worst_spur_freq", NULL },
  { "worst_spur_dbc", NULL },
  { "worst_spur_is_harm", NULL },
  { "rbw_hz", NULL },
  { "enbw_hz", NULL },
  { "bin_hz", NULL },
  { "lobe_bins", NULL },
  { "n_noise_bins", NULL },
  { "proc_gain_db", NULL },
  { "amp_uncert_db", NULL },
  { "floor_uncert_db", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc ToneMeasureObj_analyze_desc
    = { "doppler.measure.ToneMetrics", NULL, ToneMeasureObj_analyze_fields,
        23 };
static PyTypeObject *ToneMeasureObj_analyze_type = NULL;

static PyObject *
ToneMeasureObj_analyze (ToneMeasureObject *self, PyObject *args)
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
  if (!ToneMeasureObj_analyze_type)
    {
      ToneMeasureObj_analyze_type
          = PyStructSequence_NewType (&ToneMeasureObj_analyze_desc);
      if (!ToneMeasureObj_analyze_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  const float *_ng0 = (const float *)PyArray_DATA (in_arr);
  tone_meas_t  _r;
  Py_BEGIN_ALLOW_THREADS
    _r = tonemeas_analyze (self->handle, _ng0, n_in);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (ToneMeasureObj_analyze_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.snr));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.sinad));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.thd));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.thd_pct));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.thd_n));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.sfdr_dbc));
  PyStructSequence_SET_ITEM (_o, 6, PyFloat_FromDouble (_r.sfdr_dbfs));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.enob));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.enob_fs));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.noise_floor_dbfs));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.fund_freq));
  PyStructSequence_SET_ITEM (_o, 11, PyFloat_FromDouble (_r.fund_dbfs));
  PyStructSequence_SET_ITEM (_o, 12, PyFloat_FromDouble (_r.worst_spur_freq));
  PyStructSequence_SET_ITEM (_o, 13, PyFloat_FromDouble (_r.worst_spur_dbc));
  PyStructSequence_SET_ITEM (_o, 14,
                             PyLong_FromLong ((long)_r.worst_spur_is_harm));
  PyStructSequence_SET_ITEM (_o, 15, PyFloat_FromDouble (_r.rbw_hz));
  PyStructSequence_SET_ITEM (_o, 16, PyFloat_FromDouble (_r.enbw_hz));
  PyStructSequence_SET_ITEM (_o, 17, PyFloat_FromDouble (_r.bin_hz));
  PyStructSequence_SET_ITEM (
      _o, 18, PyLong_FromUnsignedLongLong ((unsigned long long)_r.lobe_bins));
  PyStructSequence_SET_ITEM (
      _o, 19,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.n_noise_bins));
  PyStructSequence_SET_ITEM (_o, 20, PyFloat_FromDouble (_r.proc_gain_db));
  PyStructSequence_SET_ITEM (_o, 21, PyFloat_FromDouble (_r.amp_uncert_db));
  PyStructSequence_SET_ITEM (_o, 22, PyFloat_FromDouble (_r.floor_uncert_db));
  return _o;
}

static PyStructSequence_Field ToneMeasureObj_analyze_complex_fields[] = {
  { "snr", NULL },
  { "sinad", NULL },
  { "thd", NULL },
  { "thd_pct", NULL },
  { "thd_n", NULL },
  { "sfdr_dbc", NULL },
  { "sfdr_dbfs", NULL },
  { "enob", NULL },
  { "enob_fs", NULL },
  { "noise_floor_dbfs", NULL },
  { "fund_freq", NULL },
  { "fund_dbfs", NULL },
  { "worst_spur_freq", NULL },
  { "worst_spur_dbc", NULL },
  { "worst_spur_is_harm", NULL },
  { "rbw_hz", NULL },
  { "enbw_hz", NULL },
  { "bin_hz", NULL },
  { "lobe_bins", NULL },
  { "n_noise_bins", NULL },
  { "proc_gain_db", NULL },
  { "amp_uncert_db", NULL },
  { "floor_uncert_db", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc ToneMeasureObj_analyze_complex_desc
    = { "doppler.measure.ToneMetrics", NULL,
        ToneMeasureObj_analyze_complex_fields, 23 };
static PyTypeObject *ToneMeasureObj_analyze_complex_type = NULL;

static PyObject *
ToneMeasureObj_analyze_complex (ToneMeasureObject *self, PyObject *args)
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
  if (!ToneMeasureObj_analyze_complex_type)
    {
      ToneMeasureObj_analyze_complex_type
          = PyStructSequence_NewType (&ToneMeasureObj_analyze_complex_desc);
      if (!ToneMeasureObj_analyze_complex_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream). */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (in_arr);
  tone_meas_t          _r;
  Py_BEGIN_ALLOW_THREADS
    _r = tonemeas_analyze_complex (self->handle, _ng0, n_in);
  Py_END_ALLOW_THREADS
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (ToneMeasureObj_analyze_complex_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.snr));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.sinad));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.thd));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.thd_pct));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.thd_n));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.sfdr_dbc));
  PyStructSequence_SET_ITEM (_o, 6, PyFloat_FromDouble (_r.sfdr_dbfs));
  PyStructSequence_SET_ITEM (_o, 7, PyFloat_FromDouble (_r.enob));
  PyStructSequence_SET_ITEM (_o, 8, PyFloat_FromDouble (_r.enob_fs));
  PyStructSequence_SET_ITEM (_o, 9, PyFloat_FromDouble (_r.noise_floor_dbfs));
  PyStructSequence_SET_ITEM (_o, 10, PyFloat_FromDouble (_r.fund_freq));
  PyStructSequence_SET_ITEM (_o, 11, PyFloat_FromDouble (_r.fund_dbfs));
  PyStructSequence_SET_ITEM (_o, 12, PyFloat_FromDouble (_r.worst_spur_freq));
  PyStructSequence_SET_ITEM (_o, 13, PyFloat_FromDouble (_r.worst_spur_dbc));
  PyStructSequence_SET_ITEM (_o, 14,
                             PyLong_FromLong ((long)_r.worst_spur_is_harm));
  PyStructSequence_SET_ITEM (_o, 15, PyFloat_FromDouble (_r.rbw_hz));
  PyStructSequence_SET_ITEM (_o, 16, PyFloat_FromDouble (_r.enbw_hz));
  PyStructSequence_SET_ITEM (_o, 17, PyFloat_FromDouble (_r.bin_hz));
  PyStructSequence_SET_ITEM (
      _o, 18, PyLong_FromUnsignedLongLong ((unsigned long long)_r.lobe_bins));
  PyStructSequence_SET_ITEM (
      _o, 19,
      PyLong_FromUnsignedLongLong ((unsigned long long)_r.n_noise_bins));
  PyStructSequence_SET_ITEM (_o, 20, PyFloat_FromDouble (_r.proc_gain_db));
  PyStructSequence_SET_ITEM (_o, 21, PyFloat_FromDouble (_r.amp_uncert_db));
  PyStructSequence_SET_ITEM (_o, 22, PyFloat_FromDouble (_r.floor_uncert_db));
  return _o;
}

static PyStructSequence_Field ToneMeasureObj_time_stats_fields[] = {
  { "rms", NULL },     { "peak", NULL },      { "crest_db", NULL },
  { "papr_db", NULL }, { "dc_offset", NULL }, { "fs_util_pct", NULL },
  { NULL, NULL },
};
static PyStructSequence_Desc ToneMeasureObj_time_stats_desc
    = { "doppler.measure.TimeStats", NULL, ToneMeasureObj_time_stats_fields,
        6 };
static PyTypeObject *ToneMeasureObj_time_stats_type = NULL;

static PyObject *
ToneMeasureObj_time_stats (ToneMeasureObject *self, PyObject *args)
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
  if (!ToneMeasureObj_time_stats_type)
    {
      ToneMeasureObj_time_stats_type
          = PyStructSequence_NewType (&ToneMeasureObj_time_stats_desc);
      if (!ToneMeasureObj_time_stats_type)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
    }
  time_stats_t _r = tonemeas_time_stats (
      self->handle, (const float *)PyArray_DATA (in_arr), n_in);
  Py_DECREF (in_arr);
  PyObject *_o = PyStructSequence_New (ToneMeasureObj_time_stats_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (_o, 0, PyFloat_FromDouble (_r.rms));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.peak));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.crest_db));
  PyStructSequence_SET_ITEM (_o, 3, PyFloat_FromDouble (_r.papr_db));
  PyStructSequence_SET_ITEM (_o, 4, PyFloat_FromDouble (_r.dc_offset));
  PyStructSequence_SET_ITEM (_o, 5, PyFloat_FromDouble (_r.fs_util_pct));
  return _o;
}

static PyObject *
ToneMeasureObj_spectrum_dbfs_max_out (ToneMeasureObject *self,
                                      PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (tonemeas_spectrum_dbfs_max_out (self->handle));
}

static PyObject *
ToneMeasureObj_spectrum_dbfs (ToneMeasureObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = tonemeas_spectrum_dbfs_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = tonemeas_spectrum_dbfs (
          self->handle, (const float *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr), (float *)PyArray_DATA (out_arr));
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_spectrum_dbfs_buf || self->_spectrum_dbfs_buf_cap < _need)
    {
      size_t _max = tonemeas_spectrum_dbfs_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_spectrum_dbfs_buf
          && self->_spectrum_dbfs_retired_n
                 == self->_spectrum_dbfs_retired_cap)
        {
          size_t _rcap = self->_spectrum_dbfs_retired_cap
                             ? self->_spectrum_dbfs_retired_cap * 2
                             : 4;
          void **_rt   = realloc (self->_spectrum_dbfs_retired,
                                  _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_spectrum_dbfs_retired     = _rt;
          self->_spectrum_dbfs_retired_cap = _rcap;
        }
      float *_tmp = malloc (_max * sizeof (float));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_spectrum_dbfs_buf)
        self->_spectrum_dbfs_retired[self->_spectrum_dbfs_retired_n++]
            = self->_spectrum_dbfs_buf;
      self->_spectrum_dbfs_buf     = _tmp;
      self->_spectrum_dbfs_buf_cap = _max;
    }
  size_t n_out = tonemeas_spectrum_dbfs (
      self->handle, (const float *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_spectrum_dbfs_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_FLOAT,
                                             self->_spectrum_dbfs_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}
static PyObject *
ToneMeasure_getprop_n (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
ToneMeasure_getprop_nfft (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
ToneMeasure_getprop_fs (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
ToneMeasure_getprop_enbw (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->enbw);
}
static PyObject *
ToneMeasure_getprop_lobe_bins (ToneMeasureObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->lobe_bins);
}
static PyObject *
ToneMeasure_getprop_spur_guard_bins (ToneMeasureObject *self,
                                     void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->spur_guard_bins);
}
static PyObject *
ToneMeasure_getprop_beta (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->beta);
}
static PyObject *
ToneMeasure_getprop_rbw (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->enbw * self->handle->fs
                             / (double)self->handle->n);
}
static PyObject *
ToneMeasure_getprop_bin_hz (ToneMeasureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs / (double)self->handle->nfft);
}
static PyObject *
ToneMeasure_getprop_proc_gain_db (ToneMeasureObject *self,
                                  void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (10.0 * log10 ((double)self->handle->nfft / 2.0));
}

static PyGetSetDef ToneMeasure_getset[]
    = { { "n", (getter)ToneMeasure_getprop_n, NULL, "N.\n", NULL },
        { "nfft", (getter)ToneMeasure_getprop_nfft, NULL, "Nfft.\n", NULL },
        { "fs", (getter)ToneMeasure_getprop_fs, NULL, "Fs.\n", NULL },
        { "enbw", (getter)ToneMeasure_getprop_enbw, NULL, "Enbw.\n", NULL },
        { "lobe_bins", (getter)ToneMeasure_getprop_lobe_bins, NULL,
          "Lobe bins.\n", NULL },
        { "spur_guard_bins", (getter)ToneMeasure_getprop_spur_guard_bins, NULL,
          "Spur guard bins.\n", NULL },
        { "beta", (getter)ToneMeasure_getprop_beta, NULL, "Beta.\n", NULL },
        { "rbw", (getter)ToneMeasure_getprop_rbw, NULL, "Rbw.\n", NULL },
        { "bin_hz", (getter)ToneMeasure_getprop_bin_hz, NULL, "Bin hz.\n",
          NULL },
        { "proc_gain_db", (getter)ToneMeasure_getprop_proc_gain_db, NULL,
          "Proc gain db.\n", NULL },
        { NULL } };

static PyObject *
ToneMeasureObj_destroy (ToneMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      tonemeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ToneMeasureObj_enter (ToneMeasureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ToneMeasureObj_exit (ToneMeasureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      tonemeas_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ToneMeasureObj_methods[] = {
  { "reset", (PyCFunction)ToneMeasureObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "analyze", (PyCFunction)ToneMeasureObj_analyze, METH_VARARGS,
    "analyze(x) -> ToneMetrics record (snr, sinad, thd, thd_pct, thd_n, "
    "sfdr_dbc, sfdr_dbfs, enob, enob_fs, noise_floor_dbfs, fund_freq, "
    "fund_dbfs, worst_spur_freq, worst_spur_dbc, worst_spur_is_harm, rbw_hz, "
    "enbw_hz, bin_hz, lobe_bins, n_noise_bins, proc_gain_db, amp_uncert_db, "
    "floor_uncert_db)." },
  { "analyze_complex", (PyCFunction)ToneMeasureObj_analyze_complex,
    METH_VARARGS,
    "analyze_complex(x) -> ToneMetrics record (snr, sinad, thd, thd_pct, "
    "thd_n, sfdr_dbc, sfdr_dbfs, enob, enob_fs, noise_floor_dbfs, fund_freq, "
    "fund_dbfs, worst_spur_freq, worst_spur_dbc, worst_spur_is_harm, rbw_hz, "
    "enbw_hz, bin_hz, lobe_bins, n_noise_bins, proc_gain_db, amp_uncert_db, "
    "floor_uncert_db)." },
  { "time_stats", (PyCFunction)ToneMeasureObj_time_stats, METH_VARARGS,
    "time_stats(x) -> TimeStats record (rms, peak, crest_db, papr_db, "
    "dc_offset, fs_util_pct)." },
  { "spectrum_dbfs", (PyCFunction)ToneMeasureObj_spectrum_dbfs,
    METH_VARARGS | METH_KEYWORDS,
    "spectrum_dbfs(x) -> ndarray\n"
    "\n"
    "DC-centred dBFS magnitude spectrum of a capture (length nfft, for "
    "plots).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import ToneMeasure\n"
    "    >>> obj = ToneMeasure(8192, 1.0, 8, 1.0, 0, 0.0, 0)\n"
    "    >>> y = obj.spectrum_dbfs(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "spectrum_dbfs_max_out", (PyCFunction)ToneMeasureObj_spectrum_dbfs_max_out,
    METH_NOARGS,
    "spectrum_dbfs_max_out() -> int\n\nMax output length spectrum_dbfs() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)ToneMeasureObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)ToneMeasureObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)ToneMeasureObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject ToneMeasureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "measure.ToneMeasure",
  .tp_basicsize                           = sizeof (ToneMeasureObject),
  .tp_dealloc                             = (destructor)ToneMeasureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a ToneMeasure analyser (auto Kaiser window).\n",
  .tp_methods = ToneMeasureObj_methods,
  .tp_getset  = ToneMeasure_getset,
  .tp_new     = ToneMeasureObj_new,
  .tp_init    = (initproc)ToneMeasureObj_init,
};
