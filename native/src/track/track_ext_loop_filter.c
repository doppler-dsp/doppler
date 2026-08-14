/*
 * track_ext_loop_filter.c — LoopFilter type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* LoopFilterObject — wraps loop_filter_state_t *       */
/* ======================================================== */

#include "loop_filter/loop_filter_core.h"

typedef struct
{
  PyObject_HEAD loop_filter_state_t *handle;
} LoopFilterObject;

static void
LoopFilterObj_dealloc (LoopFilterObject *self)
{
  if (self->handle)
    loop_filter_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
LoopFilterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  LoopFilterObject *self = (LoopFilterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
LoopFilterObj_init (LoopFilterObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "bn", "zeta", "t", NULL };
  double       bn       = 0.01;
  double       zeta     = 0.707;
  double       t        = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddd", kwlist, &bn, &zeta,
                                    &t))
    return -1;
  self->handle = loop_filter_create (bn, zeta, t);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "bn must be >= 0, zeta > 0 and t > 0, and all three "
                       "finite");
      return -1;
    }
  return 0;
}

static PyObject *
LoopFilter_step (LoopFilterObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  double x;
  if (!PyArg_ParseTuple (args, "d", &x))
    return NULL;
  double y = loop_filter_step (self->handle, x);
  return PyFloat_FromDouble (y);
}

static PyObject *
LoopFilter_steps (LoopFilterObject *self, PyObject *args, PyObject *kwds)
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

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      loop_filter_steps (self->handle, (const double *)PyArray_DATA (in_arr),
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

  loop_filter_steps (self->handle, (const double *)PyArray_DATA (in_arr),
                     (double *)PyArray_DATA ((PyArrayObject *)out_arr),
                     (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
LoopFilterObj_configure (LoopFilterObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "zeta", "t", NULL };
  double       bn        = 0.0;
  double       zeta      = 0.0;
  double       t         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddd", _kwlist, &bn, &zeta,
                                    &t))
    return NULL;
  loop_filter_configure (self->handle, bn, zeta, t);
  Py_RETURN_NONE;
}

static PyObject *
LoopFilterObj_reset (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  loop_filter_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
LoopFilterObj_state_bytes (LoopFilterObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (loop_filter_state_bytes (self->handle));
}

static PyObject *
LoopFilterObj_get_state (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = loop_filter_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  loop_filter_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
LoopFilterObj_set_state (LoopFilterObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != loop_filter_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (loop_filter_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
LoopFilter_getprop_kp (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->kp);
}
static PyObject *
LoopFilter_getprop_ki (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->ki);
}
static PyObject *
LoopFilter_getprop_integ (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->integ);
}
static int
LoopFilter_setprop_integ (LoopFilterObject *self, PyObject *value,
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
  self->handle->integ = v;
  return 0;
}
static PyObject *
LoopFilter_getprop_bn (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->bn);
}
static PyObject *
LoopFilter_getprop_zeta (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->zeta);
}
static PyObject *
LoopFilter_getprop_t (LoopFilterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->t);
}

static PyGetSetDef LoopFilter_getset[]
    = { { "kp", (getter)LoopFilter_getprop_kp, NULL,
          "proportional gain (derived from bn, zeta, t).\n", NULL },
        { "ki", (getter)LoopFilter_getprop_ki, NULL,
          "integral gain (derived from bn, zeta, t).\n", NULL },
        { "integ", (getter)LoopFilter_getprop_integ,
          (setter)LoopFilter_setprop_integ,
          "integrator memory = running rate/freq estimate.\n", NULL },
        { "bn", (getter)LoopFilter_getprop_bn, NULL,
          "loop noise bandwidth, normalized cycles/sample.\n", NULL },
        { "zeta", (getter)LoopFilter_getprop_zeta, NULL,
          "damping factor (0.707 = critically damped).\n", NULL },
        { "t", (getter)LoopFilter_getprop_t, NULL,
          "update period in samples.\n", NULL },
        { NULL } };

