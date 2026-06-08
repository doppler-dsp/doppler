/*
 * agc_ext_agc.c — AGC type for the agc module.
 *
 * Included by agc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only agc_ext.c is compiled.
 */
/* ======================================================== */
/* AGCObject — wraps agc_state_t *       */
/* ======================================================== */

#include "agc/agc_core.h"

typedef struct
{
  PyObject_HEAD agc_state_t *handle;
} AGCObject;

static void
AGCObj_dealloc (AGCObject *self)
{
  if (self->handle)
    agc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
AGCObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  AGCObject *self = (AGCObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
AGCObj_init (AGCObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "ref_db", "loop_bw", "alpha", NULL };
  double       ref_db   = 0.0;
  double       loop_bw  = 0.0025;
  double       alpha    = 0.05;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ddd", kwlist, &ref_db,
                                    &loop_bw, &alpha))
    return -1;
  self->handle = agc_create (ref_db, loop_bw, alpha);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "agc_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
AGCObj_reset (AGCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  agc_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
AGC_step (AGCObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  float complex x = (float)x_raw.real + (float)x_raw.imag * I;
  float complex y = agc_step (self->handle, x);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}

static PyObject *
AGC_steps (AGCObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *in_obj  = NULL;
  PyObject *out_obj = NULL;
  if (!PyArg_ParseTuple (args, "O|O", &in_obj, &out_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      agc_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
                 (float complex *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  agc_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
             (float complex *)PyArray_DATA ((PyArrayObject *)out_arr),
             (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
AGC_getprop_gain_db (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->gain_db);
}
static PyObject *
AGC_getprop_applied_gain_db (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (agc_get_applied_gain_db (self->handle));
}
static PyObject *
AGC_getprop_ref_db (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->ref_db);
}
static int
AGC_setprop_ref_db (AGCObject *self, PyObject *value,
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
  self->handle->ref_db = v;
  return 0;
}
static PyObject *
AGC_getprop_loop_bw (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->loop_bw);
}
static int
AGC_setprop_loop_bw (AGCObject *self, PyObject *value,
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
  self->handle->loop_bw = v;
  return 0;
}
static PyObject *
AGC_getprop_alpha (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->alpha);
}
static int
AGC_setprop_alpha (AGCObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  self->handle->alpha = v;
  return 0;
}
static PyObject *
AGC_getprop_decim (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->decim);
}
static int
AGC_setprop_decim (AGCObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long long v_raw = 0ULL;
  if (!PyArg_Parse (value, "K", &v_raw))
    return -1;
  size_t v            = (size_t)v_raw;
  self->handle->decim = v;
  return 0;
}
static PyObject *
AGC_getprop_clip_db (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->clip_db);
}
static int
AGC_setprop_clip_db (AGCObject *self, PyObject *value,
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
  self->handle->clip_db = v;
  return 0;
}

