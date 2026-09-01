/*
 * dsss_ext_persistentburstcapture.c — PersistentBurstCapture type for the dsss
 * module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* PersistentBurstCaptureObject — wraps burst_capture_state_t *       */
/* ======================================================== */

#include "burst_capture/burst_capture_core.h"

typedef struct
{
  PyObject_HEAD burst_capture_state_t *handle;
} PersistentBurstCaptureObject;

static void
PersistentBurstCaptureObj_dealloc (PersistentBurstCaptureObject *self)
{
  if (self->handle)
    burst_capture_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PersistentBurstCaptureObj_new (PyTypeObject *type, PyObject *args,
                               PyObject *kwds)
{
  PersistentBurstCaptureObject *self
      = (PersistentBurstCaptureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PersistentBurstCaptureObj_init (PersistentBurstCaptureObject *self,
                                PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "path", "acq_code",  "burst_len",  "reps",
          "spc",  "chip_rate", "cn0_dbhz",   "doppler_uncertainty",
          "pfa",  "pd",        "noise_mode", NULL };
  PyObject          *acq_code_obj        = NULL;
  PyObject          *path                = NULL; /* fspath -> bytes */
  unsigned long long burst_len_raw       = 8192;
  unsigned long long reps_raw            = 5;
  unsigned long long spc_raw             = 4;
  double             chip_rate           = 1000000.0;
  double             cn0_dbhz            = 50.0;
  double             doppler_uncertainty = 0.0;
  double             pfa                 = 1e-3;
  double             pd                  = 0.9;
  const char        *noise_mode_str      = "mean";

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O&O|KKKddddds", kwlist, PyUnicode_FSConverter, &path,
          &acq_code_obj, &burst_len_raw, &reps_raw, &spc_raw, &chip_rate,
          &cn0_dbhz, &doppler_uncertainty, &pfa, &pd, &noise_mode_str))
    {
      Py_XDECREF (path);
      return -1;
    }
  size_t burst_len  = (size_t)burst_len_raw;
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
      Py_XDECREF (path);
      return -1;
    }
  PyArrayObject *acq_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      acq_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!acq_code_arr)
    {
      Py_XDECREF (path);
      return -1;
    }
  size_t acq_code_len = (size_t)PyArray_SIZE (acq_code_arr);
  self->handle        = burst_capture_create_backed (
      PyBytes_AS_STRING (path), (const uint8_t *)PyArray_DATA (acq_code_arr),
      acq_code_len, burst_len, reps, spc, chip_rate, cn0_dbhz,
      doppler_uncertainty, pfa, pd, noise_mode);
  Py_XDECREF (path);
  Py_DECREF (acq_code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "BurstCapture: invalid parameter (need non-empty "
                       "acq_code, reps >= 1, spc >= 1, chip_rate > 0, "
                       "burst_len >= 1, cn0_dbhz > 0, 0 < pfa < 1, 0 < pd < "
                       "1)");
      return -1;
    }
  return 0;
}

static PyObject *
PersistentBurstCaptureObj_push_max_out (PersistentBurstCaptureObject *self,
                                        PyObject                     *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t x_len = 0;
  if (!PyArg_ParseTuple (args, "n", &x_len))
    return NULL;
  return PyLong_FromSize_t (
      burst_capture_push_max_out (self->handle, (size_t)x_len));
}

