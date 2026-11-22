/* jm:generated track_ext_state_filter.c */
/*
 * track_ext_state_filter.c — StateFilter type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in track_ext_state_filter_extra.c.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* StateFilterObject — wraps dp_state_filter_state_t *       */
/* ======================================================== */

#include "doppler/state_filter/state_filter_core.h"

typedef struct
{
  PyObject_HEAD dp_state_filter_state_t *handle;
} StateFilterObject;

static void
StateFilterObj_dealloc (StateFilterObject *self)
{
  if (self->handle)
    dp_state_filter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
StateFilterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  StateFilterObject *self = (StateFilterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
StateFilterObj_init (StateFilterObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "bn", "t", NULL };
  double       bn       = 0.01;
  double       t        = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dd", kwlist, &bn, &t))
    return -1;
  const char *_why = NULL;
  self->handle     = dp_state_filter_create_why (bn, t, &_why);
  if (!self->handle)
    {
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "bn must be >= 0 and t > 0, both finite, with "
                         "bn*t inside the domain");
      return -1;
    }
  return 0;
}

static PyObject *
StateFilter_step (StateFilterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double x;
  if (!PyArg_ParseTuple (args, "d", &x))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double y = dp_state_filter_step (self->handle, x);
  return PyFloat_FromDouble (y);
}

static PyObject *
StateFilter_steps (StateFilterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj   = NULL;
  PyObject    *out_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", kwlist, &in_obj,
                                    &out_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)jm_array_arg (
      in_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_DOUBLE
          || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (PyExc_TypeError,
                           "out must be a writable, C-contiguous"
                           " ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)jm_array_arg (
          out_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE,
          "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_SIZE (out_arr) != n)
        {
          PyErr_Format (PyExc_ValueError, "out length %zd != input length %zd",
                        (Py_ssize_t)PyArray_SIZE (out_arr), (Py_ssize_t)n);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (in_arr);
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      dp_state_filter_steps (self->handle,
                             (const double *)PyArray_DATA (in_arr),
                             (double *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_DOUBLE);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  if (!self->handle)
    {
      Py_DECREF (in_arr);
      Py_DECREF (out_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_state_filter_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                         (double *)PyArray_DATA ((PyArrayObject *)out_arr),
                         (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
StateFilterObj_configure (StateFilterObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "t", NULL };
  double       bn        = 0.0;
  double       t         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &bn, &t))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_state_filter_configure (self->handle, bn, t);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure refused (bn, t): t must be positive and "
                    "finite, bn finite and >= 0, and bn*t inside the domain; "
                    "nothing changed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
StateFilterObj_reset (StateFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_state_filter_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
StateFilterObj_bandwidth (StateFilterObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "delay", "gain", NULL };
  double       delay     = 0.0;
  double       gain      = 1.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dd", _kwlist, &delay, &gain))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double y = dp_state_filter_bandwidth (self->handle, delay, gain);
  return PyFloat_FromDouble (y);
}

static PyObject *
StateFilterObj_stable (StateFilterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "delay", "gain", NULL };
  double       delay     = 0.0;
  double       gain      = 1.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dd", _kwlist, &delay, &gain))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  bool y = dp_state_filter_stable (self->handle, delay, gain);
  return PyBool_FromLong ((long)(y));
}

static PyObject *
StateFilterObj_state_bytes (StateFilterObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_state_filter_state_bytes (self->handle));
}

static PyObject *
StateFilterObj_get_state (StateFilterObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_state_filter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_state_filter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
StateFilterObj_set_state (StateFilterObject *self, PyObject *arg)
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
      != dp_state_filter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_state_filter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
StateFilter_getprop_kp (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->kp);
}
static PyObject *
StateFilter_getprop_ki (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->ki);
}
static PyObject *
StateFilter_getprop_kr (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->kr);
}
static PyObject *
StateFilter_getprop_integ (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->integ);
}
static int
StateFilter_setprop_integ (StateFilterObject *self, PyObject *value,
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  self->handle->integ = v;
  return 0;
}
static PyObject *
StateFilter_getprop_rate (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->rate);
}
static int
StateFilter_setprop_rate (StateFilterObject *self, PyObject *value,
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  self->handle->rate = v;
  return 0;
}
static PyObject *
StateFilter_getprop_bn (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->bn);
}
static PyObject *
StateFilter_getprop_t (StateFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->t);
}

