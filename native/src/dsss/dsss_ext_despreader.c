/*
 * dsss_ext_despreader.c — Despreader type for the dsss module.
 *
 * Included by dsss_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only dsss_ext.c is compiled.
 */
/* ======================================================== */
/* DespreaderObject — wraps despreader_state_t *       */
/* ======================================================== */

#include "despreader/despreader_core.h"

typedef struct
{
  PyObject_HEAD despreader_state_t *handle;
} DespreaderObject;

static void
DespreaderObj_dealloc (DespreaderObject *self)
{
  if (self->handle)
    despreader_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DespreaderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DespreaderObject *self = (DespreaderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DespreaderObj_init (DespreaderObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = {
    "code",    "sps",    "init_norm_freq", "init_chip", "bn_carrier",
    "bn_code", "bn_fll", "zeta",           "spacing",   "periods_per_bit",
    NULL
  };
  PyObject          *code_obj            = NULL;
  unsigned long long sps_raw             = 4;
  double             init_norm_freq      = 0.0;
  double             init_chip           = 0.0;
  double             bn_carrier          = 0.05;
  double             bn_code             = 0.005;
  double             bn_fll              = 0.0;
  double             zeta                = 0.707;
  double             spacing             = 0.5;
  unsigned long long periods_per_bit_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KdddddddK", kwlist,
                                    &code_obj, &sps_raw, &init_norm_freq,
                                    &init_chip, &bn_carrier, &bn_code, &bn_fll,
                                    &zeta, &spacing, &periods_per_bit_raw))
    return -1;
  size_t         sps             = (size_t)sps_raw;
  size_t         periods_per_bit = (size_t)periods_per_bit_raw;
  PyArrayObject *code_arr        = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = despreader_create (
      (const uint8_t *)PyArray_DATA (code_arr), code_len, sps, init_norm_freq,
      init_chip, bn_carrier, bn_code, bn_fll, zeta, spacing, periods_per_bit);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "despreader_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
DespreaderObj_steps_max_out (DespreaderObject *self,
                             PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (despreader_steps_max_out (self->handle));
}

static PyObject *
DespreaderObj_steps (DespreaderObject *self, PyObject *args, PyObject *kwds)
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
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = despreader_steps_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex       *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = despreader_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = despreader_steps_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
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
    n_out = despreader_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
DespreaderObj_bits_max_out (DespreaderObject *self,
                            PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (despreader_bits_max_out (self->handle));
}

static PyObject *
DespreaderObj_bits (DespreaderObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_UINT8
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
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = despreader_bits_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      uint8_t             *_ng2 = (uint8_t *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = despreader_bits (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  size_t _cap  = despreader_bits_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_UINT8);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  uint8_t *_d0 = (uint8_t *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = despreader_bits (self->handle, _ng0, _ng1, _d0, _cap);
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
DespreaderObj_set_telemetry (DespreaderObject *self, PyObject *args,
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
  int      _rc   = despreader_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DespreaderObj_configure_carrier_lock (DespreaderObject *self, PyObject *args,
                                      PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  double        up_thresh   = 0.0;
  double        down_thresh = 0.0;
  unsigned long n_up_raw    = 0UL;
  unsigned long n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_up_raw, &n_down_raw))
    return NULL;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  despreader_configure_carrier_lock (self->handle, up_thresh, down_thresh,
                                     n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
DespreaderObj_configure_code_lock (DespreaderObject *self, PyObject *args,
                                   PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]   = { "pfa", "n_looks", "ref_snr_db", NULL };
  double             pfa         = 0.0;
  unsigned long long n_looks_raw = 0ULL;
  double             ref_snr_db  = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dK|d", _kwlist, &pfa,
                                    &n_looks_raw, &ref_snr_db))
    return NULL;
  size_t n_looks = (size_t)n_looks_raw;
  int    _rc     = despreader_configure_code_lock (self->handle, pfa, n_looks,
                                                   ref_snr_db);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_code_lock failed (rc=%d)",
                    _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DespreaderObj_reset (DespreaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  despreader_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DespreaderObj_state_bytes (DespreaderObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (despreader_state_bytes (self->handle));
}

static PyObject *
DespreaderObj_get_state (DespreaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = despreader_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  despreader_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DespreaderObj_set_state (DespreaderObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != despreader_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (despreader_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Despreader_getprop_norm_freq (DespreaderObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_norm_freq (self->handle));
}
static int
Despreader_setprop_norm_freq (DespreaderObject *self, PyObject *value,
                              void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  despreader_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
Despreader_getprop_code_phase (DespreaderObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_code_phase (self->handle));
}
static PyObject *
Despreader_getprop_code_rate (DespreaderObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_code_rate (self->handle));
}
static PyObject *
Despreader_getprop_lock_metric (DespreaderObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_lock_metric (self->handle));
}
static PyObject *
Despreader_getprop_carrier_locked (DespreaderObject *self,
                                   void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong (
      (long)(despreader_get_carrier_locked (self->handle)));
}
static PyObject *
Despreader_getprop_code_locked (DespreaderObject *self,
                                void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(despreader_get_code_locked (self->handle)));
}
static PyObject *
Despreader_getprop_bit_phase (DespreaderObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)despreader_get_bit_phase (self->handle));
}
static PyObject *
Despreader_getprop_bn_carrier (DespreaderObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_bn_carrier (self->handle));
}
static int
Despreader_setprop_bn_carrier (DespreaderObject *self, PyObject *value,
                               void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  despreader_set_bn_carrier (self->handle, v);
  return 0;
}
static PyObject *
Despreader_getprop_bn_code (DespreaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_bn_code (self->handle));
}
static int
Despreader_setprop_bn_code (DespreaderObject *self, PyObject *value,
                            void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  despreader_set_bn_code (self->handle, v);
  return 0;
}