static PyObject *
PersistentBurstCaptureObj_push (PersistentBurstCaptureObject *self,
                                PyObject *args, PyObject *kwds)
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
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
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
          Py_DECREF (x_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = burst_capture_push_max_out (self->handle,
                                                 (size_t)PyArray_SIZE (x_arr));
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex       *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = burst_capture_push (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
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
  size_t _cap  = burst_capture_push_max_out (self->handle,
                                             (size_t)PyArray_SIZE (x_arr));
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = burst_capture_push (self->handle, _ng0, _ng1, _d0, _cap);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
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
PersistentBurstCaptureObj_events_max_out (PersistentBurstCaptureObject *self,
                                          PyObject                     *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (
      burst_capture_events_max_out (self->handle, (size_t)n));
}

static PyArray_Descr *PersistentBurstCaptureObj_events_dtype = NULL;

/* The record's numpy dtype, built from the compiler's own layout:
   offsetof/sizeof, never numpy's packing rules, so a padded
   struct cannot silently read every row after the first from the
   wrong bytes. */
static PyArray_Descr *
PersistentBurstCaptureObj_events_get_dtype (void)
{
  PyObject      *names = NULL, *formats = NULL;
  PyObject      *offsets = NULL, *spec = NULL;
  PyArray_Descr *out = NULL;
  if (PersistentBurstCaptureObj_events_dtype)
    {
      Py_INCREF (PersistentBurstCaptureObj_events_dtype);
      return PersistentBurstCaptureObj_events_dtype;
    }
  names = Py_BuildValue ("[sssss]", "preamble_start", "doppler_hz_est",
                         "doppler_res_hz", "cn0_dbhz_est", "refine_margin");
  if (!names)
    goto done;
  formats = PyList_New (5);
  if (!formats)
    goto done;
  PyList_SET_ITEM (formats, 0, (PyObject *)PyArray_DescrFromType (NPY_UINT64));
  PyList_SET_ITEM (formats, 1, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 2, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 3, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 4, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  offsets = Py_BuildValue (
      "[nnnnn]", (Py_ssize_t)offsetof (burst_capture_event_t, preamble_start),
      (Py_ssize_t)offsetof (burst_capture_event_t, doppler_hz_est),
      (Py_ssize_t)offsetof (burst_capture_event_t, doppler_res_hz),
      (Py_ssize_t)offsetof (burst_capture_event_t, cn0_dbhz_est),
      (Py_ssize_t)offsetof (burst_capture_event_t, refine_margin));
  if (!offsets)
    goto done;
  spec = Py_BuildValue ("{s:O,s:O,s:O,s:n}", "names", names, "formats",
                        formats, "offsets", offsets, "itemsize",
                        (Py_ssize_t)sizeof (burst_capture_event_t));
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
      PersistentBurstCaptureObj_events_dtype = out;
      Py_INCREF (PersistentBurstCaptureObj_events_dtype);
    }
  return out;
}

static PyObject *
PersistentBurstCaptureObj_events (PersistentBurstCaptureObject *self,
                                  PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact record dtype AND C-contiguity. Compared with
       * EquivTypes against the generated descr: a structured array's type
       * num is NPY_VOID, so a scalar enum cannot say what layout is
       * wanted, and coercing to one would silently reinterpret the
       * caller's buffer. */
      {
        PyArray_Descr *_want = PersistentBurstCaptureObj_events_get_dtype ();
        if (!_want)
          {
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
            return NULL;
          }
        Py_DECREF (_want);
      }
      PyArrayObject *out_arr = (PyArrayObject *)out_obj;
      Py_INCREF (out_arr);
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = burst_capture_events_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = burst_capture_events (
          self->handle, (size_t)n,
          (burst_capture_event_t *)PyArray_DATA (out_arr), _cap);
      npy_intp       _odim   = (npy_intp)n_out;
      PyArray_Descr *_vdescr = PersistentBurstCaptureObj_events_get_dtype ();
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = burst_capture_events_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp       _adim  = (npy_intp)_cap;
  PyArray_Descr *_descr = PersistentBurstCaptureObj_events_get_dtype ();
  if (!_descr)
    {
      return NULL;
    }
  PyObject *arr0 = PyArray_NewFromDescr (&PyArray_Type, _descr, 1, &_adim,
                                         NULL, NULL, 0, NULL);
  if (!arr0)
    {
      return NULL;
    }
  burst_capture_event_t *_d0
      = (burst_capture_event_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = burst_capture_events (self->handle, (size_t)n, _d0, _cap);
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
PersistentBurstCaptureObj_configure_search_raw (
    PersistentBurstCaptureObject *self, PyObject *args, PyObject *kwds)
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
  int    _rc = burst_capture_configure_search_raw (self->handle, doppler_bins,
                                                   n_noncoh);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure_search_raw failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PersistentBurstCaptureObj_reset (PersistentBurstCaptureObject *self,
                                 PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  burst_capture_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
PersistentBurstCaptureObj_state_bytes (PersistentBurstCaptureObject *self,
                                       PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (burst_capture_state_bytes (self->handle));
}

static PyObject *
PersistentBurstCaptureObj_get_state (PersistentBurstCaptureObject *self,
                                     PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = burst_capture_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  burst_capture_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
PersistentBurstCaptureObj_set_state (PersistentBurstCaptureObject *self,
                                     PyObject                     *arg)
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
      != burst_capture_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (burst_capture_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
PersistentBurstCapture_getprop_preamble_start (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_preamble_start (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_doppler_hz_est (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_doppler_hz_est (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_doppler_res_hz (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_doppler_res_hz (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_cn0_dbhz_est (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_cn0_dbhz_est (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_refine_margin (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_refine_margin (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_burst_len (PersistentBurstCaptureObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->burst_len);
}
static PyObject *
PersistentBurstCapture_getprop_refine_span (PersistentBurstCaptureObject *self,
                                            void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->refine_span);
}
static PyObject *
PersistentBurstCapture_getprop_retain_span (PersistentBurstCaptureObject *self,
                                            void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->retain_span);
}
static PyObject *
PersistentBurstCapture_getprop_underpowered (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->underpowered));
}
static PyObject *
PersistentBurstCapture_getprop_pd_predicted (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_pd_predicted (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_eta (PersistentBurstCaptureObject *self,
                                    void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_eta (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_eta_nc (PersistentBurstCaptureObject *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_eta_nc (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_straddle_loss (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_straddle_loss (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_doppler_bins (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_doppler_bins (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_n_noncoh (PersistentBurstCaptureObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_n_noncoh (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_code_bins (PersistentBurstCaptureObject *self,
                                          void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_code_bins (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_doppler_span_hz (
    PersistentBurstCaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (burst_capture_get_doppler_span_hz (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_pending (PersistentBurstCaptureObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_pending (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_dropped (PersistentBurstCaptureObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_dropped (self->handle));
}
static PyObject *
PersistentBurstCapture_getprop_n_bursts (PersistentBurstCaptureObject *self,
                                         void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)burst_capture_get_n_bursts (self->handle));
}

static PyObject *
PersistentBurstCapture_getprop_min_gap (PersistentBurstCaptureObject *self,
                                        void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->min_gap);
}

static PyGetSetDef PersistentBurstCapture_getset[] = {
  { "preamble_start", (getter)PersistentBurstCapture_getprop_preamble_start,
    NULL,
    "Stream-absolute sample index the most recent window's preamble starts "
    "at. NEVER LATE: a window that began after the preamble has destroyed the "
    "burst, so the refine stage's obligation is to be early-or-exact and to "
    "say how early.\n",
    NULL },
  { "doppler_hz_est", (getter)PersistentBurstCapture_getprop_doppler_hz_est,
    NULL,
    "Folded/signed coarse Doppler estimate of the most recent window, Hz. "
    "Acquisition's own bin, mapped through dp_fftfreq -- the ONE home for "
    "that fold, because a consumer seeded on the wrong side of it is off by "
    "the full search span.\n",
    NULL },
  { "doppler_res_hz", (getter)PersistentBurstCapture_getprop_doppler_res_hz,
    NULL,
    "Acquisition's native Doppler bin width = chip_rate/(sf*coherent_bins), "
    "Hz. The width of doppler_hz_est: the estimate is that value +/- half of "
    "this.\n",
    NULL },
  { "cn0_dbhz_est", (getter)PersistentBurstCapture_getprop_cn0_dbhz_est, NULL,
    "Estimated carrier-to-noise density of the most recent window (dB-Hz), "
    "backed out of the hit's test statistic. A LOWER BOUND: it tracks the "
    "true C/N0 while receiver noise dominates the CFAR estimate, then "
    "saturates at the code's own autocorrelation-sidelobe floor once the true "
    "C/N0 exceeds what this code and geometry can resolve -- a real ceiling, "
    "not a fault.\n",
    NULL },
  { "refine_margin", (getter)PersistentBurstCapture_getprop_refine_margin,
    NULL,
    "The refine stage's own confidence: the best rival code period's score "
    "over the winner's. The envelope is (reps-1)/reps when the right "
    "repetition wins, so compare against THAT and never a constant -- the "
    "floor rises with depth (0.55 at reps=2, 0.77 at 4, 0.94 at 16). Near 1 "
    "means the period was not resolved, which nothing else in the chain can "
    "see.\n",
    NULL },
  { "burst_len", (getter)PersistentBurstCapture_getprop_burst_len, NULL,
    "Samples in one emitted window -- the burst length this capture was built "
    "for, and the stride of a row in push()'s return.\n",
    NULL },
  { "refine_span", (getter)PersistentBurstCapture_getprop_refine_span, NULL,
    "Coalescing window, in samples -- the reach over which two detections\n"
    "are ONE preamble.\n"
    "\n"
    "Both sides of that test are burst STARTS (resolved code epochs), so "
    "this\n"
    "bounds start-to-start separation, NOT the dead air between bursts. The "
    "two\n"
    "differ by a whole burst, and reading it as dead air costs a caller real\n"
    "airtime for nothing: the gap actually required is\n"
    "`max(0, refine_span - burst_len)`, which is 0 whenever a burst is longer "
    "than\n"
    "the refine reach (doppler#1085).\n",
    NULL },
  { "retain_span", (getter)PersistentBurstCapture_getprop_retain_span, NULL,
    "History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.\n"
    "\n"
    "`refine_span` plus one whole burst. A burst closer than this to the end "
    "of\n"
    "what has been pushed is held rather than emitted, because refine cannot "
    "yet\n"
    "see the samples it needs. Feed at least this many more, or the last "
    "burst of\n"
    "a capture never comes out.\n",
    NULL },
  { "underpowered", (getter)PersistentBurstCapture_getprop_underpowered, NULL,
    "True when the search cannot meet the requested `pd` at this `cn0_dbhz` "
    "and geometry — `pd_predicted < pd`. The grid is still built, "
    "best-effort, so the symptom is bursts that are never captured rather "
    "than a failure. Construction also emits a UserWarning; this is the same "
    "fact as a value, for a caller that would rather ask than catch.\n",
    NULL },
  { "pd_predicted", (getter)PersistentBurstCapture_getprop_pd_predicted, NULL,
    "Detection probability the sized grid actually predicts at `cn0_dbhz`. "
    "The number behind `underpowered`, and the one to compare against the "
    "`pd` that was asked for.\n",
    NULL },
  { "eta", (getter)PersistentBurstCapture_getprop_eta, NULL,
    "Coherent detection gate: the normalised statistic a single-look decision "
    "must clear, from `pfa` spread across the search surface. In force when "
    "`n_noncoh == 1`.\n",
    NULL },
  { "eta_nc", (getter)PersistentBurstCapture_getprop_eta_nc, NULL,
    "Non-coherent detection gate — the one in force when `n_noncoh > 1`, "
    "which is the usual case. Higher than `eta` for the same `pfa`, because "
    "combining looks costs the threshold what it buys in sensitivity.\n",
    NULL },
  { "straddle_loss", (getter)PersistentBurstCapture_getprop_straddle_loss,
    NULL,
    "Correlation kept, worst case, by a burst landing BETWEEN grid points "
    "rather than on one. The search is a finite grid in Doppler and code "
    "phase, so a real burst almost never sits on a hypothesis exactly; this "
    "is what that costs, and it is already priced into `pd_predicted`.\n",
    NULL },
  { "doppler_bins", (getter)PersistentBurstCapture_getprop_doppler_bins, NULL,
    "Doppler hypotheses searched — the coherent depth the sizer chose, "
    "bounded by `reps`. `configure_search_raw` is what pins it.\n",
    NULL },
  { "n_noncoh", (getter)PersistentBurstCapture_getprop_n_noncoh, NULL,
    "Non-coherent looks combined per decision. Above 1 the object needs that "
    "many frames before it can decide at all, which is why a caller sweeping "
    "in short dwells has to pin it.\n",
    NULL },
  { "code_bins", (getter)PersistentBurstCapture_getprop_code_bins, NULL,
    "Code-phase hypotheses per Doppler row: one segment in samples, `sf * "
    "spc`.\n",
    NULL },
  { "doppler_span_hz", (getter)PersistentBurstCapture_getprop_doppler_span_hz,
    NULL,
    "Unambiguous Doppler half-range, ± this. Beyond it the per-segment "
    "integrate-and-dump's sinc rolloff suppresses the correlation, so a burst "
    "outside the span is not merely harder to find — it is nulled.\n",
    NULL },
  { "pending", (getter)PersistentBurstCapture_getprop_pending, NULL,
    "Detections held because their burst window has NOT fully arrived.\n"
    "\n"
    "push() deliberately emits nothing for these: a window is returned when "
    "it is\n"
    "complete, not when it is guessed at. What this exists for is the other "
    "end --\n"
    "a caller closing a file or a socket while this is non-zero is discarding "
    "a\n"
    "burst that would have been captured, and every other read-back looks\n"
    "identical to \"nothing was ever there\".\n",
    NULL },
  { "dropped", (getter)PersistentBurstCapture_getprop_dropped, NULL,
    "Samples the history ring refused, lifetime. A LOST BURST each, not a "
    "statistic -- it survives reset().\n",
    NULL },
  { "n_bursts", (getter)PersistentBurstCapture_getprop_n_bursts, NULL,
    "Windows emitted, lifetime.\n", NULL },
  { "min_gap", (getter)PersistentBurstCapture_getprop_min_gap, NULL,
    "Dead air to leave BETWEEN bursts, in samples — edge to edge, not\n"
    "start to start.\n"
    "\n"
    "Derived rather than documented as a rule the caller has to apply: a\n"
    "detection's anchor is the code epoch of whichever frame detected, and\n"
    "acquisition's framing is not aligned to the preamble, so the last frame "
    "that\n"
    "can detect sits up to `reps * code_period` past the true start. CLAIM "
    "merges\n"
    "two anchors closer than `refine_span`, so a pair survives only when\n"
    "`gap >= refine_span + reps*code_period - burst_len`.\n"
    "\n"
    "**Zero is a real answer** — a burst longer than `refine_span + reps*P` "
    "needs\n"
    "no gap for the claim rule's sake — but it does not mean zero is wise: a "
    "zero\n"
    "gap is a continuous stream rather than a burst link, and it measures 88% "
    "at a\n"
    "geometry where this reads 0.\n"
    "\n"
    "Replaces the prose `max(0, refine_span - burst_len)`, which was short by "
    "the\n"
    "whole detection-lag term: 32 samples against 528 at the C suite's "
    "geometry\n"
    "(doppler#1172).\n",
    NULL },
  { NULL }
};

static PyObject *
PersistentBurstCaptureObj_destroy (PersistentBurstCaptureObject *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      burst_capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PersistentBurstCaptureObj_enter (PersistentBurstCaptureObject *self,
                                 PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PersistentBurstCaptureObj_exit (PersistentBurstCaptureObject *self,
                                PyObject                     *args)
{
  (void)args;
  if (self->handle)
    {
      burst_capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyArray_Descr *PersistentBurstCaptureObj_detections_dtype = NULL;

/* The record's numpy dtype, built from the compiler's own layout:
   offsetof/sizeof, never numpy's packing rules, so a padded
   struct cannot silently read every row after the first from the
   wrong bytes. */
static PyArray_Descr *
PersistentBurstCaptureObj_detections_get_dtype (void)
{
  PyObject      *names = NULL, *formats = NULL;
  PyObject      *offsets = NULL, *spec = NULL;
  PyArray_Descr *out = NULL;
  if (PersistentBurstCaptureObj_detections_dtype)
    {
      Py_INCREF (PersistentBurstCaptureObj_detections_dtype);
      return PersistentBurstCaptureObj_detections_dtype;
    }
  names = Py_BuildValue ("[sssss]", "epoch", "doppler_hz", "cn0_dbhz",
                         "test_stat", "peak_mag");
  if (!names)
    goto done;
  formats = PyList_New (5);
  if (!formats)
    goto done;
  PyList_SET_ITEM (formats, 0, (PyObject *)PyArray_DescrFromType (NPY_UINT64));
  PyList_SET_ITEM (formats, 1, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 2, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 3, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  PyList_SET_ITEM (formats, 4, (PyObject *)PyArray_DescrFromType (NPY_DOUBLE));
  offsets = Py_BuildValue (
      "[nnnnn]", (Py_ssize_t)offsetof (burst_capture_detection_t, epoch),
      (Py_ssize_t)offsetof (burst_capture_detection_t, doppler_hz),
      (Py_ssize_t)offsetof (burst_capture_detection_t, cn0_dbhz),
      (Py_ssize_t)offsetof (burst_capture_detection_t, test_stat),
      (Py_ssize_t)offsetof (burst_capture_detection_t, peak_mag));
  if (!offsets)
    goto done;
  spec = Py_BuildValue ("{s:O,s:O,s:O,s:n}", "names", names, "formats",
                        formats, "offsets", offsets, "itemsize",
                        (Py_ssize_t)sizeof (burst_capture_detection_t));
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
      PersistentBurstCaptureObj_detections_dtype = out;
      Py_INCREF (PersistentBurstCaptureObj_detections_dtype);
    }
  return out;
}

static PyObject *
PersistentBurstCaptureObj_detections (PersistentBurstCaptureObject *self,
                                      PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact record dtype AND C-contiguity. Compared with
       * EquivTypes against the generated descr: a structured array's type
       * num is NPY_VOID, so a scalar enum cannot say what layout is
       * wanted, and coercing to one would silently reinterpret the
       * caller's buffer. */
      {
        PyArray_Descr *_want
            = PersistentBurstCaptureObj_detections_get_dtype ();
        if (!_want)
          {
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
            return NULL;
          }
        Py_DECREF (_want);
      }
      PyArrayObject *out_arr = (PyArrayObject *)out_obj;
      Py_INCREF (out_arr);
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      size_t _omax
          = burst_capture_detections_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = burst_capture_detections (
          self->handle, (size_t)n,
          (burst_capture_detection_t *)PyArray_DATA (out_arr), _cap);
      npy_intp       _odim = (npy_intp)n_out;
      PyArray_Descr *_vdescr
          = PersistentBurstCaptureObj_detections_get_dtype ();
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
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)n;
  size_t _cap  = burst_capture_detections_max_out (self->handle, (size_t)n);
  (void)_need;
  npy_intp       _adim  = (npy_intp)_cap;
  PyArray_Descr *_descr = PersistentBurstCaptureObj_detections_get_dtype ();
  if (!_descr)
    {
      return NULL;
    }
  PyObject *arr0 = PyArray_NewFromDescr (&PyArray_Type, _descr, 1, &_adim,
                                         NULL, NULL, 0, NULL);
  if (!arr0)
    {
      return NULL;
    }
  burst_capture_detection_t *_d0
      = (burst_capture_detection_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = burst_capture_detections (self->handle, (size_t)n, _d0, _cap);
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
PersistentBurstCaptureObj_detections_max_out (
    PersistentBurstCaptureObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (
      burst_capture_detections_max_out (self->handle, (size_t)n));
}

static PyMethodDef PersistentBurstCaptureObj_methods[] = {

  { "push", (PyCFunction)(void *)PersistentBurstCaptureObj_push,
    METH_VARARGS | METH_KEYWORDS,
    "push(x, out) -> ndarray\n"
    "\n"
    "Stream raw cf32 samples and get back the SAMPLES of every burst\n"
    "whose window has fully arrived, concatenated: burst i occupies\n"
    "burst_len samples starting at i*burst_len, and events() returns the\n"
    "matching record for each. Samples feed the embedded BurstAcquisition\n"
    "and are retained in a history ring; when a detection fires, the refine\n"
    "stage correlates one code period at each preamble position to recover\n"
    "the exact preamble start -- the one quantity acquisition structurally\n"
    "cannot report, since its code_phase is a lag modulo one code period --\n"
    "and the window is emitted the moment its last sample has arrived. It\n"
    "stops there: what to DO with a burst (demodulate it, write it to a\n"
    "file, ship it to another process) is the caller's. An empty return is\n"
    "normal, not an error: it means no burst completed in this call. Accepts\n"
    "any block size -- the history ring is a contiguous window over the\n"
    "stream and is never reset between bursts, so a burst whose tail falls\n"
    "outside one call is completed by a later one.\n"
    "\n"
    "Windows are concatenated: burst `i` occupies `burst_len` samples\n"
    "starting at `i*burst_len`, and events() returns the matching record for\n"
    "each. Every sample of x is consumed. An empty return is normal -- it\n"
    "means no burst completed in this call.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input samples, x_len long.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Written with the completed windows; may be NULL to drop.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Samples written -- always a multiple of `burst_len`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> win = cap.push(np.zeros(4096, dtype=np.complex64))\n"
    ">>> win.size % cap.burst_len        # whole windows, never a partial\n"
    "0\n"
    ">>> win.size                        # silence, so no burst completed\n"
    "0\n" },
  { "push_max_out", (PyCFunction)PersistentBurstCaptureObj_push_max_out,
    METH_VARARGS,
    "push_max_out(x_len) -> int\n"
    "\n"
    "Upper bound on samples push() can return for x_len input.\n"
    "\n"
    "Distinct bursts cannot overlap, so `x_len` samples complete at most\n"
    "`x_len/burst_len + 1` of them, plus whatever is already queued.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x_len : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "events", (PyCFunction)(void *)PersistentBurstCaptureObj_events,
    METH_VARARGS | METH_KEYWORDS,
    "events(count=1) -> ndarray\n"
    "\n"
    "The event record for each burst the last push() returned. Row i\n"
    "describes the window at samples[i*burst_len ...] of that push. Valid\n"
    "until the next push(), reset() or set_state().\n"
    "\n"
    "Row `i` describes the window at `i*burst_len`. Valid until the next\n"
    "push(), reset() or set_state().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[Any] | None\n"
    "    Optional pre-allocated output buffer. When given, the result is\n"
    "    written into it and the returned array is a view of exactly the\n"
    "    samples produced; when omitted, a fresh array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> win = cap.push(np.zeros(4096, dtype=np.complex64))\n"
    ">>> len(cap.events()) == win.size // cap.burst_len\n"
    "True\n"
    "\n"
    "Fields\n"
    "------\n"
    "preamble_start : int\n"
    "    Exact stream position of the preamble.\n"
    "doppler_hz_est : float\n"
    "    Signed coarse Doppler, Hz.\n"
    "doppler_res_hz : float\n"
    "    Acquisition's native bin width, Hz.\n"
    "cn0_dbhz_est : float\n"
    "    C/N0 lower bound from the hit, dB-Hz.\n"
    "refine_margin : float\n"
    "    Runner-up period over the winner.\n" },
  { "events_max_out", (PyCFunction)PersistentBurstCaptureObj_events_max_out,
    METH_VARARGS,
    "events_max_out(n) -> int\n"
    "\n"
    "Records available from the last push(). n is ignored.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "configure_search_raw",
    (PyCFunction)(void *)PersistentBurstCaptureObj_configure_search_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_search_raw(doppler_bins, n_noncoh) -> None\n"
    "\n"
    "Pin the embedded BurstAcquisition's search grid directly, bypassing\n"
    "the auto-sizing -- the escape hatch for a caller who wants a specific\n"
    "(doppler_bins, n_noncoh). Forwards to the engine unchanged.\n"
    "\n"
    "The escape hatch for a caller who wants a specific (doppler_bins,\n"
    "n_noncoh). Forwards to the engine unchanged.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "doppler_bins : int\n"
    "    Input.\n"
    "n_noncoh : int\n"
    "    Input.\n"
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
    ">>> from doppler.dsss import BurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only\n" },
  { "reset", (PyCFunction)PersistentBurstCaptureObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Return to the searching state: resets the embedded acquisition,\n"
    "drops the history ring's contents, clears every queued detection and\n"
    "every read-back, so a fresh stream cannot inherit the previous one's\n"
    "position. Construction parameters are untouched.\n"
    "\n"
    "Resets the embedded acquisition, rewinds the history ring, clears every\n"
    "queued detection and every read-back. Construction parameters are\n"
    "untouched; `dropped` deliberately survives, because a lost burst stays\n"
    "lost.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> cap.push(np.zeros(4096, dtype=np.complex64)).size\n"
    "0\n"
    ">>> cap.reset()\n"
    ">>> cap.pending\n"
    "0\n" },
  { "state_bytes", (PyCFunction)PersistentBurstCaptureObj_state_bytes,
    METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the PersistentBurstCapture has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)PersistentBurstCaptureObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the PersistentBurstCapture has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)PersistentBurstCaptureObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the PersistentBurstCapture has already been\n"
    "destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)PersistentBurstCaptureObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)PersistentBurstCaptureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a BurstCapture be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "BurstCapture\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)PersistentBurstCaptureObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the BurstCapture.\n"
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
  { "detections", (PyCFunction)(void *)PersistentBurstCaptureObj_detections,
    METH_VARARGS | METH_KEYWORDS,
    "detections(count=1) -> ndarray\n"
    "\n"
    "Every hit the search made in the last push(), unfiltered — before\n"
    "the claim rule coalesced the several detections of one preamble, and\n"
    "before the suppression window dropped the ones inside a burst already\n"
    "captured. So several rows can name one burst and a row can be a false\n"
    "alarm; that is the point. Each carries the STREAM-ABSOLUTE code epoch,\n"
    "which acquisition's own `code_phase` is not (it is a lag modulo one\n"
    "code period), plus the folded Doppler, the C/N0 lower bound and the\n"
    "CFAR statistic that gated it. Read `events()` instead for the bursts\n"
    "that survived and whose windows arrived. Valid until the next push(),\n"
    "reset() or set_state().\n"
    "\n"
    "BEFORE the claim rule and the suppression window: several rows can name\n"
    "one preamble, and a row can be a false alarm. That is the point -- this\n"
    "is what acquisition FOUND, and `events()` is what survived. Valid until\n"
    "the next push(), reset() or set_state().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "count : int\n"
    "    How many output samples to ask for. The call may return fewer; size\n"
    "    an `out=` buffer with the matching `_max_out()` when you need the\n"
    "    worst case.\n"
    "out : NDArray[Any] | None\n"
    "    Optional pre-allocated output buffer. When given, the result is\n"
    "    written into it and the returned array is a view of exactly the\n"
    "    samples produced; when omitted, a fresh array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import BurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> _ = cap.push(np.zeros(4096, dtype=np.complex64))\n"
    ">>> # what the search found, against what became a burst\n"
    ">>> len(cap.detections()) >= len(cap.events())\n"
    "True\n"
    "\n"
    "Fields\n"
    "------\n"
    "epoch : int\n"
    "    Stream-absolute code epoch of the hit.\n"
    "doppler_hz : float\n"
    "    Signed coarse Doppler, folded, Hz.\n"
    "cn0_dbhz : float\n"
    "    C/N0 lower bound from the hit, dB-Hz.\n"
    "test_stat : float\n"
    "    The CFAR gating statistic, peak over noise.\n"
    "peak_mag : float\n"
    "    Raw CFAR peak magnitude.\n" },
  { "detections_max_out",
    (PyCFunction)PersistentBurstCaptureObj_detections_max_out, METH_VARARGS,
    "detections_max_out(n) -> int\n"
    "\n"
    "Raw detections available from the last push(). n is ignored.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { NULL }
};

static PyTypeObject PersistentBurstCaptureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.PersistentBurstCapture",
  .tp_basicsize = sizeof (PersistentBurstCaptureObject),
  .tp_dealloc   = (destructor)PersistentBurstCaptureObj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a capture whose look-back lives in a FILE.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str | os.PathLike\n"
    "    File to back the ring with; not NULL and not empty.\n"
    "acq_code : NDArray[np.uint8]\n"
    "    Preamble PN chips (0/1), length acq_code_len.\n"
    "burst_len : int, default 8192\n"
    "    Samples in one burst -- what gets captured.\n"
    "reps : int, default 5\n"
    "    Preamble code repetitions.\n"
    "spc : int, default 4\n"
    "    Samples per chip.\n"
    "chip_rate : float, default 1000000.0\n"
    "    Chip rate, Hz.\n"
    "cn0_dbhz : float, default 50.0\n"
    "    C/N0 the search is sized for, dB-Hz.\n"
    "doppler_uncertainty : float, default 0.0\n"
    "    Doppler search half-range, Hz (0 = native).\n"
    "pfa : float, default 1e-3\n"
    "    Target false-alarm probability, in (0, 1).\n"
    "pd : float, default 0.9\n"
    "    Target detection probability, in (0, 1).\n"
    "noise_mode : Literal[\"mean\", \"median\", \"min\", \"max\"], default "
    "\"mean\"\n"
    "    CFAR reference: 0=mean, 1=median, 2=min, 3=max.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np, tempfile, os\n"
    ">>> from doppler.dsss import BurstCapture, PersistentBurstCapture\n"
    ">>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)\n"
    ">>> path = os.path.join(tempfile.mkdtemp(), \"ring.cf32\")\n"
    ">>> cap = PersistentBurstCapture(path, code, burst_len=512,\n"
    "...                             reps=4, spc=2)\n"
    ">>> ram = BurstCapture(code, burst_len=512, reps=4, spc=2)\n"
    ">>> _ = cap.push(np.zeros(4096, dtype=np.complex64))\n"
    ">>> # the look-back is in the file, so the blob stops carrying it\n"
    ">>> ram.state_bytes() - cap.state_bytes() == ram.retain_span * 8\n"
    "True\n"
    ">>> os.path.getsize(path) > 0\n"
    "True\n",
  .tp_methods = PersistentBurstCaptureObj_methods,
  .tp_getset  = PersistentBurstCapture_getset,
  .tp_new     = PersistentBurstCaptureObj_new,
  .tp_init    = (initproc)PersistentBurstCaptureObj_init,
};
