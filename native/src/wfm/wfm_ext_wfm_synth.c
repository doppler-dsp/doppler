/* jm:generated wfm_ext_wfm_synth.c */
/*
 * wfm_ext_wfm_synth.c — _SynthEngine type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in wfm_ext_wfm_synth_extra.c.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* _SynthEngineObject — wraps dp_wfm_synth_state_t *       */
/* ======================================================== */

#include "doppler/wfm_synth/wfm_synth_core.h"

typedef struct
{
  PyObject_HEAD dp_wfm_synth_state_t *handle;
} _SynthEngineObject;

static void
_SynthEngine_dealloc (_SynthEngineObject *self)
{
  if (self->handle)
    dp_wfm_synth_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
_SynthEngine_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  _SynthEngineObject *self = (_SynthEngineObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
_SynthEngine_init (_SynthEngineObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "type", "fs",        "freq",    "snr",  "snr_mode", "seed",
          "sps",  "pn_length", "pn_poly", "lfsr", "f_end",    NULL };
  const char        *type_str     = "tone";
  double             fs           = 1000000.0;
  double             freq         = 0.0;
  double             snr          = 100.0;
  const char        *snr_mode_str = "auto";
  unsigned long      seed_raw     = 1;
  int                sps          = 8;
  int                pn_length    = 7;
  unsigned long long pn_poly_raw  = 0;
  const char        *lfsr_str     = "galois";
  double             f_end        = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|sdddskiiKsd", kwlist,
                                    &type_str, &fs, &freq, &snr, &snr_mode_str,
                                    &seed_raw, &sps, &pn_length, &pn_poly_raw,
                                    &lfsr_str, &f_end))
    return -1;
  int type = 0;
  if (strcmp (type_str, "tone") == 0)
    type = 0;
  else if (strcmp (type_str, "noise") == 0)
    type = 1;
  else if (strcmp (type_str, "pn") == 0)
    type = 2;
  else if (strcmp (type_str, "bpsk") == 0)
    type = 3;
  else if (strcmp (type_str, "qpsk") == 0)
    type = 4;
  else if (strcmp (type_str, "chirp") == 0)
    type = 5;
  else if (strcmp (type_str, "bits") == 0)
    type = 6;
  else if (strcmp (type_str, "symbols") == 0)
    type = 7;
  else if (strcmp (type_str, "dsss") == 0)
    type = 8;
  else
    {
      PyErr_Format (
          PyExc_ValueError,
          "type must be one of \"tone\", \"noise\", \"pn\", \"bpsk\", "
          "\"qpsk\", \"chirp\", \"bits\", \"symbols\", \"dsss\", got '%s'",
          type_str);
      return -1;
    }
  int snr_mode = 0;
  if (strcmp (snr_mode_str, "auto") == 0)
    snr_mode = 0;
  else if (strcmp (snr_mode_str, "fs") == 0)
    snr_mode = 1;
  else if (strcmp (snr_mode_str, "ebno") == 0)
    snr_mode = 2;
  else if (strcmp (snr_mode_str, "esno") == 0)
    snr_mode = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "snr_mode must be one of \"auto\", \"fs\", \"ebno\", "
                    "\"esno\", got '%s'",
                    snr_mode_str);
      return -1;
    }
  uint32_t seed    = (uint32_t)seed_raw;
  uint64_t pn_poly = (uint64_t)pn_poly_raw;
  int      lfsr    = 0;
  if (strcmp (lfsr_str, "galois") == 0)
    lfsr = 0;
  else if (strcmp (lfsr_str, "fibonacci") == 0)
    lfsr = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
                    lfsr_str);
      return -1;
    }
  self->handle = dp_wfm_synth_create (type, fs, freq, snr, snr_mode, seed, sps,
                                      pn_length, pn_poly, lfsr, f_end);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_wfm_synth_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
_SynthEngine_reset (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_wfm_synth_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_step (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float _Complex y = dp_wfm_synth_step (self->handle);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}

static PyObject *
_SynthEngine_steps (_SynthEngineObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "n", NULL };
  Py_ssize_t   n        = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|n", kwlist, &n))
    return NULL;

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    return NULL;

  dp_wfm_synth_steps (
      self->handle, (float _Complex *)PyArray_DATA ((PyArrayObject *)out_arr),
      (size_t)n);

  return out_arr;
}

