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

#include "detection/detection_core.h"
#include "dll/dll_core.h"

typedef struct
{
  PyObject_HEAD dll_state_t *handle;
  float complex             *_steps_buf; /* pre-allocated output for steps */
  size_t                     _steps_buf_cap; /* allocated capacity for steps */
  void                     **_steps_retired; /* gh-219 deferred free */
  size_t                     _steps_retired_n;
  size_t                     _steps_retired_cap;
  PyObject                  *_steps_view_ref; /* gh-437 last returned view */
} DllObject;

static void
DllObj_dealloc (DllObject *self)
{
  if (self->handle)
    dll_destroy (self->handle);
  free (self->_steps_buf);
  for (size_t _i = 0; _i < self->_steps_retired_n; _i++)
    free (self->_steps_retired[_i]);
  free (self->_steps_retired);
  Py_XDECREF (self->_steps_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

/* Convert a (pfa, n_looks) target into the core's (threshold, n_looks, alpha)
 * lock config.  The pfa->threshold map is the detection module's non-coherent
 * CFAR threshold; the EMA noise reference averages 1/alpha cells, kept well
 * above n_looks (max(1024, 32*n_looks)) so its own variance does not inflate
 * Pfa.  Policy lives here in the (hand-owned) binding so dll_core links -lm.
 */
static void
dll_apply_lock_pfa (dll_state_t *handle, double pfa, size_t n_looks)
{
  if (n_looks < 1)
    n_looks = 1;
  double thr  = det_threshold_noncoherent (pfa, (int)n_looks);
  double leff = 32.0 * (double)n_looks;
  if (leff < 1024.0)
    leff = 1024.0;
  dll_configure_lock (handle, thr, n_looks, 1.0 / leff);
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
  /* Apply the exact default lock config (pfa=1e-3, 20 looks) via the detection
     module, overriding the core's baked-constant default with the precise
     threshold. */
  dll_apply_lock_pfa (self->handle, 1e-3, 20);
  {
    size_t _max = dll_steps_max_out (self->handle);
    if (_max)
      {
        self->_steps_buf = malloc (_max * sizeof (float complex));
        if (!self->_steps_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_steps_buf_cap = _max;
      }
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
  size_t _need      = (size_t)PyArray_SIZE (x_arr);
  int    _view_live = 0;
  if (self->_steps_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_steps_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_steps_view_ref) != Py_None;
#endif
    }
  if (!self->_steps_buf || self->_steps_buf_cap < _need || _view_live)
    {
      size_t _max = dll_steps_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_steps_buf
          && self->_steps_retired_n == self->_steps_retired_cap)
        {
          size_t _rcap
              = self->_steps_retired_cap ? self->_steps_retired_cap * 2 : 4;
          void **_rt = realloc (self->_steps_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_steps_retired     = _rt;
          self->_steps_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_steps_buf)
        self->_steps_retired[self->_steps_retired_n++] = self->_steps_buf;
      self->_steps_buf     = _tmp;
      self->_steps_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = dll_steps (self->handle, _ng0, _ng1, self->_steps_buf,
                       self->_steps_buf_cap);
  Py_END_ALLOW_THREADS
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_steps_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_steps_view_ref);
  self->_steps_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_steps_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (x_arr);
  return arr;
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
DllObj_configure_lock (DllObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "pfa", "n_looks", NULL };
  double             pfa       = 1e-3;
  unsigned long long n_looks   = 20;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "dK", _kwlist, &pfa, &n_looks))
    return NULL;
  if (!(pfa > 0.0 && pfa < 1.0))
    {
      PyErr_SetString (PyExc_ValueError, "pfa must be in (0, 1)");
      return NULL;
    }
  dll_apply_lock_pfa (self->handle, pfa, (size_t)n_looks);
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
  return PyFloat_FromDouble (dll_get_noise_est (self->handle));
}

static PyGetSetDef Dll_getset[] = {
  { "bn", (getter)Dll_getprop_bn, (setter)Dll_setprop_bn, "Bn.\n", NULL },
  { "code_phase", (getter)Dll_getprop_code_phase, NULL, "Code phase.\n",
    NULL },
  { "code_rate", (getter)Dll_getprop_code_rate, NULL, "Code rate.\n", NULL },
  { "last_error", (getter)Dll_getprop_last_error, NULL, "Last error.\n",
    NULL },
  { "segments", (getter)Dll_getprop_segments, NULL, "Segments.\n", NULL },
  { "locked", (getter)Dll_getprop_locked, NULL,
    "True when the code-lock detector's statistic exceeds its CFAR threshold "
    "(latched at each n_looks-look decision; see configure_lock).\n",
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

  { "steps", (PyCFunction)DllObj_steps, METH_VARARGS | METH_KEYWORDS,
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
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> y = obj.steps(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "steps_max_out", (PyCFunction)DllObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)DllObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the code loop's "
    "probes on it. Registers three probes, emitted once per code epoch "
    "(period) and further thinned by decim: \"<prefix>.e\" (the "
    "early-minus-late envelope discriminator — the loop stress), "
    "\"<prefix>.rate\" (the tracked code rate, chips advanced per nominal "
    "chip, ~1.0 at lock) and \"<prefix>.lock\" (the CFAR lock statistic R; "
    "compare against the configured threshold).  Passing NULL detaches.  "
    "Setup path, never hot: call before the producer thread starts stepping; "
    "the context is borrowed and must outlive the attachment (SPSC rules in "
    "telemetry/telemetry.h).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.set_telemetry(0, 0, 0)\n"
    "    0\n" },
  { "configure", (PyCFunction)(void *)DllObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the code "
    "phase/rate.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.configure(0.0, 0.0)\n" },
  { "configure_lock", (PyCFunction)(void *)DllObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(pfa, n_looks) -> None\n"
    "\n"
    "Tune the always-on code-lock detector to a target (pfa, n_looks). The "
    "detector reuses acquisition's non-coherent statistic R = sqrt(2*sum|P|^2 "
    "/ E|O|^2), where the prompt powers of n_looks consecutive looks are "
    "summed and E|O|^2 is an EMA of a random off-peak (noise) correlation "
    "re-drawn each epoch; it declares lock when R exceeds "
    "det_threshold_noncoherent(pfa, n_looks). Size n_looks with "
    "detection.det_n_noncoh(snr, ...) for your operating C/N0. The default is "
    "pfa=1e-3 over 20 looks. Read the result from the locked / lock_stat / "
    "noise_est properties.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.configure_lock(1e-3, 20)\n"
    "    >>> obj.locked\n"
    "    False\n" },
  { "reset", (PyCFunction)DllObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time code phase; preserve config.\n"
    "\n"
    "    >>> from doppler import Dll\n"
    "    >>> obj = Dll(np.zeros(1, dtype=np.uint8), 2, 0.0, 0.01, 0.707, 0.5, "
    "1)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)DllObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)DllObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)DllObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)DllObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)DllObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)DllObj_exit, METH_VARARGS, NULL },
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
