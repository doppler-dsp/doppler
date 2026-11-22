/* jm:generated track_ext_state_filter_kalman.c */
/*
 * track_ext_state_filter_kalman.c — KalmanStateFilter type for the track
 * module.
 *
 * Included by track_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in track_ext_state_filter_kalman_extra.c.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* KalmanStateFilterObject — wraps dp_state_filter_kalman_state_t *       */
/* ======================================================== */

#include "doppler/state_filter_kalman/state_filter_kalman_core.h"

typedef struct
{
  PyObject_HEAD dp_state_filter_kalman_state_t *handle;
} KalmanStateFilterObject;

static void
KalmanStateFilterObj_dealloc (KalmanStateFilterObject *self)
{
  if (self->handle)
    dp_state_filter_kalman_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
KalmanStateFilterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  KalmanStateFilterObject *self
      = (KalmanStateFilterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
KalmanStateFilterObj_init (KalmanStateFilterObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[]
      = { "sigma_e", "sigma_a", "t", "sigma_f0", "bn", NULL };
  double sigma_e  = 0.0;
  double sigma_a  = 0.0;
  double t        = 1.0;
  double sigma_f0 = 0.0;
  double bn       = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddddd", kwlist, &sigma_e,
                                    &sigma_a, &t, &sigma_f0, &bn))
    return -1;
  const char *_why = NULL;
  self->handle = dp_state_filter_kalman_create_args_why (sigma_e, sigma_a, t,
                                                         sigma_f0, bn, &_why);
  if (!self->handle)
    {
      if (_why)
        PyErr_SetString (PyExc_ValueError, _why);
      else
        PyErr_SetString (PyExc_ValueError,
                         "give sigma_e and sigma_a, or bn alone, in range");
      return -1;
    }
  return 0;
}

static PyObject *
KalmanStateFilterObj_reset (KalmanStateFilterObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_state_filter_kalman_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
KalmanStateFilter_step (KalmanStateFilterObject *self, PyObject *args)
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
  double y = dp_state_filter_kalman_step (self->handle, x);
  return PyFloat_FromDouble (y);
}

static PyObject *
KalmanStateFilter_steps (KalmanStateFilterObject *self, PyObject *args,
                         PyObject *kwds)
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
      dp_state_filter_kalman_steps (
          self->handle, (const double *)PyArray_DATA (in_arr),
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
  dp_state_filter_kalman_steps (
      self->handle, (const double *)PyArray_DATA (in_arr),
      (double *)PyArray_DATA ((PyArrayObject *)out_arr), (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
KalmanStateFilterObj_configure (KalmanStateFilterObject *self, PyObject *args,
                                PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "sigma_e", "sigma_a", "t", "sigma_f0", "bn", NULL };
  double sigma_e  = 0.0;
  double sigma_a  = 0.0;
  double t        = 0.0;
  double sigma_f0 = 0.0;
  double bn       = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddddd", _kwlist, &sigma_e,
                                    &sigma_a, &t, &sigma_f0, &bn))
    return NULL;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_state_filter_kalman_configure (self->handle, sigma_e, sigma_a,
                                              t, sigma_f0, bn);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "configure refused: name sigma_e and sigma_a (and "
                    "optionally sigma_f0), or bn alone, with t positive and "
                    "finite and the result inside the domain; nothing "
                    "changed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
KalmanStateFilterObj_bandwidth (KalmanStateFilterObject *self, PyObject *args,
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
  double y = dp_state_filter_kalman_bandwidth (self->handle, delay, gain);
  return PyFloat_FromDouble (y);
}

static PyObject *
KalmanStateFilterObj_stable (KalmanStateFilterObject *self, PyObject *args,
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
  bool y = dp_state_filter_kalman_stable (self->handle, delay, gain);
  return PyBool_FromLong ((long)(y));
}

static PyObject *
KalmanStateFilterObj_state_bytes (KalmanStateFilterObject *self,
                                  PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_state_filter_kalman_state_bytes (self->handle));
}

