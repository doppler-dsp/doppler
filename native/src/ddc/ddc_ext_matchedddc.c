/*
 * ddc_ext_matchedddc.c — MatchedDDC type for the ddc module.
 *
 * Included by ddc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ddc_ext.c is compiled.
 */
/* ======================================================== */
/* MatchedDDCObject — wraps ddc_state_t *       */
/* ======================================================== */

#include "ddc/ddc_core.h"

typedef struct
{
  PyObject_HEAD ddc_state_t *handle;
} MatchedDDCObject;

static void
MatchedDDCObj_dealloc (MatchedDDCObject *self)
{
  if (self->handle)
    ddc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MatchedDDCObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MatchedDDCObject *self = (MatchedDDCObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MatchedDDCObj_init (MatchedDDCObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "norm_freq", "rate",      "pulse",      "beta",
                             "span",      "pulse_sps", "num_phases", NULL };
  double       norm_freq = 0.0;
  double       rate      = 0.25;
  const char  *pulse_str = "rrc";
  double       beta      = 0.35;
  unsigned long long span_raw       = 8;
  double             pulse_sps      = 2.0;
  unsigned long long num_phases_raw = 1024;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddsdKdK", kwlist, &norm_freq,
                                    &rate, &pulse_str, &beta, &span_raw,
                                    &pulse_sps, &num_phases_raw))
    return -1;
  int pulse = 0;
  if (strcmp (pulse_str, "iandd") == 0)
    pulse = 0;
  else if (strcmp (pulse_str, "rrc") == 0)
    pulse = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "pulse must be one of \"iandd\", \"rrc\", got '%s'",
                    pulse_str);
      return -1;
    }
  size_t span       = (size_t)span_raw;
  size_t num_phases = (size_t)num_phases_raw;
  self->handle      = ddc_create_matched (norm_freq, rate, pulse, beta, span,
                                          pulse_sps, num_phases);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "DDC: invalid parameter (need rate > 0, 0 <= beta <= "
                       "1, span >= 1, pulse_sps > 0, num_phases a power of "
                       "two >= 2)");
      return -1;
    }
  if (self->handle->narrow_pulse)
    {
      if (PyErr_WarnEx (PyExc_UserWarning,
                        "pulse=\"iandd\" with pulse_sps < 4: the rectangle "
                        "is one symbol wide, so its matched filter is a 2-3 "
                        "tap sum here and barely opens the eye (measured on "
                        "the timing loop this feeds: lock statistic -0.34 at "
                        "2 samples/symbol against +0.95 at 4). Use pulse_sps "
                        ">= 4, or pulse=\"rrc\".",
                        1)
          < 0)
        return -1;
    }
  return 0;
}

static PyObject *
MatchedDDCObj_execute_max_out (MatchedDDCObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t x_len = 0;
  if (!PyArg_ParseTuple (args, "n", &x_len))
    return NULL;
  return PyLong_FromSize_t (ddc_execute_max_out (self->handle, (size_t)x_len));
}

static PyObject *
MatchedDDCObj_execute (MatchedDDCObject *self, PyObject *args, PyObject *kwds)
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
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      size_t _omax
          = ddc_execute_max_out (self->handle, (size_t)PyArray_SIZE (x_arr));
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
        n_out = ddc_execute (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap
      = ddc_execute_max_out (self->handle, (size_t)PyArray_SIZE (x_arr));
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
    n_out = ddc_execute (self->handle, _ng0, _ng1, _d0, _cap);
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
MatchedDDCObj_execute_ctrl (MatchedDDCObject *self, PyObject *args,
                            PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "rate_ctrl", "freq_ctrl", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  double         rate_ctrl = 0;
  double         freq_ctrl = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Odd", _kwlist, &x_obj,
                                    &rate_ctrl, &freq_ctrl))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap
      = ddc_execute_ctrl_max_out (self->handle, (size_t)PyArray_SIZE (x_arr));
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
    n_out = ddc_execute_ctrl (self->handle, _ng0, _ng1, rate_ctrl, freq_ctrl,
                              _d0, _cap);
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
MatchedDDCObj_execute_ctrl_push (MatchedDDCObject *self, PyObject *args,
                                 PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "rate_ctrl", "freq_ctrl", NULL };
  Py_complex   x_raw     = { 0.0, 0.0 };
  double       rate_ctrl = 0;
  double       freq_ctrl = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Ddd", _kwlist, &x_raw,
                                    &rate_ctrl, &freq_ctrl))
    return NULL;
  float complex x     = (float)x_raw.real + (float)x_raw.imag * I;
  size_t        _need = ddc_execute_ctrl_push_max_out (self->handle);
  size_t        _cap  = ddc_execute_ctrl_push_max_out (self->handle);
  (void)_need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = ddc_execute_ctrl_push (self->handle, x, rate_ctrl, freq_ctrl,
                                        _d0, _cap);
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
MatchedDDCObj_reset (MatchedDDCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ddc_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
MatchedDDCObj_state_bytes (MatchedDDCObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ddc_state_bytes (self->handle));
}

