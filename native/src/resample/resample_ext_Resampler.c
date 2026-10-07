/* jm:generated resample_ext_Resampler.c */
/*
 * resample_ext_Resampler.c — Resampler type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in resample_ext_Resampler_extra.c.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* ResamplerObject — wraps dp_Resampler_state_t *       */
/* ======================================================== */

#include "doppler/Resampler/Resampler_core.h"

typedef struct
{
  PyObject_HEAD dp_Resampler_state_t *handle;
} ResamplerObject;

static void
ResamplerObj_dealloc (ResamplerObject *self)
{
  if (self->handle)
    dp_Resampler_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ResamplerObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  ResamplerObject *self = (ResamplerObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ResamplerObj_init (ResamplerObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "rate", "bank", NULL };
  double       rate     = 0.0;
  PyObject    *bank_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dO", kwlist, &rate,
                                    &bank_obj))
    return -1;
  if (bank_obj && bank_obj != Py_None)
    {
      PyArrayObject *bank_arr
          = jm_array_arg (bank_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS, "bank");
      if (!bank_arr)
        {
          return -1;
        }
      if (PyArray_NDIM (bank_arr) != 2)
        {
          PyErr_SetString (PyExc_ValueError, "bank must be a 2-D array");
          Py_DECREF (bank_arr);
          return -1;
        }
      size_t bank_dim0 = (size_t)PyArray_DIM (bank_arr, 0);
      size_t bank_dim1 = (size_t)PyArray_DIM (bank_arr, 1);
      self->handle     = dp_Resampler_create_custom (
          bank_dim0, bank_dim1, (const float *)PyArray_DATA (bank_arr), rate);
      Py_DECREF (bank_arr);
    }
  else
    {
      self->handle = dp_Resampler_create (rate);
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_Resampler_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
ResamplerObj_execute_max_out (ResamplerObject *self,
                              PyObject        *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_Resampler_execute_max_out (self->handle));
}

static PyObject *
ResamplerObj_execute (ResamplerObject *self, PyObject *args, PyObject *kwds)
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
  x_arr = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
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
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_Resampler_execute_max_out (self->handle);
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
      size_t n_out = dp_Resampler_execute (
          self->handle, (const float _Complex *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr),
          (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Resampler.execute: wrote %zu elements into a buffer of %zu",
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_Resampler_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Resampler.execute: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t          n_out = dp_Resampler_execute (
      self->handle, (const float _Complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), _d0, _cap);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Resampler.execute: wrote %zu elements into a buffer of %zu",
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
ResamplerObj_execute_ctrl_max_out (ResamplerObject *self,
                                   PyObject        *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_Resampler_execute_ctrl_max_out (self->handle));
}

static PyObject *
ResamplerObj_execute_ctrl (ResamplerObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "ctrl", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *ctrl_obj  = NULL;
  PyArrayObject *ctrl_arr  = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO|O", _kwlist, &x_obj,
                                    &ctrl_obj, &out_obj))
    return NULL;
  x_arr = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    return NULL;
  ctrl_arr
      = jm_array_arg (ctrl_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS, "ctrl");
  if (!ctrl_arr)
    {
      Py_DECREF (x_arr);
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
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_Resampler_execute_ctrl_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      int64_t _rc = dp_Resampler_execute_ctrl (
          self->handle, (const float _Complex *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr),
          (const double *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr),
          (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (x_arr);
      Py_DECREF (ctrl_arr);
      if (_rc < 0)
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                        "ctrl must be at least as long as x", (long long)_rc);
          return NULL;
        }
      size_t n_out = (size_t)_rc;
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (PyExc_RuntimeError,
                        "Resampler.execute_ctrl: wrote %zu elements into a "
                        "buffer of %zu",
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
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_Resampler_execute_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      Py_DECREF (ctrl_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "Resampler.execute_ctrl: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  int64_t         _rc = dp_Resampler_execute_ctrl (
      self->handle, (const float _Complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), (const double *)PyArray_DATA (ctrl_arr),
      (size_t)PyArray_SIZE (ctrl_arr), _d0, _cap);
  Py_DECREF (x_arr);
  Py_DECREF (ctrl_arr);
  if (_rc < 0)
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "ctrl must be at least as long as x", (long long)_rc);
      return NULL;
    }
  size_t n_out = (size_t)_rc;
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "Resampler.execute_ctrl: wrote %zu elements into a buffer of %zu",
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
ResamplerObj_reset (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_Resampler_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ResamplerObj_state_bytes (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_Resampler_state_bytes (self->handle));
}

