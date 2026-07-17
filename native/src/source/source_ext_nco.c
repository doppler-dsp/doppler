/*
 * source_ext_nco.c — NCO type for the source module.
 *
 * Included by source_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only source_ext.c is compiled.
 */
/* ======================================================== */
/* NCOObject — wraps nco_state_t *       */
/* ======================================================== */

#include "dp_state_pyhelp.h"
#include "nco/nco_core.h"

typedef struct
{
  PyObject_HEAD nco_state_t *handle;
} NCOObject;

static void
NCOObj_dealloc (NCOObject *self)
{
  if (self->handle)
    nco_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
NCOObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  NCOObject *self = (NCOObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
NCOObj_init (NCOObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[]  = { "norm_freq", "nmax", NULL };
  double        norm_freq = 0.0;
  unsigned long nmax_raw  = 0UL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dk", kwlist, &norm_freq,
                                    &nmax_raw))
    return -1;
  uint32_t nmax = (uint32_t)nmax_raw;
  self->handle  = nco_create (norm_freq, nmax);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "nco_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
NCOObj_reset (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  nco_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
NCOObj_steps_u32 (NCOObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be >= 0");
      return NULL;
    }
  /* NumPy owns the output: allocate exactly n and write into it (sizing a
   * shared reuse buffer to a fixed cap overflowed for large n — #116). */
  npy_intp  dim = (npy_intp)n;
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_UINT32);
  if (!arr)
    return NULL;
  nco_steps_u32 (self->handle, (size_t)n,
                 (uint32_t *)PyArray_DATA ((PyArrayObject *)arr));
  return arr;
}

static PyObject *
NCOObj_steps_u32_scaled (NCOObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be >= 0");
      return NULL;
    }
  npy_intp  dim = (npy_intp)n;
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_UINT32);
  if (!arr)
    return NULL;
  nco_steps_u32_scaled (self->handle, (size_t)n,
                        (uint32_t *)PyArray_DATA ((PyArrayObject *)arr));
  return arr;
}