static PyGetSetDef StateFilter_getset[]
    = { { "kp", (getter)StateFilter_getprop_kp, NULL,
          "proportional gain (from bn, t).\n", NULL },
        { "ki", (getter)StateFilter_getprop_ki, NULL,
          "frequency-path gain (from bn, t).\n", NULL },
        { "kr", (getter)StateFilter_getprop_kr, NULL,
          "rate-path gain (from bn, t).\n", NULL },
        { "integ", (getter)StateFilter_getprop_integ,
          (setter)StateFilter_setprop_integ,
          "frequency estimate, control units per update.\n", NULL },
        { "rate", (getter)StateFilter_getprop_rate,
          (setter)StateFilter_setprop_rate,
          "slope of the frequency estimate: control units per update per "
          "update (a Doppler rate R in Hz/s at an update period T in seconds "
          "is R * T^2 cycles per update per update).\n",
          NULL },
        { "bn", (getter)StateFilter_getprop_bn, NULL,
          "loop noise bandwidth, normalized cycles/sample.\n", NULL },
        { "t", (getter)StateFilter_getprop_t, NULL,
          "update period in samples.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
StateFilterObj_destroy (StateFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_state_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
StateFilterObj_enter (StateFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
StateFilterObj_exit (StateFilterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_state_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef StateFilterObj_methods[] = {
  { "step", (PyCFunction)StateFilter_step, METH_VARARGS,
    "step(x) -> double\n"
    "\n"
    "Advance the loop one update with error x and return the control\n"
    "value the tracker should apply.\n"
    "\n"
    "`rate += kr*x; integ += rate + ki*x; control = integ + kp*x`. Inline,\n"
    "with no test of any kind: this is the hot step of a tracking loop.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Loop error (discriminator output) for this update.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Control value `integ + kp*x`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.02, t=1.0)\n"
    ">>> round(sf.step(1.0), 6)             # unit error: kr + ki + kp\n"
    "0.047602\n"
    ">>> round(sf.rate, 8), round(sf.integ, 8)\n"
    "(1.257e-05, 0.00109406)\n"
    "\n" },
  { "steps", (PyCFunction)(void *)StateFilter_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Filter a block of loop errors, one control value per update.\n"
    "\n"
    "Equivalent to dp_state_filter_step() once per element in order,\n"
    "carrying both accumulators across the block.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.float64]\n"
    "    Loop-error array, one discriminator sample per update.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.05, t=1.0)\n"
    ">>> ctl = sf.steps(np.full(50, 0.1))   # constant error into the loop\n"
    ">>> round(float(ctl[0]), 4)            # the first nudge\n"
    "0.0117\n"
    ">>> round(float(ctl[-1]), 4)           # open loop: ramping\n"
    "0.0638\n"
    "\n" },

  { "configure", (PyCFunction)(void *)StateFilterObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, t) -> None\n"
    "\n"
    "Recompute the gains for a new (bn, t); preserves the frequency and rate "
    "estimates. Raises ValueError, changing nothing, when (bn, t) is outside "
    "the domain.\n"
    "\n"
    "As dp_loop_filter_configure() does for the second-order filter: widen\n"
    "to acquire, narrow to track, holding the estimates. A `(bn, t)` outside\n"
    "dp_state_filter_params_ok() is refused and changes nothing.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalized cycles/sample.\n"
    "t : float\n"
    "    Update period in samples.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure refused (bn, t): t must be positive and finite, bn\n"
    "    finite and >= 0, and bn*t inside the domain; nothing changed``,\n"
    "    with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.01, t=1.0)\n"
    ">>> _ = sf.step(1.0)\n"
    ">>> before = sf.rate\n"
    ">>> sf.configure(0.05, 1.0)            # widen the loop, keep lock\n"
    ">>> sf.rate == before                  # the estimates are preserved\n"
    "True\n"
    ">>> round(sf.kp, 6)                    # the gains rose\n"
    "0.111058\n" },
  { "reset", (PyCFunction)StateFilterObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero the frequency and rate estimates; keep the configured gains.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.02, t=1.0)\n"
    ">>> for _ in range(10):\n"
    "...     _ = sf.step(1.0)               # open loop: both estimates ramp\n"
    ">>> round(sf.integ, 6), round(sf.rate, 6)\n"
    "(0.011506, 0.000126)\n"
    ">>> sf.reset()\n"
    ">>> sf.integ, sf.rate                  # cleared, gains kept\n"
    "(0.0, 0.0)\n" },
  { "bandwidth", (PyCFunction)(void *)StateFilterObj_bandwidth,
    METH_VARARGS | METH_KEYWORDS,
    "bandwidth(delay, gain) -> float\n"
    "\n"
    "Exact delivered noise bandwidth of this loop, in cycles per update, for "
    "a detector delay of `delay` updates and a loop gain of `gain` times "
    "nominal. NaN when the loop is not stable there.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "delay : float, default 0.0\n"
    "    Detector delay in updates.\n"
    "gain : float, default 1.0\n"
    "    Loop gain as a multiple of nominal.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Cycles per update, or NaN if not stable there.\n"
    "\n"
    "See Also\n"
    "--------\n"
    "dp_state_filter_loop_bandwidth()\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.03, t=1.0)\n"
    ">>> round(sf.bandwidth(), 6)           # delay 0: the bandwidth asked "
    "for\n"
    "0.03\n"
    ">>> round(sf.bandwidth(delay=1.0), 6)  # one update of detector delay\n"
    "0.032646\n"
    ">>> sf.bandwidth(gain=0.05)            # unstable: no bandwidth\n"
    "nan\n" },
  { "stable", (PyCFunction)(void *)StateFilterObj_stable,
    METH_VARARGS | METH_KEYWORDS,
    "stable(delay, gain) -> bool\n"
    "\n"
    "Whether this loop is stable for a detector delay of `delay` updates and "
    "a loop gain of `gain` times nominal.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "delay : float, default 0.0\n"
    "    Detector delay in updates.\n"
    "gain : float, default 1.0\n"
    "    Loop gain as a multiple of nominal.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bool\n"
    "    true if stable, false if not.\n"
    "\n"
    "See Also\n"
    "--------\n"
    "dp_state_filter_loop_stable()\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.12, t=1.0)   # the bound itself\n"
    ">>> sf.stable(delay=2.0, gain=2.0)     # worst corner of the rectangle\n"
    "True\n"
    ">>> sf.stable(delay=2.0, gain=400.0)   # a detector slope far too high\n"
    "False\n"
    ">>> sf.stable(gain=0.05)               # ...and far too low\n"
    "False\n" },
  { "state_bytes", (PyCFunction)StateFilterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the StateFilter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)StateFilterObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the StateFilter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)StateFilterObj_set_state, METH_O,
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
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the StateFilter has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)StateFilterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)StateFilterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a StateFilter be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "StateFilter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)StateFilterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the StateFilter.\n"
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

static PyTypeObject StateFilterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.track.StateFilter",
  .tp_basicsize                           = sizeof (StateFilterObject),
  .tp_dealloc                             = (destructor)StateFilterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "A third-order state filter whose closed loop has noise bandwidth `bn`,\n"
    "for a loop whose rest has unit gain.\n"
    "\n"
    "The contract. The closed loop's noise bandwidth is `bn` cycles per\n"
    "sample, a fixed function of `kp`, `ki` and `kr`, provided the rest of\n"
    "the loop has unit gain. The filter cannot see the rest of the loop, so\n"
    "each of these is the caller's to deliver, and each is checkable:\n"
    "\n"
    "1. The error detector's slope at lock is 1 per unit of the filter's\n"
    "   error input, open loop and noise free, in the unit the control is\n"
    "   applied in per update; the radians-to-cycles factor, where the\n"
    "   oscillator takes cycles, is applied once after the filter, never\n"
    "   inside the loop gain.\n"
    "2. The actuator applies the control with unit gain, once per update,\n"
    "   `t` samples apart.\n"
    "3. The loop is tracking, inside the detector's linear region.\n"
    "4. The error reaches the filter and the control reaches the actuator\n"
    "   within the same update: the delivered bandwidth is the one asked for\n"
    "   at a delay of zero.\n"
    "\n"
    "A loop gain K other than 1 changes the delivered bandwidth, and this\n"
    "third-order loop is also stable only for K above a lower edge near\n"
    "1/(a*b) of nominal and below an upper edge that falls with the delay\n"
    "(docs/design/loop-filter.md section 12), so a detector slope below that\n"
    "edge breaks the loop with no error reported. The second-order closed\n"
    "form (4*K*zeta^2 + 1) / (4*zeta^2 + 1) does not apply to it. A detector\n"
    "whose slope is not 1, or rides on signal level or SNR, is scaled to\n"
    "unit slope before the filter; a slope that rides on level makes the\n"
    "bandwidth ride on level.\n"
    "\n"
    "Held by the state filter's own tests for the promise,\n"
    "test_loop_filter_contract.py for these sentences on every face, and the\n"
    "detector matrix of doppler#3229 for the callers' side.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float, default 0.01\n"
    "    Loop noise bandwidth, normalized cycles/sample, of the closed loop\n"
    "    given a unit-slope detector (see the contract); >= 0 (default "
    "0.01).\n"
    "    `bn = 0` is a deliberately frozen loop.\n"
    "t : float, default 1.0\n"
    "    Update period in samples; > 0 (default 1.0).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is the reason the\n"
    "    constructor gives, or ``bn must be >= 0 and t > 0, both finite, "
    "with\n"
    "    bn*t inside the domain`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import StateFilter\n"
    ">>> sf = StateFilter(bn=0.01, t=1.0)\n"
    ">>> round(sf.kp, 6), round(sf.ki, 6)   # the gains follow from bn and t\n"
    "(0.023622, 0.000279)\n"
    ">>> sf.integ, sf.rate                  # both estimates start at zero\n"
    "(0.0, 0.0)\n"
    ">>> StateFilter(bn=0.5)                # bn*t past the stability bound\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: bn*t is above the largest the filter accepts, the bound the "
    "third-order loop is stable to\n",
  .tp_methods = StateFilterObj_methods,
  .tp_getset  = StateFilter_getset,
  .tp_new     = StateFilterObj_new,
  .tp_init    = (initproc)StateFilterObj_init,
};
