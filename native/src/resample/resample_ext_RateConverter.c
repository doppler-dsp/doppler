/*
 * resample_ext_RateConverter.c — RateConverter type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* RateConverterObject — wraps RateConverter_state_t *       */
/* ======================================================== */

#include "RateConverter/RateConverter_core.h"

typedef struct
{
  PyObject_HEAD RateConverter_state_t *handle;
} RateConverterObject;

static void
RateConverterObj_dealloc (RateConverterObject *self)
{
  if (self->handle)
    RateConverter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RateConverterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RateConverterObject *self = (RateConverterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
RateConverterObj_init (RateConverterObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char *kwlist[]   = { "rate", "compensate", NULL };
  double       rate       = 1.0;
  int          compensate = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|di", kwlist, &rate,
                                    &compensate))
    return -1;
  self->handle = RateConverter_create (rate, compensate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "RateConverter: invalid parameter (need rate > 0, 0 "
                       "<= beta <= 1, span >= 1, pulse_sps > 0, num_phases a "
                       "power of two >= 2)");
      return -1;
    }
  return 0;
}

static PyObject *
RateConverterObj_execute_max_out (RateConverterObject *self,
                                  PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (RateConverter_execute_max_out (self->handle));
}

static PyObject *
RateConverterObj_execute (RateConverterObject *self, PyObject *args,
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
      size_t _omax    = RateConverter_execute_max_out (self->handle);
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
      size_t n_out = RateConverter_execute (
          self->handle, (const float complex *)PyArray_DATA (x_arr),
          (size_t)PyArray_SIZE (x_arr),
          (float complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = RateConverter_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = RateConverter_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), _d0, _cap);
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
RateConverterObj_execute_ctrl (RateConverterObject *self, PyObject *args,
                               PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "ctrl", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  double         ctrl      = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Od", _kwlist, &x_obj, &ctrl))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = RateConverter_execute_ctrl_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = RateConverter_execute_ctrl (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), ctrl, _d0, _cap);
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
RateConverterObj_execute_ctrl_push_max_out (RateConverterObject *self,
                                            PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (
      RateConverter_execute_ctrl_push_max_out (self->handle));
}

static PyObject *
RateConverterObj_execute_ctrl_push (RateConverterObject *self, PyObject *args,
                                    PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "ctrl", "out", NULL };
  Py_complex   x_raw     = { 0.0, 0.0 };
  double       ctrl      = 0;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Dd|O", _kwlist, &x_raw, &ctrl,
                                    &out_obj))
    return NULL;
  float complex x = (float)x_raw.real + (float)x_raw.imag * I;
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
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap  = (size_t)PyArray_SIZE (out_arr);
      size_t _omax = RateConverter_execute_ctrl_push_max_out (self->handle);
      size_t _min_cap
          = _omax > RateConverter_execute_ctrl_push_max_out (self->handle)
                ? _omax
                : (RateConverter_execute_ctrl_push_max_out (self->handle));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = RateConverter_execute_ctrl_push (
          self->handle, x, ctrl, (float complex *)PyArray_DATA (out_arr),
          _cap);
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
  size_t _need = RateConverter_execute_ctrl_push_max_out (self->handle);
  size_t _cap  = RateConverter_execute_ctrl_push_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out
      = RateConverter_execute_ctrl_push (self->handle, x, ctrl, _d0, _cap);
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
RateConverterObj_reset (RateConverterObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  RateConverter_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
RateConverterObj_state_bytes (RateConverterObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (RateConverter_state_bytes (self->handle));
}

static PyObject *
RateConverterObj_get_state (RateConverterObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  RateConverter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
RateConverterObj_set_state (RateConverterObject *self, PyObject *arg)
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
      != RateConverter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (RateConverter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
RateConverter_getprop_rate (RateConverterObject *self,
                            void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (RateConverter_get_rate (self->handle));
}
static int
RateConverter_setprop_rate (RateConverterObject *self, PyObject *value,
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
  RateConverter_set_rate (self->handle, v);
  return 0;
}
static PyObject *
RateConverter_getprop_clipped (RateConverterObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(RateConverter_get_clipped (self->handle)));
}
static PyObject *
RateConverter_getprop_narrow_pulse (RateConverterObject *self,
                                    void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong (
      (long)(RateConverter_get_narrow_pulse (self->handle)));
}
static PyObject *
RateConverter_getprop_stages (RateConverterObject *self,
                              void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_num_stages (self->handle);
  PyObject *_c = PyList_New ((Py_ssize_t)_n);
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      const char *_r = RateConverter_stages_value (self->handle, _i);
      if (!_r)
        {
          PyErr_Format (
              PyExc_RuntimeError,
              "stages: RateConverter_stages_value returned NULL at index %zu",
              _i);
          Py_DECREF (_c);
          return NULL;
        }
      PyObject *_v = PyUnicode_FromString (_r);
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      PyList_SET_ITEM (_c, (Py_ssize_t)_i, _v);
    }
  return _c;
}
static PyObject *
RateConverter_getprop_bank_shape (RateConverterObject *self,
                                  void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = RateConverter_num_bank_shape (self->handle);
  PyObject *_c = PyList_New ((Py_ssize_t)_n);
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      PyObject *_v = PyLong_FromUnsignedLongLong (
          (unsigned long long)RateConverter_bank_shape_value (self->handle,
                                                              _i));
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      PyList_SET_ITEM (_c, (Py_ssize_t)_i, _v);
    }
  return _c;
}

