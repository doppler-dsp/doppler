/*
 * track_ext_dll.c — Dll type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* DllObject — wraps dll_state_t *       */
/* ======================================================== */

#include "dll/dll_core.h"

typedef struct
{
  PyObject_HEAD dll_state_t *handle;
} DllObject;

static void
DllObj_dealloc (DllObject *self)
{
  if (self->handle)
    dll_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DllObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DllObject *self = (DllObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DllObj_init (DllObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]     = { "code", "sps",     "init_chip", "bn",
                                      "zeta", "spacing", "segments",  NULL };
  PyObject          *code_obj     = NULL;
  unsigned long long sps_raw      = 2;
  double             init_chip    = 0.0;
  double             bn           = 0.01;
  double             zeta         = 0.707;
  double             spacing      = 0.5;
  unsigned long long segments_raw = 1;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|KddddK", kwlist, &code_obj,
                                    &sps_raw, &init_chip, &bn, &zeta, &spacing,
                                    &segments_raw))
    return -1;
  size_t         sps      = (size_t)sps_raw;
  size_t         segments = (size_t)segments_raw;
  PyArrayObject *code_arr = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle
      = dll_create ((const uint8_t *)PyArray_DATA (code_arr), code_len, sps,
                    init_chip, bn, zeta, spacing, segments);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dll_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
DllObj_steps_max_out (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dll_steps_max_out (self->handle));
}

static PyObject *
DllObj_steps (DllObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = dll_steps_max_out (self->handle);
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
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex       *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = dll_steps (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
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
  size_t _cap  = dll_steps_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = dll_steps (self->handle, _ng0, _ng1, _d0, _cap);
  Py_END_ALLOW_THREADS
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
DllObj_set_telemetry (DllObject *self, PyObject *args, PyObject *kwds)
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
  int      _rc   = dll_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DllObj_configure (DllObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "bn", "zeta", NULL };
  double       bn        = 0.0;
  double       zeta      = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dd", _kwlist, &bn, &zeta))
    return NULL;
  dll_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
DllObj_set_rate_aid (DllObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "rate_aid", NULL };
  double       rate_aid  = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &rate_aid))
    return NULL;
  dll_set_rate_aid (self->handle, rate_aid);
  Py_RETURN_NONE;
}

static PyObject *
DllObj_configure_lock (DllObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[]   = { "pfa", "n_looks", "ref_snr_db", NULL };
  double             pfa         = 0.0;
  unsigned long long n_looks_raw = 0ULL;
  double             ref_snr_db  = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dK|d", _kwlist, &pfa,
                                    &n_looks_raw, &ref_snr_db))
    return NULL;
  size_t n_looks = (size_t)n_looks_raw;
  int    _rc     = dll_configure_lock (self->handle, pfa, n_looks, ref_snr_db);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "configure_lock failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DllObj_configure_lock_raw (DllObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]   = { "up_thresh", "down_thresh", "n_looks", "alpha",
                               "n_up",      "n_down",      NULL };
  double       up_thresh   = 0.0;
  double       down_thresh = 0.0;
  unsigned long long n_looks_raw = 0ULL;
  double             alpha       = 0.0;
  unsigned long      n_up_raw    = 0UL;
  unsigned long      n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddKdkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_looks_raw, &alpha,
                                    &n_up_raw, &n_down_raw))
    return NULL;
  size_t   n_looks = (size_t)n_looks_raw;
  uint32_t n_up    = (uint32_t)n_up_raw;
  uint32_t n_down  = (uint32_t)n_down_raw;
  dll_configure_lock_raw (self->handle, up_thresh, down_thresh, n_looks, alpha,
                          n_up, n_down);
  Py_RETURN_NONE;
}

static PyObject *
DllObj_reset (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dll_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DllObj_state_bytes (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dll_state_bytes (self->handle));
}