static PyGetSetDef Despreader_getset[] = {
  { "norm_freq", (getter)Despreader_getprop_norm_freq,
    (setter)Despreader_setprop_norm_freq, "Norm freq.\n", NULL },
  { "code_phase", (getter)Despreader_getprop_code_phase, NULL, "Code phase.\n",
    NULL },
  { "code_rate", (getter)Despreader_getprop_code_rate, NULL,
    "chips advanced per nominal chip (~1.0).\n", NULL },
  { "lock_metric", (getter)Despreader_getprop_lock_metric, NULL,
    "EMA of |Re P|/|P| (1 = locked).\n", NULL },
  { "carrier_locked", (getter)Despreader_getprop_carrier_locked, NULL,
    "Carrier lock decision: the embedded Costas loop's verify-counted "
    "detector on its lock-metric EMA (True = locked; see "
    "Costas.configure_lock).\n",
    NULL },
  { "code_locked", (getter)Despreader_getprop_code_locked, NULL,
    "Code lock decision: the embedded DLL's verify-counted CFAR detector "
    "(True = locked; see Dll.configure_lock). Live in composition — the "
    "despreader runs the same always-on detector Dll.steps does.\n",
    NULL },
  { "bit_phase", (getter)Despreader_getprop_bit_phase, NULL,
    "detected bit boundary (argmax flip_hist).\n", NULL },
  { "bn_carrier", (getter)Despreader_getprop_bn_carrier,
    (setter)Despreader_setprop_bn_carrier, "Bn carrier.\n", NULL },
  { "bn_code", (getter)Despreader_getprop_bn_code,
    (setter)Despreader_setprop_bn_code, "Bn code.\n", NULL },
  { NULL }
};