static PyObject *
NCOObj_steps_u32_ovf (NCOObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  if (n < 0)
    {
      PyErr_SetString (PyExc_ValueError, "n must be >= 0");
      return NULL;
    }
  /* Two parallel outputs (phase u32 + overflow flags u8), both length n, each
   * NumPy-owned and independent (see steps_u32). */
  npy_intp  dim  = (npy_intp)n;
  PyObject *arr0 = PyArray_SimpleNew (1, &dim, NPY_UINT32);
  if (!arr0)
    return NULL;
  PyObject *arr1 = PyArray_SimpleNew (1, &dim, NPY_UINT8);
  if (!arr1)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  nco_steps_u32_ovf (self->handle, (size_t)n,
                     (uint32_t *)PyArray_DATA ((PyArrayObject *)arr0),
                     (uint8_t *)PyArray_DATA ((PyArrayObject *)arr1));
  PyObject *result = PyTuple_Pack (2, arr0, arr1); /* INCREFs both */
  Py_DECREF (arr0);
  Py_DECREF (arr1);
  return result;
}
static PyObject *
NCO_getprop_norm_freq (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (nco_get_norm_freq (self->handle));
}
static int
NCO_setprop_norm_freq (NCOObject *self, PyObject *value,
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
  nco_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
NCO_getprop_phase (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong ((unsigned long)nco_get_phase (self->handle));
}
static int
NCO_setprop_phase (NCOObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long v_raw = 0UL;
  if (!PyArg_Parse (value, "k", &v_raw))
    return -1;
  uint32_t v = (uint32_t)v_raw;
  nco_set_phase (self->handle, v);
  return 0;
}
static PyObject *
NCO_getprop_phase_inc (NCOObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLong (
      (unsigned long)nco_get_phase_inc (self->handle));
}

static PyGetSetDef NCO_getset[] = {
  { "norm_freq", (getter)NCO_getprop_norm_freq, (setter)NCO_setprop_norm_freq,
    "Normalised frequency (read/write). Setting norm_freq recomputes "
    "phase_inc = floor(frac(v) × 2^32) and takes effect on the next "
    "nco_steps_* call; phase is NOT reset.\n",
    NULL },
  { "phase", (getter)NCO_getprop_phase, (setter)NCO_setprop_phase,
    "Current phase accumulator value (read/write). Reading returns the "
    "current integer phase in [0, 2^32).  Writing overrides the accumulator "
    "directly, allowing arbitrary phase offsets without re-creating the "
    "NCO.\n",
    NULL },
  { "phase_inc", (getter)NCO_getprop_phase_inc, NULL,
    "Per-sample phase increment (read-only). Derived from norm_freq as "
    "floor(frac(norm_freq) × 2^32).  Updated automatically whenever norm_freq "
    "is written.  A freq of 0.25 gives phase_inc = 1073741824 (0x40000000).\n",
    NULL },
  { NULL }
};

static PyObject *
NCOObj_destroy (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      nco_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
NCOObj_enter (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
NCOObj_exit (NCOObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      nco_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): the standard state triplet, generated by the
 * shared macro (see dp_state_pyhelp.h) — byte-identical to jm's output.
 * The matching PyMethodDef rows are below. */
DP_PY_STATE_METHODS (NCOObj, NCOObject, self->handle, nco)

static PyObject *
NCOObj_steps_u32_max_out (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32_scaled_max_out (NCOObject *self,
                                 PyObject  *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_scaled_max_out (self->handle));
}

static PyObject *
NCOObj_steps_u32_ctrl (NCOObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "ctrl", "out", NULL };
  PyObject      *ctrl_obj  = NULL;
  PyArrayObject *ctrl_arr  = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &ctrl_obj,
                                    &out_obj))
    return NULL;
  ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (ctrl_obj, NPY_FLOAT,
                                                NPY_ARRAY_C_CONTIGUOUS);
  if (!ctrl_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT32, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = nco_steps_u32_ctrl_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (ctrl_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (ctrl_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t n_out = nco_steps_u32_ctrl (
          self->handle, (const float *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr), (uint32_t *)PyArray_DATA (out_arr));
      Py_DECREF (ctrl_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT32,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  /* NumPy owns the output: allocate exactly ctrl_len and write into it,
   * fresh every call -- matches steps_u32/steps_u32_scaled/steps_u32_ovf
   * above (see the steps_u32 comment re #116). jm's default
   * variable_output template (cached buffer + gh-437 weakref-gated
   * retire) leaks unboundedly here: Python only decrefs a call's
   * PREVIOUS return value AFTER evaluating the new call
   * (`x = obj.method(...)` rebinds `x` only once the RHS finishes), so
   * the "is the last view still alive" check is true on essentially
   * every call in a tight loop -- exactly the natural way to drive a
   * per-epoch control-port NCO -- and the retired buffer is then never
   * reclaimed until the whole NCO is destroyed. Confirmed leaking
   * ~12 KB/call under that exact pattern before this fix. */
  size_t    ctrl_len = (size_t)PyArray_SIZE (ctrl_arr);
  npy_intp  dim      = (npy_intp)ctrl_len;
  PyObject *arr      = PyArray_SimpleNew (1, &dim, NPY_UINT32);
  if (!arr)
    {
      Py_DECREF (ctrl_arr);
      return NULL;
    }
  nco_steps_u32_ctrl (self->handle, (const float *)PyArray_DATA (ctrl_arr),
                      ctrl_len,
                      (uint32_t *)PyArray_DATA ((PyArrayObject *)arr));
  Py_DECREF (ctrl_arr);
  return arr;
}

static PyObject *
NCOObj_steps_u32_ctrl_max_out (NCOObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (nco_steps_u32_ctrl_max_out (self->handle));
}

static PyMethodDef NCOObj_methods[] = {
  { "reset", (PyCFunction)NCOObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "steps_u32", (PyCFunction)NCOObj_steps_u32, METH_VARARGS,
    "steps_u32(n=1) -> ndarray\n"
    "\n"
    "Advance n samples; write raw uint32 accumulator values.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_scaled", (PyCFunction)NCOObj_steps_u32_scaled, METH_VARARGS,
    "steps_u32_scaled(n=1) -> ndarray\n"
    "\n"
    "Advance n samples; values scaled to [0, nmax).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_scaled(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_ovf", (PyCFunction)NCOObj_steps_u32_ovf, METH_VARARGS,
    "steps_u32_ovf(n=1) -> tuple[ndarray, ndarray]\n"
    "\n"
    "Advance n samples; write raw phase values and per-sample carry.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_ovf(4)\n"
    "    >>> y[0].dtype\n"
    "    dtype('uint32')\n" },
  { "state_bytes", (PyCFunction)NCOObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)NCOObj_get_state, METH_NOARGS,
    "Serialize the phase accumulator to bytes." },
  { "set_state", (PyCFunction)NCOObj_set_state, METH_O,
    "Restore phase from a get_state() blob." },
  { "destroy", (PyCFunction)NCOObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)NCOObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)NCOObj_exit, METH_VARARGS, NULL },
  { "steps_u32_max_out", (PyCFunction)NCOObj_steps_u32_max_out, METH_NOARGS,
    "steps_u32_max_out() -> int\n\nMax output length steps_u32() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "steps_u32_scaled_max_out", (PyCFunction)NCOObj_steps_u32_scaled_max_out,
    METH_NOARGS,
    "steps_u32_scaled_max_out() -> int\n\nMax output length "
    "steps_u32_scaled() can produce for the current state.\nUse to size the "
    "``out=`` buffer." },
  { "steps_u32_ctrl", (PyCFunction)NCOObj_steps_u32_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "steps_u32_ctrl(ctrl) -> ndarray\n"
    "\n"
    "Advance ctrl_len samples; raw phase, with a per-sample control offset "
    "added on top of the fixed phase_inc (not persisted).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import NCO\n"
    "    >>> obj = NCO(0.0, 0)\n"
    "    >>> y = obj.steps_u32_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('uint32')\n" },
  { "steps_u32_ctrl_max_out", (PyCFunction)NCOObj_steps_u32_ctrl_max_out,
    METH_NOARGS,
    "steps_u32_ctrl_max_out() -> int\n\nMax output length steps_u32_ctrl() "
    "can produce for the current state.\nUse to size the ``out=`` buffer." },
  { NULL }
};

static PyTypeObject NCOObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "source.NCO",
  .tp_basicsize                           = sizeof (NCOObject),
  .tp_dealloc                             = (destructor)NCOObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an NCO instance.\n",
  .tp_methods                             = NCOObj_methods,
  .tp_getset                              = NCO_getset,
  .tp_new                                 = NCOObj_new,
  .tp_init                                = (initproc)NCOObj_init,
};