static PyObject *
KalmanStateFilterObj_get_state (KalmanStateFilterObject *self,
                                PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_state_filter_kalman_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_state_filter_kalman_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
KalmanStateFilterObj_set_state (KalmanStateFilterObject *self, PyObject *arg)
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
      != dp_state_filter_kalman_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_state_filter_kalman_set_state (self->handle, PyBytes_AS_STRING (arg))
      != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
KalmanStateFilter_getprop_kp (KalmanStateFilterObject *self,
                              void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.kp));
}
static PyObject *
KalmanStateFilter_getprop_ki (KalmanStateFilterObject *self,
                              void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.ki));
}
static PyObject *
KalmanStateFilter_getprop_kr (KalmanStateFilterObject *self,
                              void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.kr));
}
static PyObject *
KalmanStateFilter_getprop_integ (KalmanStateFilterObject *self,
                                 void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.integ));
}
static PyObject *
KalmanStateFilter_getprop_rate (KalmanStateFilterObject *self,
                                void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.rate));
}
static PyObject *
KalmanStateFilter_getprop_bn (KalmanStateFilterObject *self,
                              void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.bn));
}
static PyObject *
KalmanStateFilter_getprop_t (KalmanStateFilterObject *self,
                             void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((self->handle->base.t));
}
static PyObject *
KalmanStateFilter_getprop_sigma_f (KalmanStateFilterObject *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((dp_state_filter_kalman_sigma_f (self->handle)));
}

static PyGetSetDef KalmanStateFilter_getset[] = {
  { "kp", (getter)KalmanStateFilter_getprop_kp, NULL,
    "Proportional gain in force now (rewritten each update).\n", NULL },
  { "ki", (getter)KalmanStateFilter_getprop_ki, NULL,
    "Integral gain in force now.\n", NULL },
  { "kr", (getter)KalmanStateFilter_getprop_kr, NULL,
    "Rate gain in force now.\n", NULL },
  { "integ", (getter)KalmanStateFilter_getprop_integ, NULL,
    "The frequency estimate.\n", NULL },
  { "rate", (getter)KalmanStateFilter_getprop_rate, NULL,
    "The rate estimate.\n", NULL },
  { "bn", (getter)KalmanStateFilter_getprop_bn, NULL,
    "The steady-state noise bandwidth in cycles per sample.\n", NULL },
  { "t", (getter)KalmanStateFilter_getprop_t, NULL,
    "The update period in samples.\n", NULL },
  { "sigma_f", (getter)KalmanStateFilter_getprop_sigma_f, NULL,
    "The steady state's frequency deviation for these figures, in the loop's "
    "units: the lowest `sigma_f0` accepted; the highest is 5 times it up to "
    "bn*t 0.04 and less above (see the validation report).\n",
    NULL },
  { NULL, NULL, NULL, NULL, NULL }
};