static PyObject *
ResamplerObj_get_state (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_Resampler_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_Resampler_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ResamplerObj_set_state (ResamplerObject *self, PyObject *arg)
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
      != dp_Resampler_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_Resampler_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Resampler_getprop_rate (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dp_Resampler_get_rate (self->handle));
}
static int
Resampler_setprop_rate (ResamplerObject *self, PyObject *value,
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
  dp_Resampler_set_rate (self->handle, v);
  return 0;
}
static PyObject *
Resampler_getprop_ctrl_acc (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dp_Resampler_get_ctrl_acc (self->handle));
}
static PyObject *
Resampler_getprop_num_phases (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_Resampler_get_num_phases (self->handle));
}
static PyObject *
Resampler_getprop_num_taps (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dp_Resampler_get_num_taps (self->handle));
}
static PyObject *
Resampler_getprop_delay (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dp_Resampler_get_delay (self->handle));
}

static PyGetSetDef Resampler_getset[] = {
  { "rate", (getter)Resampler_getprop_rate, (setter)Resampler_setprop_rate,
    "Get / set the output-to-input sample rate ratio. The setter recomputes "
    "the phase increment immediately; the delay line and phase accumulator "
    "are preserved so in-stream rate changes are glitch-free. Switching sign "
    "of (rate - 1) (i.e. crossing the boundary between interp and decim "
    "modes) requires a fresh create().\n",
    NULL },
  { "ctrl_acc", (getter)Resampler_getprop_ctrl_acc, NULL,
    "The control accumulator's fractional phase, in [0, 1).\n", NULL },
  { "num_phases", (getter)Resampler_getprop_num_phases, NULL, "Num phases.\n",
    NULL },
  { "num_taps", (getter)Resampler_getprop_num_taps, NULL,
    "Taps per polyphase branch. Total prototype filter length is num_phases * "
    "num_taps - 1. The built-in bank uses 19 taps per branch.\n",
    NULL },
  { "delay", (getter)Resampler_getprop_delay, NULL,
    "Group delay of the interpolator, in input samples.\n", NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
ResamplerObj_destroy (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_Resampler_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ResamplerObj_enter (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ResamplerObj_exit (ResamplerObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_Resampler_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ResamplerObj_methods[] = {

  { "execute", (PyCFunction)(void *)ResamplerObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray\n"
    "\n"
    "Resample a block of CF32 samples at the fixed base rate. Uses the\n"
    "dual-mode polyphase engine: output-driven for rate >= 1\n"
    "(interpolation), input-driven transposed-form for rate < 1\n"
    "(decimation). State carries over between calls, so contiguous blocks\n"
    "produce the same result as one large block.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    CF32 input samples.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer; must hold at least RESAMPLER_MAX_OUT samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 output array; length is approximately x_len * rate, capped at\n"
    "    max_out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import Resampler\n"
    ">>> import numpy as np\n"
    ">>> r = Resampler(rate=2.0)\n"
    ">>> y = r.execute(np.zeros(128, dtype=np.complex64))\n"
    ">>> y.shape, y.dtype\n"
    "((256,), dtype('complex64'))\n" },
  { "execute_max_out", (PyCFunction)ResamplerObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Always returns RESAMPLER_MAX_OUT.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_ctrl", (PyCFunction)(void *)ResamplerObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x, ctrl, out) -> ndarray\n"
    "\n"
    "Resample with per-sample additive rate deviations. Effective rate\n"
    "for sample i is base_rate + `ctrl[i]`. Uses a unified double-precision\n"
    "accumulator that handles both interpolation and decimation in a single\n"
    "code path — suitable for Doppler-shift simulation and fractional-sample\n"
    "timing correction. ctrl and x must have the same length.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    CF32 input samples.\n"
    "ctrl : npt.NDArray[np.float64]\n"
    "    Real float64 array, same length as x; the per-sample rate addend.\n"
    "    Anything numpy can safely widen to float64 is accepted (float32, a\n"
    "    plain list); a complex array is refused rather than truncated.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer; must hold at least RESAMPLER_MAX_OUT samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 output array; length depends on accumulated rate deviations,\n"
    "    capped at max_out. In C, the count written (>= 0), or\n"
    "    DP_ERR_INVALID (negative) when ctrl_len is shorter than x_len:\n"
    "    nothing is written. 0 stays a valid, empty result, so the sign\n"
    "    alone tells a refusal from it (doppler#1869).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a negative value. The exception message is\n"
    "    ``ctrl must be at least as long as x``, with the return code\n"
    "    appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import Resampler\n"
    ">>> import numpy as np\n"
    ">>> r = Resampler(rate=1.0)\n"
    ">>> x = np.zeros(64, dtype=np.complex64)\n"
    ">>> ctrl = np.zeros(64)\n"
    ">>> y = r.execute_ctrl(x, ctrl)\n"
    ">>> y.shape, y.dtype\n"
    "((64,), dtype('complex64'))\n" },
  { "execute_ctrl_max_out", (PyCFunction)ResamplerObj_execute_ctrl_max_out,
    METH_NOARGS,
    "execute_ctrl_max_out() -> int\n"
    "\n"
    "Always returns RESAMPLER_MAX_OUT.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "reset", (PyCFunction)ResamplerObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero the delay line and phase accumulator. Rate and polyphase bank\n"
    "are preserved so the resampler can be resumed at the same ratio.\n"
    "Zeroing state eliminates transient artefacts when starting a new signal\n"
    "burst.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import Resampler\n"
    ">>> import numpy as np\n"
    ">>> r = Resampler(rate=2.0)\n"
    ">>> _ = r.execute(np.ones(64, dtype=np.complex64))\n"
    ">>> r.reset()\n"
    ">>> r.rate\n"
    "2.0\n" },
  { "state_bytes", (PyCFunction)ResamplerObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Resampler has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)ResamplerObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the Resampler has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)ResamplerObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Resampler has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)ResamplerObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)ResamplerObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Resampler be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Resampler\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ResamplerObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Resampler.\n"
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

static PyTypeObject ResamplerObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.resample.Resampler",
  .tp_basicsize                           = sizeof (ResamplerObject),
  .tp_dealloc                             = (destructor)ResamplerObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a Resampler with the built-in 4096×19 Kaiser bank. The bank\n"
    "provides ~60 dB alias rejection with 0.4/0.6 pass/stop normalised "
    "cutoffs.\n"
    "Pass rate >= 1.0 to interpolate (upsample); pass rate < 1.0 to decimate\n"
    "(downsample). For a custom bank use dp_Resampler_create_custom() "
    "instead.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rate : float, default 0.0\n"
    "    Output-to-input sample rate ratio (any positive float). Values >= "
    "1.0\n"
    "    interpolate; values < 1.0 decimate.\n"
    "bank : NDArray[np.float32] or None\n"
    "    Optional polyphase bank: a 2-D float32 array of shape (num_phases,\n"
    "    num_taps), num_phases a power of two. Omit for the built-in 4096x19\n"
    "    Kaiser bank.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import Resampler\n"
    ">>> import numpy as np\n"
    ">>> r = Resampler(rate=2.0)\n"
    ">>> r.num_phases, r.num_taps\n"
    "(4096, 19)\n"
    ">>> r.rate\n"
    "2.0\n",
  .tp_methods = ResamplerObj_methods,
  .tp_getset  = Resampler_getset,
  .tp_new     = ResamplerObj_new,
  .tp_init    = (initproc)ResamplerObj_init,
};