static PyObject *
DllObj_get_state (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dll_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dll_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DllObj_set_state (DllObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dll_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dll_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Dll_getprop_bn (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_bn (self->handle));
}
static int
Dll_setprop_bn (DllObject *self, PyObject *value, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return -1;
    }
  double v = 0.0;
  if (!PyArg_Parse (value, "d", &v))
    return -1;
  dll_set_bn (self->handle, v);
  return 0;
}
static PyObject *
Dll_getprop_code_phase (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_code_phase (self->handle));
}
static PyObject *
Dll_getprop_code_rate (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_code_rate (self->handle));
}
static PyObject *
Dll_getprop_last_error (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_last_error (self->handle));
}
static PyObject *
Dll_getprop_segments (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)dll_get_segments (self->handle));
}
static PyObject *
Dll_getprop_locked (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(dll_get_locked (self->handle)));
}
static PyObject *
Dll_getprop_lock_stat (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_lock_stat (self->handle));
}
static PyObject *
Dll_getprop_noise_est (DllObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (dll_get_noise_est (self->handle));
}

static PyGetSetDef Dll_getset[] = {
  { "bn", (getter)Dll_getprop_bn, (setter)Dll_setprop_bn,
    "loop noise bandwidth (retained).\n", NULL },
  { "code_phase", (getter)Dll_getprop_code_phase, NULL, "Code phase.\n",
    NULL },
  { "code_rate", (getter)Dll_getprop_code_rate, NULL,
    "chips advanced per nominal chip (~1.0).\n", NULL },
  { "last_error", (getter)Dll_getprop_last_error, NULL,
    "last discriminator output (loop stress).\n", NULL },
  { "segments", (getter)Dll_getprop_segments, NULL,
    "partial correlations per epoch (1 = full).\n", NULL },
  { "locked", (getter)Dll_getprop_locked, NULL,
    "Current lock decision: True after the verify count of consecutive "
    "above-threshold N-look decisions, False again after the drop count of "
    "consecutive below-threshold ones (see configure_lock).\n",
    NULL },
  { "lock_stat", (getter)Dll_getprop_lock_stat, NULL,
    "Last code-lock test statistic R = sqrt(2*sum|P|^2 / E|O|^2); compare "
    "against det_threshold_noncoherent(pfa, n_looks).\n",
    NULL },
  { "noise_est", (getter)Dll_getprop_noise_est, NULL,
    "Current CFAR noise-power estimate E|O|^2 from the off-peak (noise) tap "
    "EMA.\n",
    NULL },
  { NULL }
};