static PyGetSetDef RateConverter_getset[] = {
  { "rate", (getter)RateConverter_getprop_rate,
    (setter)RateConverter_setprop_rate,
    "Get / set the output-to-input sample rate ratio. The setter rebuilds the "
    "entire cascade (new stage selection, new sub-objects) and resets all "
    "filter memories — equivalent to destroying and recreating with the new "
    "rate. Setting rate <= 0 is silently ignored.\n",
    NULL },
  { "clipped", (getter)RateConverter_getprop_clipped, NULL,
    "True if any planned CIC stage has clipped its input since the last "
    "`reset()`. The cascade inherits the CIC's input bound (`|Re|`, `|Im| <= "
    "1.0`) whenever `stages` names a CIC -- any decimation by 8 or more. The "
    "clip is invisible in the samples (finite, no NaN, merely distorted), so "
    "this is the only reliable check, and it is free: the boundary "
    "comparisons run on every sample regardless. Always False for a cascade "
    "with no CIC stage -- those plans are scale-free.\n",
    NULL },
  { "narrow_pulse", (getter)RateConverter_getprop_narrow_pulse, NULL,
    "True when a rectangular pulse was selected with fewer than four output "
    "samples per symbol, where its matched filter degenerates to a 2-3 tap "
    "sum. Construction also raises a UserWarning; this is the same diagnostic "
    "to pull rather than catch. Always False for `pulse=\"rrc\"` and for a "
    "plain converter.\n",
    NULL },
  { "stages", (getter)RateConverter_getprop_stages, NULL,
    "Stage labels for the planned cascade, e.g. `['CIC(8)', "
    "'Resampler(0.8)']`. A terminal stage carrying a pulse-shaped bank names "
    "its pulse: `'Resampler(0.923077,rrc)'`.\n",
    NULL },
  { "bank_shape", (getter)RateConverter_getprop_bank_shape, NULL,
    "`[num_phases, num_taps]` of the terminal polyphase stage, or `[]` when "
    "the cascade ends in an integer decimator and so has no bank to describe. "
    "`num_taps` is the per-output MAC count and, times `num_phases`, the "
    "bank's size in floats. With a pulse selected it is set by the terminal "
    "stage's rate rather than the input rate -- which is what keeps a matched "
    "filter affordable at a high input samples-per-symbol: the same 34 taps "
    "per arm at 4 samples/symbol and at 256, where filtering at the input "
    "rate would need 4225.\n",
    NULL },
  { NULL }
};