static PyObject *
MatchedDDCObj_get_state (MatchedDDCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = ddc_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  ddc_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
MatchedDDCObj_set_state (MatchedDDCObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != ddc_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (ddc_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MatchedDDC_getprop_norm_freq (MatchedDDCObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddc_get_norm_freq (self->handle));
}
static int
MatchedDDC_setprop_norm_freq (MatchedDDCObject *self, PyObject *value,
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
  ddc_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
MatchedDDC_getprop_rate (MatchedDDCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddc_get_rate (self->handle));
}
static PyObject *
MatchedDDC_getprop_clipped (MatchedDDCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ddc_get_clipped (self->handle)));
}
static PyObject *
MatchedDDC_getprop_narrow_pulse (MatchedDDCObject *self,
                                 void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ddc_get_narrow_pulse (self->handle)));
}

static PyGetSetDef MatchedDDC_getset[] = {
  { "norm_freq", (getter)MatchedDDC_getprop_norm_freq,
    (setter)MatchedDDC_setprop_norm_freq,
    "Return the current LO normalised frequency (cycles/sample).\n", NULL },
  { "rate", (getter)MatchedDDC_getprop_rate, NULL,
    "Return the configured output/input rate ratio (read-only). The rate is "
    "fixed at create time; change it by destroying and recreating the DDC "
    "with the new value.\n",
    NULL },
  { "clipped", (getter)MatchedDDC_getprop_clipped, NULL,
    "Has the cascade's CIC clipped its input since the last reset?\n", NULL },
  { "narrow_pulse", (getter)MatchedDDC_getprop_narrow_pulse, NULL,
    "Is this object's rectangular matched filter degenerately narrow?\n",
    NULL },
  { NULL }
};

static PyObject *
MatchedDDCObj_destroy (MatchedDDCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ddc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MatchedDDCObj_enter (MatchedDDCObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MatchedDDCObj_exit (MatchedDDCObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ddc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef MatchedDDCObj_methods[] = {

  { "execute", (PyCFunction)(void *)MatchedDDCObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Mix input block with LO, then rate-convert.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    CF32 input block; accepted as float32 (auto-cast).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of output samples written (C-only).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ddc import DDC\n"
    ">>> import numpy as np\n"
    ">>> ddc = DDC(norm_freq=-0.1, rate=0.25)\n"
    ">>> t = np.arange(4096)\n"
    ">>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)\n"
    ">>> y = ddc.execute(x)\n"
    ">>> y.shape\n"
    "(1024,)\n"
    ">>> y.dtype\n"
    "dtype('complex64')\n"
    ">>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1\n"
    "1.0\n" },
  { "execute_max_out", (PyCFunction)MatchedDDCObj_execute_max_out,
    METH_VARARGS,
    "execute_max_out(x_len) -> int\n\nMax output length execute() can produce "
    "for x_len.\nUse to size the ``out=`` buffer." },
  { "execute_ctrl", (PyCFunction)(void *)MatchedDDCObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x) -> ndarray\n"
    "\n"
    "Mix and resample a block, steering both control ports.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MatchedDDC\n"
    "    >>> obj = MatchedDDC(norm_freq=0.0, rate=0.25, pulse=\"rrc\", "
    "beta=0.35, span=8, pulse_sps=2.0, num_phases=1024)\n"
    "    >>> y = obj.execute_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ctrl_push", (PyCFunction)(void *)MatchedDDCObj_execute_ctrl_push,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl_push(n=1) -> ndarray\n"
    "\n"
    "Push ONE input sample; emit whatever outputs it completes.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import MatchedDDC\n"
    "    >>> obj = MatchedDDC(norm_freq=0.0, rate=0.25, pulse=\"rrc\", "
    "beta=0.35, span=8, pulse_sps=2.0, num_phases=1024)\n"
    "    >>> y = obj.execute_ctrl_push(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)MatchedDDCObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero LO phase and filter history.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ddc import DDC\n"
    ">>> import numpy as np\n"
    ">>> ddc = DDC(norm_freq=0.0, rate=0.25)\n"
    ">>> x = np.ones(64, dtype=np.complex64)\n"
    ">>> y1 = ddc.execute(x)\n"
    ">>> ddc.reset()\n"
    ">>> y2 = ddc.execute(x)\n"
    ">>> bool(np.array_equal(y1, y2))\n"
    "True\n" },
  { "state_bytes", (PyCFunction)MatchedDDCObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the MatchedDDC has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)MatchedDDCObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the MatchedDDC has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)MatchedDDCObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the MatchedDDC has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)MatchedDDCObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)MatchedDDCObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DDC be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DDC\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MatchedDDCObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DDC.\n"
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
  { NULL }
};

static PyTypeObject MatchedDDCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ddc.MatchedDDC",
  .tp_basicsize                           = sizeof (MatchedDDCObject),
  .tp_dealloc                             = (destructor)MatchedDDCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "MatchedDDC type.\n",
  .tp_methods                             = MatchedDDCObj_methods,
  .tp_getset                              = MatchedDDC_getset,
  .tp_new                                 = MatchedDDCObj_new,
  .tp_init                                = (initproc)MatchedDDCObj_init,
};