static PyObject *
DllObj_destroy (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dll_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DllObj_enter (DllObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DllObj_exit (DllObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dll_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DllObj_methods[] = {

  { "steps", (PyCFunction)(void *)DllObj_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "Correlate a cf32 block against the local code with early/prompt/late "
    "taps and steer the code NCO each code period on the non-coherent "
    "(sum|E|-sum|L|)/(sum|E|+sum|L|) discriminator. With segments=1 (default) "
    "this is a coherent full-epoch integrate-and-dump: one prompt symbol per "
    "period. With segments>1 each epoch is split into that many sub-epoch "
    "partial correlations: it emits that many partial prompts per period (a "
    "stream at ~segments samples/symbol when the symbol rate is near the code "
    "rate) and tracks the code non-coherently across the partials, which a "
    "data flip cannot collapse (robust to an asynchronous data-symbol clock). "
    "segments>1 is the streaming despreader: it removes the PN code and "
    "outputs samples. The non-coherent loop is carrier-blind, so it tracks "
    "with a residual carrier still on the input; carrier recovery (Costas) "
    "and symbol-timing recovery (SymbolSync) are downstream stages fed from "
    "the partial output. Returned blocks are safe to keep across calls "
    "(block-size invariant): a block whose array is still referenced is never "
    "overwritten by a later call (jm gh-437).\n"
    "\n"
    "The Python face of the loop. Each code period the early/prompt/late\n"
    "correlators dump, the power-domain non-coherent early-minus-late\n"
    "discriminator runs, and the fixed-point code-phase NCO is re-steered;\n"
    "the prompt correlator value is emitted as one output symbol per period\n"
    "(or `segments` partial prompts per period when `segments > 1`). The "
    "loop\n"
    "is carrier-blind — it tracks with a residual carrier still on the "
    "input,\n"
    "so carrier recovery (Costas) and symbol-timing recovery are downstream\n"
    "stages fed from this output. Returned blocks are block-size invariant\n"
    "and safe to keep across calls (a block still referenced is never\n"
    "overwritten, jm gh-437).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Carrier-wiped input samples (one contiguous block).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of prompt symbols written — one per completed code period\n"
    "    (`segments` per period when `segments > 1`) — up to max_out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> chip = np.where(code & 1, -1.0, 1.0)    # BPSK spreading code\n"
    ">>> x = np.tile(np.repeat(chip, 2), 40).astype(np.complex64)\n"
    ">>> d = Dll(code=code, sps=2)\n"
    ">>> sym = d.steps(x)                        # one prompt per period\n"
    ">>> sym.dtype\n"
    "dtype('complex64')\n"
    ">>> round(float(np.mean(sym.real[-10:])), 1)  # despread to a clean +1\n"
    "1.0\n"
    ">>> round(d.code_rate, 3)                   # locked at nominal rate\n"
    "1.0\n" },
  { "steps_max_out", (PyCFunction)DllObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)DllObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the code loop's "
    "probes on it. Registers four probes, emitted once per code epoch "
    "(period) and further thinned by decim: \"<prefix>.e\" (the "
    "early-minus-late envelope discriminator — the loop stress), "
    "\"<prefix>.rate\" (the tracked code rate, chips advanced per nominal "
    "chip, ~1.0 at lock), \"<prefix>.lock\" (the CFAR lock statistic R; "
    "compare against the configured threshold) and \"<prefix>.locked\" (the "
    "verify-counted lock decision, 0/1 — the lockdet output, so a consumer "
    "sees where the declare/drop rule fired without re-deriving it from the "
    "statistic).  Passing NULL detaches. Setup path, never hot: call before "
    "the producer thread starts stepping; the context is borrowed and must "
    "outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"code\" or \"ch0.code\".\n"
    "decim : int\n"
    "    Emit every decim-th epoch; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> code = np.zeros(31, dtype=np.uint8)\n"
    ">>> d = Dll(code=code, sps=2)\n"
    ">>> d.set_telemetry(tlm, \"code\")\n"
    ">>> sorted(tlm.probe_names)\n"
    "['code.e', 'code.lock', 'code.locked', 'code.rate']\n"
    ">>> x = np.ones(31 * 2 * 50, dtype=np.complex64)\n"
    ">>> _ = d.steps(x)\n"
    ">>> recs = tlm.read()   # four records per code epoch\n"
    ">>> len(recs) > 0 and len(recs) % 4 == 0\n"
    "True\n" },
  { "configure", (PyCFunction)(void *)DllObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the code "
    "phase/rate.\n"
    "\n"
    "Re-derives the 2nd-order loop filter's proportional and integral gains\n"
    "for a new noise bandwidth and damping, leaving the tracked code phase,\n"
    "code rate and correlator accumulators untouched — retune the loop\n"
    "mid-run (e.g. narrow the bandwidth once pulled in) without dropping\n"
    "lock.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalised to the code-period rate.\n"
    "zeta : float\n"
    "    Damping factor (0.707 = critically damped).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
    ">>> d = Dll(code=code, sps=2, bn=0.01)\n"
    ">>> d.configure(bn=0.02, zeta=0.707)   # widen the bandwidth mid-run\n"
    ">>> round(d.bn, 3)\n"
    "0.02\n" },
  { "set_rate_aid", (PyCFunction)(void *)DllObj_set_rate_aid,
    METH_VARARGS | METH_KEYWORDS,
    "set_rate_aid(rate_aid) -> None\n"
    "\n"
    "Set the carrier-aiding code-rate deviation (ratio; 0 = off): a fixed "
    "fractional rate bias summed into the code NCO's phase_inc every epoch, "
    "on top of the loop's own control. For physically-coupled Doppler, pass "
    "carrier_offset_hz / carrier_freq_hz so the code NCO rides the code-rate "
    "dilation the discriminator alone can't pull in at low SNR. Applied "
    "continuously across the epoch (not a phase pulse), and nudges the "
    "current phase_inc so the aid takes effect before the first period "
    "update. code_rate stays the loop's own observable and is unaffected.\n"
    "\n"
    "A fixed fractional rate bias summed into the sample-and-hold "
    "`phase_inc`\n"
    "on top of the loop's own control every epoch -- for physically-coupled\n"
    "Doppler, `carrier_offset_hz / carrier_freq_hz`, so the code NCO rides\n"
    "the code-rate dilation the discriminator alone can't pull in at low "
    "SNR.\n"
    "Applied continuously across the epoch (via `phase_inc`), not as a phase\n"
    "pulse. Also nudges the current `phase_inc` so the aid takes effect\n"
    "before the first period update. `code_rate` stays the loop's own\n"
    "observable and is unaffected.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "rate_aid : float\n"
    "    Fractional code-rate deviation (e.g. 8e-6). 0 disables.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> rng = np.random.default_rng(11)\n"
    ">>> code = rng.integers(0, 2, 63).astype(np.uint8)\n"
    ">>> delta = 5e-4                                   # code-rate Doppler\n"
    ">>> idx = (np.arange(63 * 4 * 300) * (1 + delta) / 4).astype(\n"
    "...     np.int64) % 63\n"
    ">>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)\n"
    ">>> plain = Dll(code, sps=4, bn=0.005)\n"
    ">>> _ = plain.steps(x)\n"
    ">>> round(plain.code_rate, 4)      # loop had to pull the whole Doppler\n"
    "1.0005\n"
    ">>> aided = Dll(code, sps=4, bn=0.005)\n"
    ">>> aided.set_rate_aid(delta)      # feed the Doppler forward instead\n"
    ">>> _ = aided.steps(x)\n"
    ">>> round(aided.code_rate, 4)      # loop integrator stays at nominal\n"
    "1.0\n" },
  { "configure_lock", (PyCFunction)(void *)DllObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(pfa, n_looks, ref_snr_db) -> int\n"
    "\n"
    "Tune the always-on code-lock detector to a target (pfa, n_looks). The "
    "detector reuses acquisition's non-coherent statistic R = sqrt(2*sum|P|^2 "
    "/ E|O|^2), where the prompt powers of n_looks consecutive looks are "
    "summed and E|O|^2 is an EMA of a random off-peak (noise) correlation "
    "re-drawn each epoch; a decision compares R against "
    "det_threshold_noncoherent(pfa, n_looks). Size n_looks with "
    "detection.det_n_noncoh(snr, ...) for your operating C/N0. The EMA "
    "bandwidth is sized probabilistically (detection.det_ema_alpha): "
    "ref_snr_db sets the noise reference's estimator SNR (mean^2/variance of "
    "the EMA output); the default 0.0 derives it from n_looks so the "
    "reference's std stays an eighth of the statistic's intrinsic H0 spread, "
    "floored at ~33 dB. Decisions feed a verify-counted lock detector rather "
    "than a single-comparison latch: locked flips up only after "
    "det_verify_count(pfa, pfa*1e-3) consecutive above-threshold decisions (2 "
    "for the default pfa=1e-3, compounding the false-declare rate three "
    "decades under pfa) and drops only after 2 consecutive below-threshold "
    "decisions, so a statistic grazing the threshold cannot chatter the flag. "
    "The default config is pfa=1e-3 over 20 looks. Raises ValueError for pfa "
    "outside (0, 1). Read the result from the locked / lock_stat / noise_est "
    "properties.\n"
    "\n"
    "The DLL carries a lock detector that reuses acquisition's non-coherent\n"
    "test statistic. Every emitted look (a partial in segments mode, or the\n"
    "full-epoch prompt when segments == 1) is also correlated at a *random\n"
    "off-peak* code phase — re-drawn each epoch and kept `noise_guard` chips\n"
    "clear of the prompt/early/late lobe — to give a signal-free CFAR noise\n"
    "sample (valid for a low-sidelobe code, e.g. Gold). The offset power\n"
    "feeds an EMA reference `E|O|^2`; the prompt powers of n_looks\n"
    "consecutive looks are summed into `S = sum|P_k|^2`, and the detector\n"
    "declares lock when\n"
    "\n"
    "R = sqrt(2 * S / E|O|^2) > det_threshold_noncoherent(pfa, n_looks)\n"
    "\n"
    "which under H0 has `P(R > eta) = marcum_q(n_looks, 0, eta)`. Size\n"
    "n_looks with det_n_noncoh(snr, ...) for the operating C/N0.\n"
    "\n"
    "The noise-reference EMA bandwidth is sized probabilistically via\n"
    "det_ema_alpha(): the signal-free `|O|^2` samples are exponential (0 dB\n"
    "estimator SNR per sample — a DC level in fluctuation of equal power),\n"
    "and ref_snr_db chooses the EMA output's estimator SNR "
    "(mean^2/variance).\n"
    "Passing 0 derives it from n_looks: the reference's relative std is held\n"
    "to an eighth of the statistic's intrinsic H0 spread (`1/sqrt(N)`),\n"
    "floored at ~33 dB — which reproduces the classic `1/alpha = max(1024,\n"
    "32*N)` sizing exactly, now as a consequence instead of a constant.\n"
    "\n"
    "The detector needs an off-peak code phase to sample noise from: with a\n"
    "very short code (fewer than ~2*(spacing+2)+1 chips, i.e. sf <= 6 at the\n"
    "default spacing) no offset clears the prompt/early/late lobe, the noise\n"
    "tap aliases the prompt, and the statistic pins below threshold — locked\n"
    "stays 0 (fail-closed) no matter the signal. Use a code of >= 7 chips\n"
    "(real spreading codes are far longer) for a meaningful lock decision.\n"
    "\n"
    "The decision itself runs through an embedded lock detector\n"
    "(lockdet_core.h) rather than a single-comparison latch: `locked` flips\n"
    "up only after det_verify_count(pfa, pfa*1e-3) CONSECUTIVE\n"
    "above-threshold decisions (the false-declare budget held three decades\n"
    "under the per-decision pfa — 2 straight for the default 1e-3), and "
    "drops\n"
    "only after 2 straight below-threshold decisions, so a statistic grazing\n"
    "the threshold cannot chatter the flag. Full control of the verify "
    "counts\n"
    "and a split declare/drop threshold pair is C-only via\n"
    "dll_configure_lock_raw().\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "pfa : float\n"
    "    Per-decision false-alarm probability, in (0, 1).\n"
    "n_looks : int\n"
    "    Non-coherent integration depth N (looks); clamped >= 1.\n"
    "ref_snr_db : float\n"
    "    Noise-reference estimator SNR in dB (> 0), or 0 to derive from\n"
    "    n_looks as above.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> d = Dll(code=np.zeros(31, dtype=np.uint8), sps=2)\n"
    ">>> d.configure_lock(1e-3, 20)\n"
    ">>> d.locked\n"
    "False\n"
    ">>> d.configure_lock(1e-3, 20, ref_snr_db=20.0)   # ~50-look reference\n"
    ">>> d.configure_lock(2.0, 20)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: configure_lock failed (rc=-4)\n" },
  { "configure_lock_raw", (PyCFunction)(void *)DllObj_configure_lock_raw,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock_raw(up_thresh, down_thresh, n_looks, alpha, n_up, n_down) "
    "-> None\n"
    "\n"
    "Escape hatch under configure_lock() for direct control of the lock "
    "detector's geometry: a split declare/drop threshold pair on the "
    "statistic R (level hysteresis), the noise-EMA coefficient alpha, and "
    "both verify counts n_up/n_down (time hysteresis) independently -- "
    "configure_lock() only ever derives a symmetric threshold (up_thresh == "
    "down_thresh) and a fixed n_down=2. Re-tuning clears the in-flight "
    "statistic and drops the lock so the next decision uses only looks "
    "gathered under the new config. Size up_thresh/down_thresh with "
    "detection.det_threshold_noncoherent(pfa, n_looks), alpha with "
    "detection.det_ema_alpha, and n_up/n_down with "
    "detection.det_verify_count. Read the result from the locked / lock_stat "
    "/ noise_est properties.\n"
    "\n"
    "The escape hatch under dll_configure_lock() for a composing C caller\n"
    "that derives its own threshold/EMA/hysteresis geometry — the full\n"
    "lockdet decision rule is exposed: a split declare/drop threshold pair\n"
    "(level hysteresis) and both verify counts (time hysteresis; size them\n"
    "with det_verify_count()). Re-tuning clears the in-flight statistic and\n"
    "drops the lock so the next decision uses only looks gathered under the\n"
    "new config.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold on the statistic R (e.g. the CFAR eta from\n"
    "    det_threshold_noncoherent()).\n"
    "down_thresh : float\n"
    "    Drop threshold on R; choose <= up_thresh for level hysteresis.\n"
    "n_looks : int\n"
    "    Non-coherent integration depth N (looks); clamped >= 1.\n"
    "alpha : float\n"
    "    EMA coefficient for the noise reference, in (0, 1].\n"
    "n_up : int\n"
    "    Consecutive above-threshold decisions to declare lock; clamped to "
    ">=\n"
    "    1.\n"
    "n_down : int\n"
    "    Consecutive below-threshold decisions to drop it; clamped to >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> # >= 7 chips gives a usable lock statistic\n"
    ">>> code = rng.integers(0, 2, 63).astype(np.uint8)\n"
    ">>> chip = np.where(code & 1, -1.0, 1.0)\n"
    ">>> x = np.tile(np.repeat(chip, 4), 400).astype(np.complex64)\n"
    ">>> d = Dll(code, sps=4, bn=0.005)\n"
    ">>> # raw geometry: declare at R>3, drop at R<2.5, 8-look,\n"
    ">>> # 2-of-2 hysteresis\n"
    ">>> d.configure_lock_raw(3.0, 2.5, 8, 1.0 / 1024, 2, 2)\n"
    ">>> _ = d.steps(x)\n"
    ">>> d.locked                       # cleared the declare threshold\n"
    "True\n"
    ">>> bool(d.lock_stat > 3.0)\n"
    "True\n" },
  { "reset", (PyCFunction)DllObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time code phase; preserve config.\n"
    "\n"
    "Restores the code phase, loop filter, correlator accumulators and lock\n"
    "detector to their post-construction state while preserving the tuned\n"
    "configuration (bn/zeta, spacing, segments, lock geometry). Re-running\n"
    "the same input after a reset therefore reproduces the same tracked\n"
    "state bit-for-bit — the basis of a deterministic replay.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import Dll\n"
    ">>> rng = np.random.default_rng(21)\n"
    ">>> code = rng.integers(0, 2, 63).astype(np.uint8)\n"
    ">>> idx = (np.arange(63 * 4 * 300) * (1 + 3e-4) / 4).astype(\n"
    "...     np.int64) % 63\n"
    ">>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)\n"
    ">>> d = Dll(code, sps=4, bn=0.005)\n"
    ">>> _ = d.steps(x)\n"
    ">>> first = round(d.code_rate, 6)\n"
    ">>> d.reset()                     # back to the create-time code phase\n"
    ">>> _ = d.steps(x)                # same input -> same tracked rate\n"
    ">>> round(d.code_rate, 6) == first\n"
    "True\n" },
  { "state_bytes", (PyCFunction)DllObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DllObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DllObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DllObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DllObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DllObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DllObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)DllObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Dll be used in a `with` statement so its C resources are "
    "released\n"
    "deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Dll\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DllObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Dll.\n"
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

static PyTypeObject DllObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.Dll",
  .tp_basicsize                           = sizeof (DllObject),
  .tp_dealloc                             = (destructor)DllObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a DLL instance (COPIES code).\n",
  .tp_methods = DllObj_methods,
  .tp_getset  = Dll_getset,
  .tp_new     = DllObj_new,
  .tp_init    = (initproc)DllObj_init,
};
