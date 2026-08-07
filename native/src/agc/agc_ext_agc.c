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
AGC_steps (AGCObject *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
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
AGCObj_set_telemetry (AGCObject *self, PyObject *args, PyObject *kwds)
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
  int      _rc   = agc_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
AGCObj_state_bytes (AGCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (agc_state_bytes (self->handle));
}

static PyObject *
AGCObj_get_state (AGCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = agc_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  agc_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
AGCObj_set_state (AGCObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != agc_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (agc_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
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
static PyObject *
AGC_getprop_gain_update_period (AGCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->gain_update_period);
}
static int
AGC_setprop_gain_update_period (AGCObject *self, PyObject *value,
                                void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  unsigned long long v_raw = 0ULL;
  if (!PyArg_Parse (value, "K", &v_raw))
    return -1;
  size_t v                         = (size_t)v_raw;
  self->handle->gain_update_period = v;
  return 0;
}

static PyGetSetDef AGC_getset[]
    = { { "gain_db", (getter)AGC_getprop_gain_db, NULL, "Gain db.\n", NULL },
        { "applied_gain_db", (getter)AGC_getprop_applied_gain_db, NULL,
          "Return the gain (in dB) actually applied to the most recent "
          "sample. Computes 20*log10(g_last), where g_last is the linear "
          "multiplier that was used on the most recently processed sample.  "
          "This differs from gain_db (the loop integrator's current command) "
          "because the loop filter advances the command one step ahead after "
          "each sample: immediately after agc_step() gain_db already reflects "
          "the updated command while applied_gain_db still reflects what the "
          "signal actually saw.  At loop convergence the two values are "
          "numerically equal.  At create/reset both are 0.0 dB (unity).\n",
          NULL },
        { "ref_db", (getter)AGC_getprop_ref_db, (setter)AGC_setprop_ref_db,
          "Ref db.\n", NULL },
        { "loop_bw", (getter)AGC_getprop_loop_bw, (setter)AGC_setprop_loop_bw,
          "Loop bw.\n", NULL },
        { "alpha", (getter)AGC_getprop_alpha, (setter)AGC_setprop_alpha,
          "Alpha.\n", NULL },
        { "decim", (getter)AGC_getprop_decim, (setter)AGC_setprop_decim,
          "Emit every decim-th event, >= 1.\n", NULL },
        { "clip_db", (getter)AGC_getprop_clip_db, (setter)AGC_setprop_clip_db,
          "Clip db.\n", NULL },
        { "gain_update_period", (getter)AGC_getprop_gain_update_period,
          (setter)AGC_setprop_gain_update_period, "Gain update period.\n",
          NULL },
        { NULL } };

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
    "Reset the AGC loop state to its post-create condition. Sets gain_db back "
    "to 0 dB (unity), clears g_last, and re-seeds the power-detector EMA "
    "p_avg from the current ref_db so that the first post-reset block "
    "produces no transient.  All configuration fields (ref_db, loop_bw, "
    "alpha, decim, clip_db) are left untouched.  Use this to process a new, "
    "independent signal segment without re-allocating." },
  { "step", (PyCFunction)AGC_step, METH_VARARGS,
    "step(x) -> float complex\n"
    "\n"
    "Process one complex sample through the per-sample AGC loop. Applies the "
    "current gain, measures the output power via the EMA detector, advances "
    "the loop-filter integrator, then square-clips the returned sample to "
    "clip_db.  The clip is applied after the detector update, so clipping "
    "never disturbs convergence.  With the default gain_update_period == 1 "
    "this is the exact per-sample reference path; with gain_update_period P > "
    "1 the detector and gain-apply still run every sample but the loop-filter "
    "command (and the exp10/log10 it needs) refreshes once per P samples — a "
    "zero-order hold on the gain that amortises the transcendentals on a "
    "sample-rate hot loop, the streaming analogue of agc_steps()' decimation. "
    "agc_steps() is the faster block equivalent; neither is bit-identical to "
    "the P == 1 loop once decimated, but both converge to the same steady "
    "state.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    Complex input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    Gained, clipped output sample x * 10^(gain_db/20) with each\n"
    "    component independently clamped to +/-10^(clip_db/20).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.step(1.0+0.0j)   # unity gain at start, 0 dB in = 0 dB out\n"
    "(1+0j)\n"
    ">>> agc.gain_db           # loop already advanced from 0 dB\n"
    "0.0\n"
    ">>> agc2 = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample at unity gain\n"
    "(4+0j)\n"
    ">>> round(agc2.gain_db, 6)  # loop starts driving gain negative\n"
    "-0.024276\n" },
  { "steps", (PyCFunction)(void *)AGC_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of complex samples through the decimated AGC loop. "
    "Splits the input into chunks of decim samples.  Within each chunk the "
    "gain is linearly interpolated from the previous chunk's end value to the "
    "new loop-filter output (a first-order hold) so there is no inter-chunk "
    "gain staircase.  The detector and loop filter run once per chunk on the "
    "chunk's mean power — O(n/decim) control-loop work versus O(n) for "
    "agc_step().  The output array may alias the input (in-place).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.agc import AGC\n"
    ">>> import numpy as np\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))\n"
    ">>> round(agc.gain_db, 1)   # gain converged to -12 dB\n"
    "-12.0\n"
    ">>> x = np.full(8, 4.0+0.0j, dtype=np.complex64)\n"
    ">>> y = agc.steps(x)\n"
    ">>> y.shape, y.dtype\n"
    "((8,), dtype('complex64'))\n"
    ">>> [round(abs(v)**2, 2) for v in y.tolist()]  # output power ~1.0\n"
    "[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]\n"
    "\n" },

  { "set_telemetry", (PyCFunction)(void *)AGCObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the AGC's probes on "
    "it. Registers one probe, \"<prefix>.gain_db\" — the loop-filter "
    "integrator (the commanded gain in dB), recorded once per gain-update "
    "event and further thinned by decim.  Passing NULL detaches (probe sites "
    "revert to their single-branch disabled cost); re-attaching after a reset "
    "is idempotent (same name -> same probe id).  Setup path, never hot: call "
    "before the producer thread starts stepping, and keep every object "
    "attached to one context on that one thread (the ring is SPSC — see "
    "dp_tlm/dp_tlm_core.h).  The context is borrowed, not owned: it must "
    "outlive the attachment.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"agc\" or \"rx.agc\".\n"
    "decim : int\n"
    "    Emit every decim-th gain update; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.agc import AGC\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)\n"
    ">>> agc.set_telemetry(tlm, \"agc\")\n"
    ">>> tlm.probe_names\n"
    "{'agc.gain_db': 0}\n"
    ">>> x = (0.5 + 0j) * np.ones(256, dtype=np.complex64)\n"
    ">>> _ = agc.steps(x)\n"
    ">>> recs = tlm.read()          # one record per decim-chunk update\n"
    ">>> len(recs) == 256 // agc.decim\n"
    "True\n"
    ">>> bool(recs[\"value\"][-1] > recs[\"value\"][0])  # gain rises to ref\n"
    "True\n" },
  { "state_bytes", (PyCFunction)AGCObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the AGCObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)AGCObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the AGCObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)AGCObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the AGCObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)AGCObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)AGCObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Agc be used in a `with` statement so its C resources are "
    "released\n"
    "deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Agc\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)AGCObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Agc.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never "
    "suppresses\n"
    "one.\n"
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

static PyTypeObject AGCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "agc.AGC",
  .tp_basicsize                           = sizeof (AGCObject),
  .tp_dealloc                             = (destructor)AGCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Construct a log-domain feedback AGC and return its heap state. The loop "
    "integrator starts at 0 dB (unity gain) and the power detector p_avg is "
    "pre-seeded to 10^(ref_db/10) linear, so the first block of on-target "
    "samples produces no transient.  Three parameters tune the closed-loop "
    "behaviour: ref_db sets the target, loop_bw sets the convergence speed, "
    "and alpha sets the detector smoothing.\n",
  .tp_methods = AGCObj_methods,
  .tp_getset  = AGC_getset,
  .tp_new     = AGCObj_new,
  .tp_init    = (initproc)AGCObj_init,
};