static PyObject *
RateConverterObj_destroy (RateConverterObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RateConverterObj_enter (RateConverterObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
RateConverterObj_exit (RateConverterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      RateConverter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef RateConverterObj_methods[] = {

  { "execute", (PyCFunction)(void *)RateConverterObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray\n"
    "\n"
    "Convert a block of CF32 samples through the cascade. Passes input\n"
    "through each stage in order, ping-ponging between two intermediate\n"
    "buffers. State persists between calls, so contiguous calls on\n"
    "sequential blocks give the same result as one large call. Output length\n"
    "is approximately n_in * rate.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input.\n"
    "out : NDArray[np.complex64] | None\n"
    "    Output buffer; must hold at least max_out samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 output array; length is approximately n_in * rate.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import RateConverter\n"
    ">>> import numpy as np\n"
    ">>> rc = RateConverter(rate=0.5, compensate=0)\n"
    ">>> y = rc.execute(np.zeros(1024, dtype=np.complex64))\n"
    ">>> y.shape, y.dtype\n"
    "((512,), dtype('complex64'))\n" },
  { "execute_max_out", (PyCFunction)RateConverterObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Upper bound on execute output for a standard 65536-sample block.\n"
    "\n"
    "Returns (size_t)(65536 * max(rate, 1.0)) + 2. The Python extension uses\n"
    "this to pre-allocate the output buffer on the first execute call.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "execute_ctrl", (PyCFunction)(void *)RateConverterObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x, ctrl) -> ndarray\n"
    "\n"
    "Convert a block, steering the cascade's fractional stage by ctrl.\n"
    "\n"
    "The control-port form of RateConverter_execute(): the fixed integer\n"
    "stages (HalfbandDecimator / CIC) run unchanged, and the scalar rate\n"
    "deviation ctrl is forwarded to the **terminal polyphase Resampler\n"
    "stage's** accumulator (via resamp_execute_ctrl_push) — so its effective\n"
    "rate becomes `stage_rate + ctrl` for this call. This exposes the\n"
    "fractional tail's control port that RateConverter_execute() hides: a\n"
    "timing/rate-tracking loop can decimate a high input rate cheaply\n"
    "through the HB/CIC stages and then arbitrary-rate + strobe-align in the\n"
    "last stage, updating ctrl per block.\n"
    "\n"
    "`ctrl` is referenced to the terminal stage's (post-decimation) rate,\n"
    "not the overall rate. It is meaningful only when the cascade actually\n"
    "ends in a Resampler stage; a pure integer HB/CIC cascade has no\n"
    "fractional stage to steer, so this **falls through to\n"
    "RateConverter_execute()** (ctrl ignored).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    CF32 input block.\n"
    "ctrl : float\n"
    "    Rate deviation added to the terminal Resampler stage's rate.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 output array; length tracks the accumulated effective rate.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import RateConverter\n"
    ">>> import numpy as np\n"
    ">>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)\n"
    ">>> x = np.ones(1000, dtype=np.complex64)\n"
    ">>> rc.execute_ctrl(x, 0.0).shape[0]    # base rate: 1000 -> 800\n"
    "800\n"
    ">>> rc2 = RateConverter(rate=0.8, compensate=0)\n"
    ">>> rc2.execute_ctrl(x, 0.05).shape[0]  # +ctrl speeds the tail up\n"
    "851\n" },
  { "execute_ctrl_push",
    (PyCFunction)(void *)RateConverterObj_execute_ctrl_push,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl_push(x, ctrl, out) -> ndarray\n"
    "\n"
    "Push ONE input sample; emit whatever outputs it completes.\n"
    "\n"
    "The per-input streaming form of RateConverter_execute_ctrl(), and the\n"
    "only form a closed loop can use: a block call must know its whole\n"
    "`ctrl` history up front, whereas a timing loop computes each correction\n"
    "*from* the outputs already emitted. Feeding a stream one sample at a\n"
    "time through this reproduces RateConverter_execute_ctrl() on the same\n"
    "block bit-for-bit when ctrl is held constant (the cascade is\n"
    "block-boundary invariant), so the cheap block form stays correct for\n"
    "open-loop use.\n"
    "\n"
    "The integer HB/CIC stages consume the sample and emit at most one\n"
    "intermediate sample each; the terminal Resampler stage then emits 0\n"
    "outputs (a decimator between strobes — the common case), 1, or several\n"
    "(an interpolator). A cascade with no terminal Resampler ignores ctrl.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    One CF32 input sample.\n"
    "ctrl : float\n"
    "    Rate deviation added to the terminal stage's rate for this input\n"
    "    (referenced to the terminal, post-decimation rate).\n"
    "out : NDArray[np.complex64] | None\n"
    "    Output buffer for any emitted samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    CF32 array of the outputs completed by this input (0, 1, or more).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import RateConverter\n"
    ">>> import numpy as np\n"
    ">>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)\n"
    ">>> x = (np.arange(10, dtype=np.float32) + 1).astype(np.complex64)\n"
    ">>> # a decimator emits 0 between strobes, 1 on a strobe:\n"
    ">>> [rc.execute_ctrl_push(complex(v), 0.0).shape[0] for v in x]\n"
    "[1, 1, 1, 1, 0, 1, 1, 1, 1, 0]\n" },
  { "execute_ctrl_push_max_out",
    (PyCFunction)RateConverterObj_execute_ctrl_push_max_out, METH_NOARGS,
    "execute_ctrl_push_max_out() -> int\n"
    "\n"
    "Bound for ONE pushed input: `ceil(rate) + 1` output periods.\n"
    "Non-zero because the push form has no input block to size from.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "reset", (PyCFunction)RateConverterObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero all sub-stage filter memories. Rate, stage count, and stage\n"
    "types are preserved. Processing from a reset state produces the same\n"
    "output as a freshly created converter fed the same input. Use between\n"
    "signal bursts to suppress transient artefacts from prior filter memory.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import RateConverter\n"
    ">>> rc = RateConverter(rate=0.5, compensate=0)\n"
    ">>> rc.reset()\n"
    ">>> rc.rate\n"
    "0.5\n" },
  { "state_bytes", (PyCFunction)RateConverterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the RateConverter has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)RateConverterObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the RateConverter has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)RateConverterObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the RateConverter has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)RateConverterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)RateConverterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a RateConverter be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "RateConverter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)RateConverterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the RateConverter.\n"
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

static PyTypeObject RateConverterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.RateConverter",
  .tp_basicsize                           = sizeof (RateConverterObject),
  .tp_dealloc = (destructor)RateConverterObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a rate converter for the given output/input rate ratio. Selects\n"
    "the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase "
    "Resampler\n"
    "stages at construction time (see file header for the selection table).\n"
    "Setting compensate=1 appends a closed-form Molnar-Vucic CIC\n"
    "droop-compensating FIR after any CIC stage, which improves passband\n"
    "flatness at the cost of one extra FIR stage.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rate : float, default 1.0\n"
    "    Output-to-input sample rate ratio. Any positive float.\n"
    "compensate : int, default 0\n"
    "    Non-zero to append a CIC passband-droop compensating FIR after any "
    "CIC\n"
    "    stage.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``RateConverter:\n"
    "    invalid parameter (need rate > 0, 0 <= beta <= 1, span >= 1, "
    "pulse_sps\n"
    "    > 0, num_phases a power of two >= 2)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.resample import RateConverter\n"
    ">>> rc = RateConverter(rate=0.5, compensate=0)\n"
    ">>> rc.rate\n"
    "0.5\n",
  .tp_methods = RateConverterObj_methods,
  .tp_getset  = RateConverter_getset,
  .tp_new     = RateConverterObj_new,
  .tp_init    = (initproc)RateConverterObj_init,
};
