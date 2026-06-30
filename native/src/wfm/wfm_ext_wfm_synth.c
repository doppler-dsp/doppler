/*
 * wfm_ext_wfm_synth.c — _SynthEngine type for the wfmgen module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* _SynthEngineObject — wraps wfm_synth_state_t *       */
/* ======================================================== */

#include "wfm_synth/wfm_synth_core.h"

typedef struct
{
  PyObject_HEAD wfm_synth_state_t *handle;
} _SynthEngineObject;

static void
_SynthEngine_dealloc (_SynthEngineObject *self)
{
  if (self->handle)
    wfm_synth_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
_SynthEngine_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  _SynthEngineObject *self = (_SynthEngineObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
_SynthEngine_init (_SynthEngineObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "type", "snr_mode",  "fs",      "freq", "snr",   "seed",
          "sps",  "pn_length", "pn_poly", "lfsr", "f_end", NULL };
  const char        *type_str     = "tone";
  const char        *snr_mode_str = "auto";
  double             fs           = 1000000.0;
  double             freq         = 0.0;
  double             snr          = 100.0;
  unsigned long      seed_raw     = 0UL;
  int                sps          = 8;
  int                pn_length    = 7;
  unsigned long long pn_poly_raw  = 0ULL;
  const char        *lfsr_str     = "galois";
  double             f_end        = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ssdddkiiKsd", kwlist,
                                    &type_str, &snr_mode_str, &fs, &freq, &snr,
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
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "type must be one of \"tone\", \"noise\", \"pn\", "
                    "\"bpsk\", \"qpsk\", \"chirp\", \"bits\", got '%s'",
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
  int lfsr = 0;
  if (strcmp (lfsr_str, "galois") == 0)
    lfsr = 0;
  else if (strcmp (lfsr_str, "fibonacci") == 0)
    lfsr = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "lfsr must be \"galois\" or \"fibonacci\", got '%s'",
                    lfsr_str);
      return -1;
    }
  uint32_t seed    = (uint32_t)seed_raw;
  uint64_t pn_poly = (uint64_t)pn_poly_raw;
  self->handle = wfm_synth_create (type, fs, freq, snr, snr_mode, seed, sps,
                                   pn_length, pn_poly, lfsr, f_end);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "wfm_synth_create returned NULL");
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
  wfm_synth_reset (self->handle);
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
  float complex y = wfm_synth_step (self->handle);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}

static PyObject *
_SynthEngine_steps (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    return NULL;

  wfm_synth_steps (self->handle,
                   (float complex *)PyArray_DATA ((PyArrayObject *)out_arr),
                   (size_t)n);

  return out_arr;
}

/* set_rrc(taps) — enable RRC pulse shaping with the given real FIR taps
 * (e.g. doppler.wfm.rrc_taps(beta, sps, span)); no-op for non-modulated. */
