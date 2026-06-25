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
  static char *kwlist[]
      = { "code",       "sf",      "sps", "init_norm_freq", "init_chip_phase",
          "bn_carrier", "bn_code", NULL };
  PyObject          *code_obj        = NULL;
  unsigned long long sf_raw          = 1;
  unsigned long long sps_raw         = 2;
  double             init_norm_freq  = 0.0;
  double             init_chip_phase = 0.0;
  double             bn_carrier      = 0.05;
  double             bn_code         = 0.01;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KKdddd", kwlist, &code_obj,
                                    &sf_raw, &sps_raw, &init_norm_freq,
                                    &init_chip_phase, &bn_carrier, &bn_code))
    return -1;
  size_t         sf       = (size_t)sf_raw;
  size_t         sps      = (size_t)sps_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle = despreader_create ((const uint8_t *)PyArray_DATA (code_arr),
                                    code_len, sf, sps, init_norm_freq,
                                    init_chip_phase, bn_carrier, bn_code);
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
DespreaderObj_steps (DespreaderObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
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
DespreaderObj_bits (DespreaderObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
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
DespreaderObj_set_acq (DespreaderObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]    = { "acq_code", "acq_reps", NULL };
  PyObject          *acq_code_obj = NULL;
  unsigned long long acq_reps_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OK", _kwlist, &acq_code_obj,
                                    &acq_reps_raw))
    return NULL;
  size_t         acq_reps     = (size_t)acq_reps_raw;
  PyArrayObject *acq_code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      acq_code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!acq_code_arr)
    {
      return NULL;
    }
  const uint8_t *acq_code     = (const uint8_t *)PyArray_DATA (acq_code_arr);
  size_t         acq_code_len = (size_t)PyArray_SIZE (acq_code_arr);
  despreader_set_acq (self->handle, acq_code, acq_code_len, acq_reps);
  Py_DECREF (acq_code_arr);
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
Despreader_getprop_snr_est (DespreaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (despreader_get_snr_est (self->handle));
}

static PyGetSetDef Despreader_getset[] = {
  { "bn_carrier", (getter)Despreader_getprop_bn_carrier,
    (setter)Despreader_setprop_bn_carrier,
    "Carrier (Costas) loop noise bandwidth, normalized to the symbol rate.\n",
    NULL },
  { "bn_code", (getter)Despreader_getprop_bn_code,
    (setter)Despreader_setprop_bn_code,
    "Code (DLL) loop noise bandwidth, normalized to the symbol rate.\n",
    NULL },
  { "norm_freq", (getter)Despreader_getprop_norm_freq,
    (setter)Despreader_setprop_norm_freq,
    "Current carrier frequency estimate, cycles/sample.\n", NULL },
  { "code_phase", (getter)Despreader_getprop_code_phase, NULL,
    "Current tracked code phase within the symbol, chips.\n", NULL },
  { "lock_metric", (getter)Despreader_getprop_lock_metric, NULL,
    "Lock indicator in [0,1] (EMA of |Re prompt|/|prompt|; ~1 = locked).\n",
    NULL },
  { "snr_est", (getter)Despreader_getprop_snr_est, NULL,
    "Post-despread SNR estimate (EMA of (Re prompt)^2 / (Im prompt)^2).\n",
    NULL },
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

  { "steps", (PyCFunction)DespreaderObj_steps, METH_VARARGS,
    "steps(x) -> ndarray\n"
    "\n"
    "Despread a cf32 block; emit one complex prompt symbol per code period.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, "
    "0.05, 0.01)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "bits", (PyCFunction)DespreaderObj_bits, METH_VARARGS,
    "bits(x) -> ndarray\n"
    "\n"
    "Despread a cf32 block; emit one hard BPSK bit per code period.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, "
    "0.05, 0.01)\n"
    "    >>> y = obj.bits(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "set_acq", (PyCFunction)(void *)DespreaderObj_set_acq,
    METH_VARARGS | METH_KEYWORDS,
    "set_acq(acq_code, acq_reps) -> None\n"
    "\n"
    "Enable preamble-aided pull-in: track acq_reps periods of the (distinct) "
    "acq_code coherently before despreading the payload with the data code. "
    "Call before feeding the burst; clears when the preamble is consumed.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, "
    "0.05, 0.01)\n"
    "    >>> obj.set_acq(np.zeros(4, dtype=np.uint8), 0)\n" },
  { "reset", (PyCFunction)DespreaderObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loops to the create-time phase/frequency; preserve config.\n"
    "\n"
    "    >>> from doppler import Despreader\n"
    "    >>> obj = Despreader(np.zeros(1, dtype=np.uint8), 1, 2, 0.0, 0.0, "
    "0.05, 0.01)\n"
    "    >>> obj.reset()\n" },
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
  .tp_doc                                 = "Despreader type.\n",
  .tp_methods                             = DespreaderObj_methods,
  .tp_getset                              = Despreader_getset,
  .tp_new                                 = DespreaderObj_new,
  .tp_init                                = (initproc)DespreaderObj_init,
};