static PyObject *
DespreaderObj_destroy (DespreaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      despreader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DespreaderObj_enter (DespreaderObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DespreaderObj_exit (DespreaderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      despreader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DespreaderObj_methods[] = {

  { "steps", (PyCFunction)(void *)DespreaderObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Track carrier + code and despread a cf32 block: per sample wipe the "
    "carrier (Costas) and correlate early/prompt/late against the code (DLL), "
    "update both loops each code period, and emit one complex prompt symbol "
    "per period.\n"
    "\n"
    "The continuous kernel: per input sample it wipes the carrier (Costas\n"
    "NCO) and correlates the de-rotated sample against the early/prompt/late\n"
    "code taps (DLL); per code period it dumps the prompt "
    "integrate-and-dump,\n"
    "updates the code loop on the early/late envelopes and the carrier loop\n"
    "on the same prompt, and emits that prompt. A partial period is carried\n"
    "in state across calls, so a long stream can be fed in blocks. Each\n"
    "emitted symbol's sign is the BPSK decision; its phase and magnitude are\n"
    "the soft information.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input CF32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of prompt symbols written into out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)  # one code period\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)            # 0 -> +1, 1 -> -1\n"
    ">>> bits = rng.integers(0, 2, 40).astype(np.uint8)  # 1 bit / period\n"
    ">>> syms = np.where(bits == 1, -1.0, 1.0)\n"
    ">>> rx = np.concatenate(\n"
    "...     [s * np.repeat(chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = Despreader(code=code, sps=4)\n"
    ">>> prompt = d.steps(rx)                    # one prompt per code "
    "period\n"
    ">>> hard = (prompt.real < 0).astype(np.uint8)\n"
    ">>> e = np.mean(hard != bits[:hard.size])   # payload recovered\n"
    ">>> round(float(min(e, 1.0 - e)), 4)\n"
    "0.0\n" },
  { "steps_max_out", (PyCFunction)DespreaderObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)(void *)DespreaderObj_bits,
    METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Same tracking kernel as steps(), but bit-sync the per-period prompts "
    "into hard data bits: periods_per_bit prompts are coherently summed "
    "across each detected bit boundary and one 0/1 bit is emitted per data "
    "bit.\n"
    "\n"
    "The same tracking kernel as despreader_steps(), followed by bit\n"
    "synchronisation: the per-period prompts are coherently summed across\n"
    "each detected bit boundary (a data bit spans periods_per_bit code\n"
    "periods) and one hard 0/1 bit is emitted per data bit. The bit boundary\n"
    "is estimated on-line from the prompt sign-flip histogram, so the phase\n"
    "is a BPSK ambiguity — a globally inverted decision stream is equally\n"
    "correct.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input CF32 samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Number of data bits written into out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)\n"
    ">>> bits = rng.integers(0, 2, 40).astype(np.uint8)\n"
    ">>> syms = np.where(bits == 1, -1.0, 1.0)\n"
    ">>> rx = np.concatenate(\n"
    "...     [s * np.repeat(chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = Despreader(code=code, sps=4)\n"
    ">>> data = d.bits(rx)                       # hard data bits\n"
    ">>> e = np.mean(data != bits[:data.size])   # up to a BPSK sign flip\n"
    ">>> round(float(min(e, 1.0 - e)), 4)\n"
    "0.0\n" },
  { "bits_max_out", (PyCFunction)DespreaderObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n\nMax output length bits() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)DespreaderObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context across the despreader. Pure "
    "forwarder — the despreader registers no probes of its own: the carrier "
    "loop registers \"<prefix>.car.lock\" / \".e\" / \".freq\" / \".locked\" "
    "and the code loop registers \"<prefix>.code.e\" / \".rate\" / \".lock\" "
    "/ \".locked\" (the \".locked\" pair are the loops' verify-counted "
    "lockdet decisions, 0/1) — eight probes, all thinned by decim and emitted "
    "once per code period (the despreader flushes both loops at its "
    "per-period update). Passing NULL detaches both loops.  Setup path, never "
    "hot; the context is borrowed and must outlive the attachment (SPSC rules "
    "in dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"ch0\".\n"
    "decim : int\n"
    "    Emit every decim-th code period; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> code = (np.arange(31) % 2).astype(np.uint8)\n"
    ">>> ch = Despreader(code=code, sps=4)\n"
    ">>> ch.set_telemetry(tlm, \"ch0\")\n"
    ">>> names = sorted(tlm.probe_names)\n"
    ">>> names[:4]\n"
    "['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.car.locked']\n"
    ">>> names[4:]\n"
    "['ch0.code.e', 'ch0.code.lock', 'ch0.code.locked', 'ch0.code.rate']\n"
    ">>> chips = 1.0 - 2.0 * (np.arange(31) % 2)\n"
    ">>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)\n"
    ">>> _ = ch.steps(x)\n"
    ">>> recs = tlm.read()   # eight records per code period\n"
    ">>> len(recs) > 0 and len(recs) % 8 == 0\n"
    "True\n" },
  { "configure_carrier_lock",
    (PyCFunction)(void *)DespreaderObj_configure_carrier_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_carrier_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the embedded carrier loop's lock detector directly: forwards to "
    "the Costas loop's configure_lock (locked flips up after n_up consecutive "
    "symbols with the lock-metric EMA above up_thresh, and drops after n_down "
    "consecutive symbols below down_thresh; see Costas.configure_lock). "
    "Symmetric with the carrier_locked state property: state is readable, so "
    "config should be writable too, rather than forcing a caller who needs "
    "this control to drop to raw Dll+Costas composition.\n"
    "\n"
    "Thin forwarder to costas_configure_lock() on the embedded Costas loop —\n"
    "symmetric with despreader_get_carrier_locked() exposing its state: "
    "state\n"
    "is readable, so config should be writable too, rather than forcing a\n"
    "caller who needs this control to drop to raw Dll+Costas composition\n"
    "instead of Despreader. See costas_configure_lock() for the parameter\n"
    "semantics.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold on the lock-metric EMA.\n"
    "down_thresh : float\n"
    "    Drop threshold (<= up_thresh for level hysteresis).\n"
    "n_up : int\n"
    "    Consecutive above-threshold symbols to declare.\n"
    "n_down : int\n"
    "    Consecutive below-threshold symbols to drop.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)\n"
    ">>> d.configure_carrier_lock(0.9, 0.8, 4, 16)  # tighter "
    "declare/drop\n" },
  { "configure_code_lock",
    (PyCFunction)(void *)DespreaderObj_configure_code_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_code_lock(pfa, n_looks, ref_snr_db) -> int\n"
    "\n"
    "Re-tune the embedded code loop's lock detector: forwards to the DLL's "
    "configure_lock (see Dll.configure_lock) -- the derived (pfa-style) entry "
    "point, matching Despreader's role as the easy composed API (Dll's raw "
    "escape hatch, configure_lock_raw, stays a Dll-only control for a caller "
    "that composes Dll+Costas directly). Raises ValueError for pfa outside "
    "(0, 1).\n"
    "\n"
    "Thin forwarder to dll_configure_lock() on the embedded DLL — the "
    "derived\n"
    "(pfa-style) entry point, matching Despreader's role as the \"easy\"\n"
    "composed API (Dll's raw escape hatch, dll_configure_lock_raw(), stays a\n"
    "Dll-only control for a caller that composes Dll+Costas directly). See\n"
    "dll_configure_lock() for the parameter semantics.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "pfa : float\n"
    "    Per-decision false-alarm probability, in (0, 1).\n"
    "n_looks : int\n"
    "    Non-coherent integration depth N (looks); clamped >= 1.\n"
    "ref_snr_db : float\n"
    "    Noise-reference estimator SNR in dB (> 0), or 0 to derive from\n"
    "    n_looks (see dll_configure_lock()).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)\n"
    ">>> d.configure_code_lock(1e-3, 20)\n"
    ">>> d.code_locked\n"
    "False\n"
    ">>> d.configure_code_lock(2.0, 20)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: configure_code_lock failed (rc=-4)\n" },
  { "reset", (PyCFunction)DespreaderObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed both loops to the create-time frequency/phase; preserve config.\n"
    "\n"
    "Restores the carrier NCO to init_norm_freq and the code phase to\n"
    "init_chip, zeroes the loop-filter accumulators and the bit-sync\n"
    "histogram, and clears the lock detectors — the spreading code and every\n"
    "configured bandwidth are preserved. Use it to re-run the same "
    "despreader\n"
    "over an independent stream and get a fresh instance's result.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import Despreader\n"
    ">>> rng = np.random.default_rng(3)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> chips = np.where(code & 1, -1.0, 1.0)\n"
    ">>> syms = np.where(rng.integers(0, 2, 40) == 1, -1.0, 1.0)\n"
    ">>> rx = np.concatenate(\n"
    "...     [s * np.repeat(chips, 4) for s in syms]).astype(np.complex64)\n"
    ">>> d = Despreader(code=code, sps=4)\n"
    ">>> first = d.bits(rx)\n"
    ">>> d.reset()                          # re-seed to acquisition\n"
    ">>> np.array_equal(first, d.bits(rx))  # same result as a fresh object\n"
    "True\n" },
  { "state_bytes", (PyCFunction)DespreaderObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DespreaderObj has already been "
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DespreaderObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DespreaderObj has already been "
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DespreaderObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DespreaderObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DespreaderObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)DespreaderObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Despreader be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Despreader\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DespreaderObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Despreader.\n"
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

static PyTypeObject DespreaderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "dsss.Despreader",
  .tp_basicsize                           = sizeof (DespreaderObject),
  .tp_dealloc                             = (destructor)DespreaderObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a despreader (COPIES code).\n",
  .tp_methods = DespreaderObj_methods,
  .tp_getset  = Despreader_getset,
  .tp_new     = DespreaderObj_new,
  .tp_init    = (initproc)DespreaderObj_init,
};