static PyObject *
_SynthEngine_set_rrc (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *taps_obj = NULL;
  if (!PyArg_ParseTuple (args, "O", &taps_obj))
    return NULL;
  PyArrayObject *taps = (PyArrayObject *)PyArray_FROM_OTF (
      taps_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
  if (!taps)
    return NULL;
  size_t n = (size_t)PyArray_SIZE (taps);
  int rc = wfm_synth_set_rrc (self->handle, (const float *)PyArray_DATA (taps),
                              n);
  Py_DECREF (taps);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_rrc: empty taps or alloc failed");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* set_bits(pattern, modulation=1) — attach a user bit pattern to a type=bits
 * synth. pattern is any array-like of 0/1 (coerced to uint8); modulation is
 * 0=none, 1=bpsk, 2=qpsk. */
static PyObject *
_SynthEngine_set_bits (_SynthEngineObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject *pat_obj    = NULL;
  int       modulation = 1;
  if (!PyArg_ParseTuple (args, "O|i", &pat_obj, &modulation))
    return NULL;
  PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_OTF (
      pat_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!arr)
    return NULL;
  size_t n  = (size_t)PyArray_SIZE (arr);
  int    rc = wfm_synth_set_bits (
      self->handle, (const uint8_t *)PyArray_DATA (arr), n, modulation);
  Py_DECREF (arr);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "set_bits: empty pattern, modulation not in 0..2, or "
                       "not a bits synth");
      return NULL;
    }
  Py_RETURN_NONE;
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
  return PyLong_FromLong ((long)wfm_synth_get_wtype (self->handle));
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
  wfm_synth_set_wtype (self->handle, v);
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
  return PyLong_FromLong ((long)wfm_synth_get_nsps (self->handle));
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
  wfm_synth_set_nsps (self->handle, v);
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
  return PyLong_FromLong ((long)wfm_synth_get_sym_pos (self->handle));
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
  wfm_synth_set_sym_pos (self->handle, v);
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
  return PyFloat_FromDouble ((double)wfm_synth_get_cur_re (self->handle));
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
  wfm_synth_set_cur_re (self->handle, v);
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
  return PyFloat_FromDouble ((double)wfm_synth_get_cur_im (self->handle));
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
  wfm_synth_set_cur_im (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
_SynthEngine_destroy (_SynthEngineObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      wfm_synth_destroy (self->handle);
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
      wfm_synth_destroy (self->handle);
      self->handle = NULL;
    }
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
  return PyLong_FromSize_t (wfm_synth_state_bytes (self->handle));
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
  size_t    _n = wfm_synth_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  wfm_synth_get_state (self->handle, PyBytes_AS_STRING (_b));
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
  if ((size_t)PyBytes_GET_SIZE (arg) != wfm_synth_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (wfm_synth_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef _SynthEngine_methods[] = {
  { "reset", (PyCFunction)_SynthEngine_reset, METH_NOARGS,
    "Reset state to post-create defaults." },
  { "step", (PyCFunction)_SynthEngine_step, METH_NOARGS,
    "step() -> float complex\n"
    "\n"
    "Generate one output sample from internal state.\n"
    "\n"
    "    >>> from doppler import _SynthEngine\n"
    "    >>> obj = _SynthEngine(\"tone\", \"auto\", 1000000.0, 0.0, "
    "100.0, 1, 8, 7, "
    "0)\n"
    "    >>> obj.step()\n"
    "    0j\n" },
  { "steps", (PyCFunction)_SynthEngine_steps, METH_VARARGS,
    "steps(n=1) -> ndarray\n"
    "\n"
    "Generate n output samples.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import _SynthEngine\n"
    "    >>> obj = _SynthEngine(\"tone\", \"auto\", 1000000.0, 0.0, "
    "100.0, 1, 8, 7, "
    "0)\n"
    "    >>> y = obj.steps(4)\n"
    "    >>> y.shape\n"
    "    (4,)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },

  { "set_rrc", (PyCFunction)_SynthEngine_set_rrc, METH_VARARGS,
    "set_rrc(taps) -> None\n"
    "\n"
    "Enable RRC pulse shaping with real FIR taps (pn/bpsk/qpsk/bits).\n" },
  { "set_bits", (PyCFunction)_SynthEngine_set_bits, METH_VARARGS,
    "set_bits(pattern, modulation=1) -> None\n"
    "\n"
    "Attach a user bit pattern (array of 0/1) to a type='bits' synth.\n"
    "modulation: 0=none (0/1), 1=bpsk (+-1), 2=qpsk (2 bits/symbol).\n" },
  { "get_wtype", (PyCFunction)_SynthEngine_get_wtype, METH_NOARGS,
    "Get wtype." },
  { "set_wtype", (PyCFunction)_SynthEngine_set_wtype, METH_VARARGS,
    "Set wtype." },
  { "get_nsps", (PyCFunction)_SynthEngine_get_nsps, METH_NOARGS, "Get nsps." },
  { "set_nsps", (PyCFunction)_SynthEngine_set_nsps, METH_VARARGS,
    "Set nsps." },
  { "get_sym_pos", (PyCFunction)_SynthEngine_get_sym_pos, METH_NOARGS,
    "Get sym_pos." },
  { "set_sym_pos", (PyCFunction)_SynthEngine_set_sym_pos, METH_VARARGS,
    "Set sym_pos." },
  { "get_cur_re", (PyCFunction)_SynthEngine_get_cur_re, METH_NOARGS,
    "Get cur_re." },
  { "set_cur_re", (PyCFunction)_SynthEngine_set_cur_re, METH_VARARGS,
    "Set cur_re." },
  { "get_cur_im", (PyCFunction)_SynthEngine_get_cur_im, METH_NOARGS,
    "Get cur_im." },
  { "set_cur_im", (PyCFunction)_SynthEngine_set_cur_im, METH_VARARGS,
    "Set cur_im." },
  { "destroy", (PyCFunction)_SynthEngine_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)_SynthEngine_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)_SynthEngine_exit, METH_VARARGS, NULL },
  { "state_bytes", (PyCFunction)_SynthEngine_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)_SynthEngine_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)_SynthEngine_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { NULL }
};

static PyTypeObject _SynthEngineType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfmgen._SynthEngine",
  .tp_basicsize                           = sizeof (_SynthEngineObject),
  .tp_dealloc                             = (destructor)_SynthEngine_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and configure a waveform synthesiser. The synthesiser combines "
    "a local oscillator (LO), optional AWGN, and an optional PN LFSR into a "
    "single streaming source.  One call to wfm_synth_step() or "
    "wfm_synth_steps() "
    "advances all sub-components in lock-step. SNR >= WFM_SYNTH_SNR_CLEAN "
    "(100 "
    "dB) skips AWGN entirely — clean waveforms pay no noise overhead.  When "
    "``snr_mode`` is \"auto\" the library picks the natural reference: Es/No "
    "for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN.\n",
  .tp_methods = _SynthEngine_methods,
  .tp_new     = _SynthEngine_new,
  .tp_init    = (initproc)_SynthEngine_init,
};