static PyGetSetDef AGC_getset[] = {
  { "gain_db", (getter)AGC_getprop_gain_db, NULL,
    "Loop-filter integrator: the gain the loop currently commands, in dB.\n"
    "\n"
    "Starts at 0.0 dB (unity) at create/reset and is updated once per\n"
    "sample by agc_step() or once per decimation chunk by agc_steps().\n"
    "After convergence on a stationary input this equals the negative of\n"
    "the input level relative to ref_db.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.gain_db\n"
    "0.0\n"
    ">>> _ = agc.step(4.0+0.0j)\n"
    ">>> round(agc.gain_db, 6)   # loop started driving gain negative\n"
    "-0.024276\n",
    NULL },
  { "applied_gain_db", (getter)AGC_getprop_applied_gain_db, NULL,
    "Gain (dB) actually multiplied into the most recent output sample.\n"
    "\n"
    "Equal to 20*log10(g_last).  Differs from gain_db right after a step\n"
    "because the loop filter advances the command one step ahead; at\n"
    "convergence the two values are numerically equal.  Zero at\n"
    "create/reset.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.applied_gain_db\n"
    "0.0\n"
    ">>> _ = agc.step(4.0+0.0j)\n"
    ">>> agc.applied_gain_db   # 0 dB was used on that sample\n"
    "0.0\n"
    ">>> round(agc.gain_db, 6) # loop command already advanced\n"
    "-0.024276\n",
    NULL },
  { "ref_db", (getter)AGC_getprop_ref_db, (setter)AGC_setprop_ref_db,
    "Target output power in dB.  The loop drives measured output power\n"
    "toward this value.  Default 0.0 dB (unity power).  Writable at\n"
    "runtime; takes effect on the next sample processed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0)\n"
    ">>> agc.ref_db\n"
    "0.0\n"
    ">>> agc.ref_db = -10.0\n"
    ">>> agc.ref_db\n"
    "-10.0\n",
    NULL },
  { "loop_bw", (getter)AGC_getprop_loop_bw, (setter)AGC_setprop_loop_bw,
    "Loop noise bandwidth in cycles/sample.  Controls convergence speed:\n"
    "the loop settles in roughly 1/(4*loop_bw) samples.  Smaller values\n"
    "converge more slowly but track less noise.  Default 0.0025.  Keep\n"
    "well below 1/(4*decim) for loop stability when using steps().\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(loop_bw=0.0025)\n"
    ">>> agc.loop_bw\n"
    "0.0025\n"
    ">>> agc.loop_bw = 0.01\n"
    ">>> agc.loop_bw\n"
    "0.01\n",
    NULL },
  { "alpha", (getter)AGC_getprop_alpha, (setter)AGC_setprop_alpha,
    "Power-detector EMA coefficient in (0, 1].  Sets the detector\n"
    "bandwidth: smaller alpha smooths harder but reacts more slowly to\n"
    "envelope changes.  Default 0.05.  Writable at runtime.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(alpha=0.05)\n"
    ">>> agc.alpha\n"
    "0.05\n"
    ">>> agc.alpha = 0.2\n"
    ">>> agc.alpha\n"
    "0.2\n",
    NULL },
  { "decim", (getter)AGC_getprop_decim, (setter)AGC_setprop_decim,
    "Decimation factor for the control loop inside steps().  The detector\n"
    "and loop filter run once per chunk of this many samples; the gain is\n"
    "linearly interpolated within each chunk.  Default 8.  Useful values\n"
    "are 8, 16, and 32; must stay small relative to 1/(4*loop_bw).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC()\n"
    ">>> agc.decim\n"
    "8\n"
    ">>> agc.decim = 16\n"
    ">>> agc.decim\n"
    "16\n",
    NULL },
  { "clip_db", (getter)AGC_getprop_clip_db, (setter)AGC_setprop_clip_db,
    "Output square-clip level in dB per component.  Each output sample's\n"
    "real and imaginary parts are independently clamped to\n"
    "+/-10^(clip_db/20).  Applied after the power detector, so clipping\n"
    "never disturbs loop convergence.  Default 120.0 dB (effectively off).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC()\n"
    ">>> agc.clip_db\n"
    "120.0\n"
    ">>> agc.clip_db = 3.0    # limit each component to 10^(3/20) ~ 1.4125\n"
    ">>> round(agc.step(20.0+0.0j).real, 4)\n"
    "1.4125\n",
    NULL },
  { NULL }
};