static PyObject *
KalmanStateFilterObj_destroy (KalmanStateFilterObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_state_filter_kalman_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
KalmanStateFilterObj_enter (KalmanStateFilterObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
KalmanStateFilterObj_exit (KalmanStateFilterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_state_filter_kalman_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef KalmanStateFilterObj_methods[] = {
  { "reset", (PyCFunction)KalmanStateFilterObj_reset, METH_NOARGS,
    "Return to the start: the estimates zero and the covariance back to\n"
    "the one the filter began with. The noise figures are kept.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)\n"
    ">>> _ = k.steps(np.full(50, 0.1))\n"
    ">>> round(k.integ, 4)\n"
    "0.0016\n"
    ">>> k.reset()\n"
    ">>> k.integ\n"
    "0.0\n" },
  { "step", (PyCFunction)KalmanStateFilter_step, METH_VARARGS,
    "step(x) -> double\n"
    "\n"
    "One update, the call to use: the loop's three lines on the gains in\n"
    "force, then the covariance advanced and the gains refreshed. Inline,\n"
    "over dp_state_filter_kalman_update().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    The control value `integ + kp*e`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)\n"
    ">>> round(k.step(0.1), 4)             # the first nudge\n"
    "0.0024\n"
    "\n" },
  { "steps", (PyCFunction)(void *)KalmanStateFilter_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "A block of updates, equivalent to dp_state_filter_kalman_step() once\n"
    "per element.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.float64]\n"
    "    Loop errors.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)\n"
    ">>> y = k.steps(np.full(50, 0.1))     # constant error: open loop, "
    "ramping\n"
    ">>> round(float(y[0]), 4), round(float(y[-1]), 4)\n"
    "(0.0024, 0.004)\n"
    "\n" },

  { "configure", (PyCFunction)(void *)KalmanStateFilterObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(sigma_e, sigma_a, t, sigma_f0, bn) -> None\n"
    "\n"
    "Change the figures (or `bn` alone), keeping the frequency and rate "
    "estimates; the covariance restarts from the new steady state, so a "
    "retune restarts the gain schedule while the estimates carry. An argument "
    "of 0 keeps its current value (`sigma_f0` 0 is the steady state), so a "
    "call names only what changes. A filter built from `bn` holds the figures "
    "that bandwidth implies (`sigma_e` 1), a call with `bn` replaces the "
    "stored figures by those it implies, and `bn` may not be mixed with the "
    "figures. With no arguments the gains go to the steady state at once and "
    "the estimates carry (this ends a high-gain start early); "
    "`configure(sigma_f0=x)` alone re-opens the gains with the same figures. "
    "Raises ValueError, changing nothing, outside the domain.\n"
    "\n"
    "A retune restarts the gain schedule; the estimates carry: the\n"
    "covariance restarts from the new steady state (scaled by the start\n"
    "sigma_f0 means), the gains follow it, and `integ` and `rate` are\n"
    "untouched. An argument of 0 keeps its current value (sigma_f0 0 is the\n"
    "steady state), so a call names only what changes. bn alone is the\n"
    "automatic case and may not be mixed with the figures. A call outside\n"
    "the domain (dp_state_filter_kalman_params_ok()) changes nothing.\n"
    "\n"
    "Three behaviors, all in the blob (q, r and t are reject keys, the start\n"
    "is P0):\n"
    "\n"
    "- The automatic case holds the figures its bandwidth implies, with\n"
    "  `sigma_e` 1 (the detector's rms as the unit) and `sigma_a` the square\n"
    "  root of the noise ratio that bandwidth selects; a figures-form call\n"
    "  that names one figure keeps the other at that implied value.\n"
    "- A call with bn replaces the stored figures by those the bandwidth\n"
    "  implies: a figures-form call that follows keeps the implied `r` and\n"
    "  `q`, not the ones the filter was built with.\n"
    "- A call with every argument 0 puts the gains at the steady state at\n"
    "  once and keeps the estimates: it ends a high-gain start early, once\n"
    "  the loop has pulled in, without changing the design. `configure (k,\n"
    "  0, 0, 0, f, 0)` with a frequency uncertainty f alone does the\n"
    "  opposite and re-opens the gains with the same figures, as after a\n"
    "  loss of lock.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sigma_e : float, default 0.0\n"
    "    RMS detector error when locked, 0 to keep.\n"
    "sigma_a : float, default 0.0\n"
    "    RMS change of the rate per update, 0 to keep.\n"
    "t : float, default 0.0\n"
    "    Update period in samples, 0 to keep.\n"
    "sigma_f0 : float, default 0.0\n"
    "    Frequency uncertainty, 0 for the steady state.\n"
    "bn : float, default 0.0\n"
    "    Noise bandwidth in cycles per sample (automatic form), 0 for the\n"
    "    figures form.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``configure refused: name sigma_e and sigma_a (and optionally\n"
    "    sigma_f0), or bn alone, with t positive and finite and the result\n"
    "    inside the domain; nothing changed``, with the return code appended\n"
    "    (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)\n"
    ">>> _ = k.steps(np.full(50, 0.1))\n"
    ">>> k.configure(bn=0.02)               # widen the loop, keep lock\n"
    ">>> round(k.integ, 4), round(k.bn, 4)\n"
    "(0.0016, 0.02)\n" },
  { "bandwidth", (PyCFunction)(void *)KalmanStateFilterObj_bandwidth,
    METH_VARARGS | METH_KEYWORDS,
    "bandwidth(delay, gain) -> float\n"
    "\n"
    "Exact delivered noise bandwidth of this loop AT THE GAINS IN FORCE NOW, "
    "in cycles per update, for a detector delay of `delay` updates and a loop "
    "gain of `gain` times nominal; the loop it would be if the gains stopped "
    "changing. NaN when that loop is not stable there.\n"
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
    "    Cycles per update, NaN where the frozen loop is not stable.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)\n"
    ">>> round(k.bandwidth(delay=1.0), 4)   # a detector delay of one update\n"
    "0.0103\n" },
  { "stable", (PyCFunction)(void *)KalmanStateFilterObj_stable,
    METH_VARARGS | METH_KEYWORDS,
    "stable(delay, gain) -> bool\n"
    "\n"
    "Whether the loop at the gains in force now is stable for a detector "
    "delay of `delay` updates and a loop gain of `gain` times nominal. A "
    "statement about frozen gains, not about the converging loop.\n"
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
    "    true where the frozen loop is stable.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> KalmanStateFilter(bn=0.01).stable(delay=2.0)\n"
    "True\n" },
  { "state_bytes", (PyCFunction)KalmanStateFilterObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the KalmanStateFilter has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)KalmanStateFilterObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the KalmanStateFilter has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)KalmanStateFilterObj_set_state, METH_O,
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
    "``RuntimeError`` if the KalmanStateFilter has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)KalmanStateFilterObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)KalmanStateFilterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a KalmanStateFilter be used in a `with` statement so its C\n"
    "resources are released deterministically on exit rather than at\n"
    "collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "KalmanStateFilter\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)KalmanStateFilterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the KalmanStateFilter.\n"
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

static PyTypeObject KalmanStateFilterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.track.KalmanStateFilter",
  .tp_basicsize                           = sizeof (KalmanStateFilterObject),
  .tp_dealloc = (destructor)KalmanStateFilterObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "The third-order loop filter whose gains come from a Kalman covariance "
    "recursion instead of being held: the loop of `StateFilter`, sized from "
    "the signal. Give the rms detector error when the loop is locked "
    "(`sigma_e`) and the rms change of the rate per update (`sigma_a`), in "
    "the loop's own units, or a bandwidth alone (`bn`); the steady-state "
    "bandwidth follows from their ratio. An optional initial frequency "
    "uncertainty (`sigma_f0`) gives a high-gain start that settles a "
    "frequency step about 1.7 times as fast, within a cap the constructor "
    "enforces. `step` and `steps` take loop errors and return control values; "
    "`kp`, `ki`, `kr` are the gains in force now.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sigma_e : float, default 0.0\n"
    "    RMS detector error when the loop is locked, in the loop's error "
    "units\n"
    "    per update. With `sigma_a`: the figures form. Leave 0 for the "
    "automatic\n"
    "    case (`bn`).\n"
    "sigma_a : float, default 0.0\n"
    "    RMS change of the rate per update, in the same units per update per\n"
    "    update. With `sigma_e`: the figures form.\n"
    "t : float, default 1.0\n"
    "    Update period in samples.\n"
    "sigma_f0 : float, default 0.0\n"
    "    Initial frequency uncertainty (figures form), 0 for the steady "
    "state:\n"
    "    at least the steady state's own frequency deviation and at most the\n"
    "    largest start whose gains stay inside the stability rectangle (5 "
    "times\n"
    "    it up to bn*t 0.04, less above).\n"
    "bn : float, default 0.0\n"
    "    The automatic case: the noise bandwidth in cycles per sample, with "
    "`t`,\n"
    "    and no `sigma_e`, `sigma_a` or `sigma_f0`. The steady-state gains "
    "of\n"
    "    the optimal filter with that exact bandwidth, from the first "
    "update.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is the reason the\n"
    "    constructor gives, or ``give sigma_e and sigma_a, or bn alone, in\n"
    "    range`` when it gives none.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import KalmanStateFilter\n"
    ">>> k = KalmanStateFilter(bn=0.01)             # the automatic case\n"
    ">>> round(k.bandwidth(), 6)\n"
    "0.01\n"
    ">>> k.stable()\n"
    "True\n"
    ">>> f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)   # the two "
    "figures\n"
    ">>> round(f.bandwidth(), 4)\n"
    "0.0904\n"
    ">>> KalmanStateFilter()                        # neither form\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: give sigma_e and sigma_a, or bn alone\n",
  .tp_methods = KalmanStateFilterObj_methods,
  .tp_getset  = KalmanStateFilter_getset,
  .tp_new     = KalmanStateFilterObj_new,
  .tp_init    = (initproc)KalmanStateFilterObj_init,
};