static PyObject *
_SynthEngine_get_wtype (_SynthEngineObject *self,
                        PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)dp_wfm_synth_get_wtype (self->handle));
}

static PyObject *
_SynthEngine_set_wtype (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  dp_wfm_synth_set_wtype (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_get_nsps (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)dp_wfm_synth_get_nsps (self->handle));
}

static PyObject *
_SynthEngine_set_nsps (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  dp_wfm_synth_set_nsps (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_get_sym_pos (_SynthEngineObject *self,
                          PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)dp_wfm_synth_get_sym_pos (self->handle));
}

static PyObject *
_SynthEngine_set_sym_pos (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  dp_wfm_synth_set_sym_pos (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_get_cur_re (_SynthEngineObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)dp_wfm_synth_get_cur_re (self->handle));
}

static PyObject *
_SynthEngine_set_cur_re (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float v = 0.0f;
  if (!PyArg_ParseTuple (args, "f", &v))
    return NULL;
  dp_wfm_synth_set_cur_re (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_get_cur_im (_SynthEngineObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble ((double)dp_wfm_synth_get_cur_im (self->handle));
}

static PyObject *
_SynthEngine_set_cur_im (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float v = 0.0f;
  if (!PyArg_ParseTuple (args, "f", &v))
    return NULL;
  dp_wfm_synth_set_cur_im (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
_SynthEngine_set_chirp_span (_SynthEngineObject *self, PyObject *args,
                             PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "span", NULL };
  unsigned long long span_raw  = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &span_raw))
    return NULL;
  size_t span = (size_t)span_raw;
  dp_wfm_synth_set_chirp_span (self->handle, span);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_state_bytes (_SynthEngineObject *self,
                          PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_wfm_synth_state_bytes (self->handle));
}

static PyObject *
_SynthEngine_get_state (_SynthEngineObject *self,
                        PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_wfm_synth_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_wfm_synth_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
_SynthEngine_set_state (_SynthEngineObject *self, PyObject *arg)
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
      != dp_wfm_synth_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_wfm_synth_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_destroy (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_wfm_synth_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_enter (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
_SynthEngine_exit (_SynthEngineObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_wfm_synth_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef _SynthEngine_methods[] = {
  { "reset", (PyCFunction)_SynthEngine_reset, METH_NOARGS,
    "Reset Synth to its post-create state. Resets the LO phase\n"
    "accumulator, AWGN internal state, and PN LFSR register to their initial\n"
    "values so the output sequence is perfectly reproducible from sample 0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> import numpy as np\n"
    ">>> s = _SynthEngine(type=\"qpsk\", sps=4, seed=1, snr=100.0)\n"
    ">>> a = s.steps(16).copy()\n"
    ">>> s.reset()\n"
    ">>> np.array_equal(a, s.steps(16))\n"
    "True\n" },
  { "step", (PyCFunction)_SynthEngine_step, METH_NOARGS,
    "step() -> float _Complex\n"
    "\n"
    "Generate one output sample from internal state. Advances the PN LFSR\n"
    "(modulated types only, on symbol boundaries), the LO phase accumulator,\n"
    "and the AWGN engine, then returns the mixed result: ``sym * carrier +\n"
    "noise``. Inlined and hot-path annotated so tight per-sample loops pay\n"
    "no call overhead.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    Next output sample (float _Complex).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type=\"tone\", fs=1.0, freq=0.0, snr=100.0)\n"
    ">>> s.step()\n"
    "(1+0j)\n"
    "\n" },
  { "steps", (PyCFunction)(void *)_SynthEngine_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(n=1) -> ndarray\n"
    "\n"
    "Generate a block of output samples. Calls dp_wfm_synth_step() in a\n"
    "tight loop, writing each cf32 sample into ``output``. The Python\n"
    "binding returns a freshly allocated NumPy complex64 array; ownership is\n"
    "transferred to the caller.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of samples to generate.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> import numpy as np\n"
    ">>> s = _SynthEngine(type=\"tone\", fs=1.0, freq=0.0, snr=100.0)\n"
    ">>> x = s.steps(4)\n"
    ">>> x.shape, x.dtype\n"
    "((4,), dtype('complex64'))\n"
    ">>> x.tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n"
    "\n" },

  { "get_wtype", (PyCFunction)_SynthEngine_get_wtype, METH_NOARGS,
    "Return the active waveform type discriminant. Maps to the WFM_SYNTH_* "
    "enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which "
    "synthesis path is active at runtime.\n" },
  { "set_wtype", (PyCFunction)_SynthEngine_set_wtype, METH_VARARGS,
    "Override the waveform type discriminant in-place. Changing wtype does "
    "not reinitialise sub-objects; use with care.\n" },
  { "get_nsps", (PyCFunction)_SynthEngine_get_nsps, METH_NOARGS,
    "Return the samples-per-symbol count. For modulated types (BPSK, QPSK, "
    "PN) each symbol is held for nsps consecutive output samples.  For "
    "tone/noise this field is present but unused by the synthesis path.\n" },
  { "set_nsps", (PyCFunction)_SynthEngine_set_nsps, METH_VARARGS,
    "Override the samples-per-symbol count in-place. Does not flush the "
    "symbol-position counter (sym_pos); set sym_pos=0 as well when changing "
    "sps mid-stream.\n" },
  { "get_sym_pos", (PyCFunction)_SynthEngine_get_sym_pos, METH_NOARGS,
    "Return the current position within the current symbol (0..nsps-1). "
    "Reaches nsps and wraps to 0 each time a new symbol is consumed from the "
    "PN LFSR.  Useful for frame alignment: sym_pos==0 on a step boundary "
    "means the very next sample begins a fresh symbol.\n" },
  { "set_sym_pos", (PyCFunction)_SynthEngine_set_sym_pos, METH_VARARGS,
    "Override the symbol-position counter in-place. Injecting 0 forces the "
    "next dp_wfm_synth_step() to latch a new PN chip; any other value "
    "fast-forwards into the middle of the current symbol hold.\n" },
  { "get_cur_re", (PyCFunction)_SynthEngine_get_cur_re, METH_NOARGS,
    "Return the real part of the current held symbol. For modulated types "
    "this is the I component latched at the last symbol boundary (±1 for "
    "BPSK/PN, ±1/√2 for QPSK).  For tone the synthesiser initialises cur_re "
    "to 1.0 so that the held symbol is a clean unit-power carrier; for noise "
    "it is 0.0 (noise has no held symbol).\n" },
  { "set_cur_re", (PyCFunction)_SynthEngine_set_cur_re, METH_VARARGS,
    "Override the held-symbol real (I) component in-place. Takes effect on "
    "the next dp_wfm_synth_step() within the current symbol hold.\n" },
  { "get_cur_im", (PyCFunction)_SynthEngine_get_cur_im, METH_NOARGS,
    "Return the imaginary part of the current held symbol. For QPSK this is "
    "the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it "
    "is 0.\n" },
  { "set_cur_im", (PyCFunction)_SynthEngine_set_cur_im, METH_VARARGS,
    "Override the held-symbol imaginary (Q) component in-place. Takes effect "
    "on the next dp_wfm_synth_step() within the current symbol hold.\n" },
  { "set_chirp_span", (PyCFunction)(void *)_SynthEngine_set_chirp_span,
    METH_VARARGS | METH_KEYWORDS,
    "set_chirp_span(span) -> None\n"
    "\n"
    "Pin a chirp's sweep to `span` samples (no-op for non-chirp). Only the "
    "first non-zero pin takes effect; until then a chirp holds its start "
    "frequency on step() and steps() alike.\n"
    "\n"
    "A linear chirp's slope is `(f_end − f_start) / span`, so the span — the\n"
    "number of samples the sweep occupies — must be known before generation.\n"
    "The composer calls this with the source's declared span or the segment\n"
    "length. A synth that is never pinned does not sweep: it holds the start\n"
    "frequency on dp_wfm_synth_step() and dp_wfm_synth_steps() alike, so the\n"
    "waveform never depends on how reads are chunked. Only the first pin\n"
    "(while the span is still 0) takes effect, so it is safe to call\n"
    "unconditionally after dp_wfm_synth_create(); span 0 is a no-op.\n"
    "\n"
    "The span is configuration, not running state: dp_wfm_synth_get_state()\n"
    "does not carry it, so pin a resumed instance exactly as the original\n"
    "was pinned.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "span : int\n"
    "    Sweep length in samples (> 0).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler.wfm import _SynthEngine\n"
    "    >>> obj = _SynthEngine(type=\"tone\", fs=1000000.0, freq=0.0, "
    "snr=100.0, snr_mode=\"auto\", seed=1, sps=8, pn_length=7, pn_poly=0, "
    "lfsr=\"galois\", f_end=0.0)\n"
    "    >>> obj.set_chirp_span(0)\n" },
  { "state_bytes", (PyCFunction)_SynthEngine_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the _SynthEngine has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)_SynthEngine_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the _SynthEngine has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)_SynthEngine_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the _SynthEngine has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "set_rrc", (PyCFunction)(void (*) (void))_SynthEngine_set_rrc,
    METH_VARARGS,
    "Shape the symbols with a root-raised-cosine pulse.\n"
    "\n"
    "Replaces the default rectangular hold: the symbol-rate impulse train\n"
    "is filtered by ``taps``, a real FIR, typically\n"
    "``rrc_taps(beta, sps, span)``. The taps are scaled by ``sqrt(sps)``\n"
    "inside, for unit transmit power, so pass them raw. Applies to the\n"
    "types with a symbol stream (``pn``, ``bpsk``, ``qpsk``, ``bits``,\n"
    "``symbols``, ``dsss``) and is a no-op for ``tone``, ``noise`` and\n"
    "``chirp``. Replaces any earlier shaper and clears its delay line.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "taps : array_like\n"
    "    Real FIR taps, coerced to float32 and copied.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``taps`` is empty.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import _SynthEngine, rrc_taps\n"
    ">>> s = _SynthEngine(type='bpsk', sps=4, seed=1, snr=100.0)\n"
    ">>> s.set_rrc(rrc_taps(0.35, 4, 4))\n"
    ">>> y = s.steps(8192)\n"
    ">>> round(float(np.mean(np.abs(y) ** 2)), 1)  # unit power\n"
    "1.0\n" },
  { "set_bits", (PyCFunction)(void (*) (void))_SynthEngine_set_bits,
    METH_VARARGS,
    "Attach a bit pattern to a ``type='bits'`` synth.\n"
    "\n"
    "The pattern is mapped to symbols by ``modulation``, held for ``sps``\n"
    "samples each, and sent ONCE: one pass is ``len(pattern) * sps``\n"
    "samples (half that for qpsk), and the output is silent after it.\n"
    "Replaces any earlier pattern and resets the read position. A no-op\n"
    "on any other type.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "pattern : array_like\n"
    "    Bits, 0/1, coerced to uint8 and copied.\n"
    "modulation : int, default 1\n"
    "    0 = none (0/1 amplitude), 1 = bpsk (0 -> +1, 1 -> -1),\n"
    "    2 = qpsk (Gray-coded +-1/sqrt(2), two bits a symbol).\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``pattern`` is empty or ``modulation`` is not 0, 1 or 2.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type='bits', sps=1, snr=100.0)\n"
    ">>> s.set_bits([0, 1, 1, 0])\n"
    ">>> s.steps(6).real.tolist()\n"
    "[1.0, -1.0, -1.0, 1.0, 0.0, 0.0]\n" },
  { "set_symbols", (PyCFunction)(void (*) (void))_SynthEngine_set_symbols,
    METH_VARARGS,
    "Attach a complex-symbol stream to a ``type='symbols'`` synth.\n"
    "\n"
    "Each element is the constellation point itself, with no bit mapping,\n"
    "so any modulation is \"compute the symbols, pass them in\". The stream\n"
    "is held for ``sps`` samples a symbol and cycled for as long as\n"
    "``steps()`` asks, and is RRC-shaped once ``set_rrc()`` is active.\n"
    "Replaces any earlier stream and resets the read position. A no-op on\n"
    "any other type.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "symbols : array_like\n"
    "    Complex symbols, coerced to complex64 and copied.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``symbols`` is empty.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type='symbols', sps=2, snr=100.0)\n"
    ">>> s.set_symbols([1 + 1j, -1 - 1j])\n"
    ">>> s.steps(6).tolist()\n"
    "[(1+1j), (1+1j), (-1-1j), (-1-1j), (1+1j), (1+1j)]\n" },
  { "set_dsss_chips",
    (PyCFunction)(void (*) (void))_SynthEngine_set_dsss_chips, METH_O,
    "Attach an already-assembled DSSS burst to a ``type='dsss'`` synth.\n"
    "\n"
    "One chip per element, BPSK-mapped (0 -> +1, 1 -> -1), held for\n"
    "``sps`` samples a chip and sent once, then silence. Assemble the\n"
    "burst from a frame description: the unspread preamble, then every\n"
    "bit of a ``Frame``'s ``bits()`` spread by the data code. Replaces\n"
    "any earlier burst and resets the read position. A no-op on any other\n"
    "type.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "chips : array_like\n"
    "    Chips, 0/1, coerced to uint8 and copied.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``chips`` is empty.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type='dsss', sps=2, snr=100.0)\n"
    ">>> s.set_dsss_chips([0, 1, 1])\n"
    ">>> s.steps(8).real.tolist()\n"
    "[1.0, 1.0, -1.0, -1.0, -1.0, -1.0, 0.0, 0.0]\n" },
  { "set_dsss_cont", (PyCFunction)(void (*) (void))_SynthEngine_set_dsss_cont,
    METH_VARARGS | METH_KEYWORDS,
    "Switch a ``type='dsss'`` synth to continuous asynchronous output.\n"
    "\n"
    "The spreading ``code`` repeats endlessly and the data rides on it at\n"
    "``chips_per_symbol`` chips a symbol; a non-integer value is the\n"
    "normal, asynchronous case. Chips are generated per sample, so the\n"
    "stream has no length to pick. A no-op on any other type.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : array_like\n"
    "    Spreading-code chips, 0/1, coerced to uint8 and copied.\n"
    "chips_per_symbol : float\n"
    "    ``chip_rate / symbol_rate``; at least 1.\n"
    "data : {'none', 'prbs', 'bits'}, default 'prbs'\n"
    "    The symbol source: ``'none'`` sends the pure code, ``'prbs'``\n"
    "    the synth's seeded PN (a receiver regenerates it with\n"
    "    ``doppler.wfm.PN``), ``'bits'`` the ``payload``, one bit a\n"
    "    symbol, sent once and then silence.\n"
    "payload : array_like, optional\n"
    "    Payload bits, 0/1. Supplying it selects ``'bits'``.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If ``data`` is not one of the three, or the geometry is invalid:\n"
    "    an empty code, ``chips_per_symbol < 1``, ``'bits'`` with no\n"
    "    payload, or ``'prbs'`` with an invalid ``pn_length``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type='dsss', sps=1, snr=100.0)\n"
    ">>> s.set_dsss_cont([0, 1, 1], 3.0, data='bits', payload=[1, 0])\n"
    ">>> s.steps(8).real.tolist()\n"
    "[-1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 0.0, 0.0]\n" },
  { "set_dsss_window",
    (PyCFunction)(void (*) (void))_SynthEngine_set_dsss_window,
    METH_VARARGS | METH_KEYWORDS,
    "Give the continuous DSSS stream a frame with a pure-code window.\n"
    "\n"
    "The frame is on the data clock: of every ``frame_symbols`` symbols,\n"
    "the first ``code_only_symbols`` carry the pure code and no data, and\n"
    "the rest carry the payload, running on across frames. The symbol\n"
    "clock free-runs through the window, so a frame edge lands at no\n"
    "particular chip phase. Configuration, not running state: ``reset()``\n"
    "keeps it. The order against ``set_dsss_cont()`` does not matter. A\n"
    "no-op on any other type.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code_only_symbols : int\n"
    "    Pure-code symbols opening each frame, at most ``frame_symbols``.\n"
    "frame_symbols : int\n"
    "    Frame length in symbols; 0 is no window at all.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If either count is negative, or ``code_only_symbols`` exceeds a\n"
    "    non-zero ``frame_symbols``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> s = _SynthEngine(type='dsss', sps=1, snr=100.0)\n"
    ">>> s.set_dsss_cont([0, 1, 1], 3.0, data='bits', payload=[1, 1])\n"
    ">>> s.set_dsss_window(1, 2)\n"
    ">>> s.steps(12).real.tolist()\n"
    "[1.0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, 1.0, 1.0]\n" },
  { "destroy", (PyCFunction)_SynthEngine_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)_SynthEngine_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a _SynthEngine be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "_SynthEngine\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)_SynthEngine_exit, METH_VARARGS,
    "Exit a context manager, releasing the _SynthEngine.\n"
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

static PyTypeObject _SynthEngineType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wfm._SynthEngine",
  .tp_basicsize                           = sizeof (_SynthEngineObject),
  .tp_dealloc                             = (destructor)_SynthEngine_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and configure a waveform synthesiser. The synthesiser combines\n"
    "a local oscillator (LO), optional AWGN, and an optional PN LFSR into a\n"
    "single streaming source. One call to dp_wfm_synth_step() or\n"
    "dp_wfm_synth_steps() advances all sub-components in lock-step. SNR >=\n"
    "WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay "
    "no\n"
    "noise overhead. When ``snr_mode`` is \"auto\" the library picks the "
    "natural\n"
    "reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for\n"
    "tone/noise/PN.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "type : Literal[\"tone\", \"noise\", \"pn\", \"bpsk\", \"qpsk\", "
    "\"chirp\", \"bits\", \"symbols\", \"dsss\"], default \"tone\"\n"
    "    Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk, 5=chirp, "
    "6=bits,\n"
    "    7=symbols, 8=dsss. The Python binding accepts strings\n"
    "    "
    "\"tone\"|\"noise\"|\"pn\"|\"bpsk\"|\"qpsk\"|\"chirp\"|\"bits\"|"
    "\"symbols\"|\"dsss\". For\n"
    "    \"bits\" attach the pattern with dp_wfm_synth_set_bits(); for "
    "\"symbols\"\n"
    "    attach the complex stream with dp_wfm_synth_set_symbols(); for "
    "\"dsss\"\n"
    "    attach the burst with dp_wfm_synth_set_dsss_chips() after create().\n"
    "fs : float, default 1000000.0\n"
    "    Sample rate in Hz. Sets the carrier frequency normalisation and the\n"
    "    noise bandwidth. Default 1 000 000.0.\n"
    "freq : float, default 0.0\n"
    "    Carrier frequency offset in Hz (−fs/2 … fs/2). A complex LO is "
    "created\n"
    "    only when freq != 0. For a chirp this is the start frequency "
    "f_start\n"
    "    (the instantaneous frequency at t=0). Default 0.0.\n"
    "snr : float, default 100.0\n"
    "    Target SNR in dB, interpreted per ``snr_mode``. Values >=\n"
    "    WFM_SYNTH_SNR_CLEAN (100) disable AWGN. Default 100.0.\n"
    "snr_mode : Literal[\"auto\", \"fs\", \"ebno\", \"esno\"], default "
    "\"auto\"\n"
    "    SNR reference: 0=auto, 1=fs (full-band), 2=ebno, 3=esno. The Python\n"
    "    binding accepts strings \"auto\"|\"fs\"|\"ebno\"|\"esno\". Default "
    "0.\n"
    "seed : int, default 1\n"
    "    PRNG seed shared by AWGN and the PN LFSR. Default 1.\n"
    "sps : int, default 8\n"
    "    Samples per symbol for modulated types (BPSK, QPSK, PN). Ignored "
    "for\n"
    "    tone/noise. Default 8.\n"
    "pn_length : int, default 7\n"
    "    LFSR register length (1..64); period = 2^pn_length - 1. Default 7\n"
    "    (period 127).\n"
    "pn_poly : int, default 0\n"
    "    Galois tap polynomial for the LFSR. 0 means \"look up the canonical "
    "MLS\n"
    "    polynomial for pn_length\" from the wfm_synth_mls_poly table. "
    "Default 0.\n"
    "lfsr : Literal[\"galois\", \"fibonacci\"], default \"galois\"\n"
    "    LFSR realization: PN_GALOIS (0) or PN_FIBONACCI (1).\n"
    "f_end : float, default 0.0\n"
    "    Chirp end frequency in Hz (type=chirp only; ignored otherwise). "
    "With\n"
    "    ``freq`` as the start, the instantaneous frequency sweeps linearly "
    "from\n"
    "    ``freq`` to ``f_end`` over the span set by\n"
    "    dp_wfm_synth_set_chirp_span(), then holds at ``f_end``. Until a span "
    "is\n"
    "    pinned the slope is 0 (a CW tone at ``freq``). ``f_end < freq`` is "
    "a\n"
    "    down-chirp. Default 0.0.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import _SynthEngine\n"
    ">>> import numpy as np\n"
    ">>> s = _SynthEngine(type=\"tone\", fs=1.0, freq=0.0, snr=100.0)\n"
    ">>> x = s.steps(4)\n"
    ">>> x.dtype\n"
    "dtype('complex64')\n"
    ">>> x.tolist()\n"
    "[(1+0j), (1+0j), (1+0j), (1+0j)]\n",
  .tp_methods = _SynthEngine_methods,
  .tp_new     = _SynthEngine_new,
  .tp_init    = (initproc)_SynthEngine_init,
};
