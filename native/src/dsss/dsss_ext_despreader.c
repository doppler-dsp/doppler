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
  float complex *_steps_buf;     /* pre-allocated output for steps */
  size_t         _steps_buf_cap; /* allocated capacity for steps */
  void         **_steps_retired; /* gh-219 deferred free */
  size_t         _steps_retired_n;
  size_t         _steps_retired_cap;
  uint8_t       *_bits_buf;     /* pre-allocated output for bits */
  size_t         _bits_buf_cap; /* allocated capacity for bits */
  void         **_bits_retired; /* gh-219 deferred free */
  size_t         _bits_retired_n;
  size_t         _bits_retired_cap;
} DespreaderObject;

static void
DespreaderObj_dealloc (DespreaderObject *self)
{
  if (self->handle)
    despreader_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  free (self->_bits_buf);
  for (size_t _i = 0; _i < self->_bits_retired_n; _i++)
    free (self->_bits_retired[_i]);
  free (self->_bits_retired);
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
  {
    size_t _max = despreader_steps_max_out (self->handle);
    if (_max)
      {
        self->_steps_buf = malloc (_max * sizeof (float complex));
        if (!self->_steps_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_steps_buf_cap = _max;
      }
  }
  {
    size_t _max = despreader_bits_max_out (self->handle);
    if (_max)
      {
        self->_bits_buf = malloc (_max * sizeof (uint8_t));
        if (!self->_bits_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_bits_buf_cap = _max;
      }
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
  if (!self->_steps_buf || self->_steps_buf_cap < _need)
    {
      size_t _max = despreader_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_steps_buf
          && self->_steps_retired_n == self->_steps_retired_cap)
        {
          size_t _rcap
              = self->_steps_retired_cap ? self->_steps_retired_cap * 2 : 4;
          void **_rt = realloc (self->_steps_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_steps_retired     = _rt;
          self->_steps_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_steps_buf)
        self->_steps_retired[self->_steps_retired_n++] = self->_steps_buf;
      self->_steps_buf     = _tmp;
      self->_steps_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = despreader_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                              self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
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
  if (!self->_bits_buf || self->_bits_buf_cap < _need)
    {
      size_t _max = despreader_bits_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_bits_buf && self->_bits_retired_n == self->_bits_retired_cap)
        {
          size_t _rcap
              = self->_bits_retired_cap ? self->_bits_retired_cap * 2 : 4;
          void **_rt = realloc (self->_bits_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_bits_retired     = _rt;
          self->_bits_retired_cap = _rcap;
        }
      uint8_t *_tmp = malloc (_max * sizeof (uint8_t));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_bits_buf)
        self->_bits_retired[self->_bits_retired_n++] = self->_bits_buf;
      self->_bits_buf     = _tmp;
      self->_bits_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = despreader_bits (self->handle, _ng0, _ng1, self->_bits_buf,
                             self->_bits_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_bits_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
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

static PyGetSetDef Despreader_getset[]
    = { { "norm_freq", (getter)Despreader_getprop_norm_freq,
          (setter)Despreader_setprop_norm_freq, "Norm freq.\n", NULL },
        { "code_phase", (getter)Despreader_getprop_code_phase, NULL,
          "Code phase.\n", NULL },
        { "code_rate", (getter)Despreader_getprop_code_rate, NULL,
          "Code rate.\n", NULL },
        { "lock_metric", (getter)Despreader_getprop_lock_metric, NULL,
          "Lock metric.\n", NULL },
        { "bit_phase", (getter)Despreader_getprop_bit_phase, NULL,
          "Bit phase.\n", NULL },
        { "bn_carrier", (getter)Despreader_getprop_bn_carrier,
          (setter)Despreader_setprop_bn_carrier, "Bn carrier.\n", NULL },
        { "bn_code", (getter)Despreader_getprop_bn_code,
          (setter)Despreader_setprop_bn_code, "Bn code.\n", NULL },
        { NULL } };

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

  { "steps", (PyCFunction)DespreaderObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Track carrier + code and despread a cf32 block: per sample wipe the "
    "carrier (Costas) and correlate early/prompt/late against the code (DLL), "
    "update both loops each code period, and emit one complex prompt symbol "
    "per period.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)DespreaderObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "bits", (PyCFunction)DespreaderObj_bits, METH_VARARGS | METH_KEYWORDS,
    "bits(x) -> ndarray\n"
    "\n"
    "Same tracking kernel as steps(), but bit-sync the per-period prompts "
    "into hard data bits: periods_per_bit prompts are coherently summed "
    "across each detected bit boundary and one 0/1 bit is emitted per data "
    "bit.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> y = obj.bits(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "bits_max_out", (PyCFunction)DespreaderObj_bits_max_out, METH_NOARGS,
    "bits_max_out() -> int\n\nMax output length bits() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)DespreaderObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context across the despreader. Pure "
    "forwarder — the despreader registers no probes of its own: the carrier "
    "loop registers \"<prefix>.car.lock\" / \".e\" / \".freq\" and the code "
    "loop registers \"<prefix>.code.e\" / \".rate\" / \".lock\" — six probes, "
    "all thinned by decim and emitted once per code period (the despreader "
    "flushes both loops at its per-period update).  Passing NULL detaches "
    "both loops.  Setup path, never hot; the context is borrowed and must "
    "outlive the attachment (SPSC rules in telemetry/telemetry.h).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "reset", (PyCFunction)DespreaderObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed both loops to the create-time frequency/phase; preserve config.\n"
    "\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 4, 0.0, 0.0, 0.05, "
    "0.005, 0.0, 0.707, 0.5, 1)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)DespreaderObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DespreaderObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DespreaderObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)DespreaderObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DespreaderObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DespreaderObj_exit, METH_VARARGS, NULL },
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