static PyObject *
AGCObj_destroy (AGCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      agc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AGCObj_enter (AGCObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
AGCObj_exit (AGCObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      agc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef AGCObj_methods[] = {
  { "reset", (PyCFunction)AGCObj_reset, METH_NOARGS,
    "Reset the AGC loop state to its post-create condition.\n"
    "\n"
    "Sets gain_db to 0 dB (unity) and re-seeds the power-detector EMA\n"
    "p_avg from the current ref_db.  All configuration fields (ref_db,\n"
    "loop_bw, alpha, decim, clip_db) are preserved.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> import numpy as np\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))\n"
    ">>> round(agc.gain_db, 1)\n"
    "-12.0\n"
    ">>> agc.reset()\n"
    ">>> agc.gain_db, agc.applied_gain_db\n"
    "(0.0, 0.0)\n" },
  { "step", (PyCFunction)AGC_step, METH_VARARGS,
    "step(x) -> complex\n"
    "\n"
    "Process one complex input sample through the exact per-sample loop.\n"
    "\n"
    "Applies the current gain_db, updates the power detector, advances the\n"
    "loop filter by one step, then square-clips to clip_db.  The first\n"
    "sample always passes at whatever gain_db is on entry (0 dB at start).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    x * 10^(gain_db/20), each component clamped to clip_db.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.step(1.0+0.0j)   # unity gain at start, 0 dB in = 0 dB out\n"
    "(1+0j)\n"
    ">>> agc.gain_db           # loop advanced from 0 dB\n"
    "0.0\n"
    ">>> agc2 = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample at unity gain\n"
    "(4+0j)\n"
    ">>> round(agc2.gain_db, 6)  # loop driving gain negative\n"
    "-0.024276\n" },
  { "steps", (PyCFunction)AGC_steps, METH_VARARGS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of complex64 samples through the decimated AGC loop.\n"
    "\n"
    "Runs the detector and loop filter once per decim samples (first-order-\n"
    "hold gain interpolation within each chunk).  Equivalent to agc_step()\n"
    "at convergence but faster for long blocks.  The out array, if given,\n"
    "may alias x (in-place processing).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : ndarray of complex64\n"
    "    Input samples.\n"
    "out : ndarray of complex64, optional\n"
    "    Pre-allocated output buffer of the same length.  If omitted a new\n"
    "    array is allocated.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "ndarray of complex64\n"
    "    Gained and clipped output, same length as x.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> import numpy as np\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))\n"
    ">>> round(agc.gain_db, 1)   # converged to -12 dB\n"
    "-12.0\n"
    ">>> x = np.full(8, 4.0+0.0j, dtype=np.complex64)\n"
    ">>> y = agc.steps(x)\n"
    ">>> y.shape, y.dtype\n"
    "((8,), dtype('complex64'))\n"
    ">>> [round(abs(v)**2, 2) for v in y.tolist()]\n"
    "[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]\n" },

  { "destroy", (PyCFunction)AGCObj_destroy, METH_NOARGS,
    "Release C resources immediately.\n"
    "\n"
    "Frees the underlying agc_state_t.  Subsequent method calls raise\n"
    "RuntimeError.  Called automatically on garbage collection and on\n"
    "context-manager exit.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC()\n"
    ">>> agc.destroy()\n" },
  { "__enter__", (PyCFunction)AGCObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)AGCObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject AGCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "agc.AGC",
  .tp_basicsize                           = sizeof (AGCObject),
  .tp_dealloc                             = (destructor)AGCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Log-domain feedback AGC for complex baseband signals.\n"
    "\n"
    "Drives the average output power toward ref_db using a first-order\n"
    "dB-domain control loop.  Three stages run per sample: linear-in-dB\n"
    "gain apply, EMA power detector, and integrating loop filter.  The\n"
    "block method steps() decimates the control loop for efficiency while\n"
    "preserving convergence.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ref_db : float, default 0.0\n"
    "    Target output power in dB.\n"
    "loop_bw : float, default 0.0025\n"
    "    Loop noise bandwidth in cycles/sample; settles in ~1/(4*loop_bw)\n"
    "    samples.  Smaller is slower and smoother.\n"
    "alpha : float, default 0.05\n"
    "    Power-detector EMA coefficient in (0, 1].  Smaller smooths harder\n"
    "    but reacts more slowly to envelope changes.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.ref_db, agc.loop_bw, agc.alpha\n"
    "(0.0, 0.0025, 0.05)\n"
    ">>> agc.gain_db, agc.applied_gain_db\n"
    "(0.0, 0.0)\n"
    ">>> agc.decim, agc.clip_db\n"
    "(8, 120.0)\n",
  .tp_methods = AGCObj_methods,
  .tp_getset  = AGC_getset,
  .tp_new     = AGCObj_new,
  .tp_init    = (initproc)AGCObj_init,
};