static PyObject *
LoopFilterObj_destroy (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      loop_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
LoopFilterObj_enter (LoopFilterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
LoopFilterObj_exit (LoopFilterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      loop_filter_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef LoopFilterObj_methods[] = {
  { "step", (PyCFunction)LoopFilter_step, METH_VARARGS,
    "step(x) -> double\n"
    "\n"
    "Advance the loop one update with error x and return the control\n"
    "value the tracker should apply.\n"
    "\n"
    "The PI recurrence is `integ += ki*x; control = integ + kp*x`: the\n"
    "integrator accumulates the running frequency/rate estimate while the\n"
    "proportional term kp*x is the instantaneous phase nudge.\n"
    "\n"
    "Fed a constant error with nothing closing the loop, the integrator —\n"
    "and therefore the control — **ramps without bound**; measured at 1.84x\n"
    "between updates 200 and 400 at `bn = 0.02`. That is the accumulation\n"
    "working, not a defect, and it is what pulls a Costas/DLL/timing loop\n"
    "into lock once the loop IS closed, because a converging loop is one\n"
    "whose error is being driven to zero by the correction. Convergence is a\n"
    "property of the closed loop; this function is one term in it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Loop error (discriminator output) for this update.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Control value integ+kp*x to drive the NCO / interpolator.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import LoopFilter\n"
    ">>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)\n"
    ">>> round(lf.step(1.0), 6)   # unit error: control = ki + kp\n"
    "0.05331\n"
    ">>> round(lf.integ, 6)       # integrator now holds ki\n"
    "0.001385\n"
    "\n" },
  { "steps", (PyCFunction)(void *)LoopFilter_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Filter a whole block of loop errors, returning the control value for\n"
    "each update.\n"
    "\n"
    "Equivalent to calling loop_filter_step() once per element of x in\n"
    "order, carrying the integrator across the block, so the loop's memory\n"
    "and lock state persist from one call to the next. This is the block\n"
    "path used to run a captured error sequence through the filter in one\n"
    "shot — a plain per-element loop, not a vectorized one: the recurrence\n"
    "is sequential, so each update depends on the one before it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float64]\n"
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
    ">>> from doppler.track import LoopFilter\n"
    ">>> lf = LoopFilter(bn=0.05, zeta=0.707, t=1.0)\n"
    ">>> ctl = lf.steps(np.full(50, 0.1))   # constant error into the loop\n"
    ">>> round(float(ctl[0]), 4)            # first control nudge\n"
    "0.0133\n"
    ">>> round(float(ctl[-1]), 4)           # open loop: ramping\n"
    "0.0541\n"
    "\n" },

  { "configure", (PyCFunction)(void *)LoopFilterObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta, t) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta, t); preserves the\n"
    "integrator.\n"
    "\n"
    "Recomputes the proportional and integral gains from the standard\n"
    "2nd-order form but leaves integ untouched, so a loop can be widened for\n"
    "fast acquisition and then narrowed for steady-state tracking while\n"
    "holding its accumulated frequency/rate estimate — the retune preserves\n"
    "lock.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalized cycles/sample (>= 0).\n"
    "zeta : float\n"
    "    Damping factor (typically 0.707).\n"
    "t : float\n"
    "    Update period in samples (> 0).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import LoopFilter\n"
    ">>> lf = LoopFilter(bn=0.01, zeta=0.707, t=1.0)\n"
    ">>> _ = lf.step(1.0)\n"
    ">>> before = round(lf.integ, 6)\n"
    ">>> lf.configure(0.05, 0.707, 1.0)   # widen the loop, keep lock\n"
    ">>> round(lf.integ, 6) == before     # integrator preserved\n"
    "True\n"
    ">>> round(lf.kp, 6)                  # proportional gain rose\n"
    "0.124728\n" },
  { "reset", (PyCFunction)LoopFilterObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero the integrator; keep the configured gains.\n"
    "\n"
    "Clears the accumulated frequency/rate estimate (integ) back to zero but\n"
    "leaves kp / ki as configured, so the loop reacquires from a clean slate\n"
    "at its current bandwidth — the right thing when a tracker drops lock\n"
    "and must restart, without re-deriving gains.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import LoopFilter\n"
    ">>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)\n"
    ">>> for _ in range(10):\n"
    "...     _ = lf.step(1.0)             # ramp the integrator\n"
    ">>> round(lf.integ, 6)\n"
    "0.013849\n"
    ">>> lf.reset()\n"
    ">>> lf.integ                          # integrator cleared, gains kept\n"
    "0.0\n" },
  { "state_bytes", (PyCFunction)LoopFilterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the LoopFilter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)LoopFilterObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the LoopFilter has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)LoopFilterObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the LoopFilter has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)LoopFilterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)LoopFilterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a LoopFilter be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "LoopFilter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)LoopFilterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the LoopFilter.\n"
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

static PyTypeObject LoopFilterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.LoopFilter",
  .tp_basicsize                           = sizeof (LoopFilterObject),
  .tp_dealloc                             = (destructor)LoopFilterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a loop_filter instance, validating its arguments.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float, default 0.01\n"
    "    Loop noise bandwidth, normalized cycles/sample; >= 0 and finite\n"
    "    (default 0.01).\n"
    "zeta : float, default 0.707\n"
    "    Damping factor; > 0 and finite (default 0.707).\n"
    "t : float, default 1.0\n"
    "    Update period in samples; > 0 and finite (default 1.0).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``bn must be >= 0, "
    "zeta\n"
    "    > 0 and t > 0, and all three finite``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler import LoopFilter\n"
    ">>> obj = LoopFilter(bn=0.01, zeta=0.707, t=1.0)\n",
  .tp_methods = LoopFilterObj_methods,
  .tp_getset  = LoopFilter_getset,
  .tp_new     = LoopFilterObj_new,
  .tp_init    = (initproc)LoopFilterObj_init,
};
