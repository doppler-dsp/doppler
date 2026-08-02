/*
 * track_ext_carrier_nda.c — CarrierNda type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CarrierNdaObject — wraps carrier_nda_state_t *       */
/* ======================================================== */

#include "carrier_nda/carrier_nda_core.h"

typedef struct
{
  PyObject_HEAD carrier_nda_state_t *handle;
} CarrierNdaObject;

static void
CarrierNdaObj_dealloc (CarrierNdaObject *self)
{
  if (self->handle)
    carrier_nda_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CarrierNdaObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CarrierNdaObject *self = (CarrierNdaObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CarrierNdaObj_init (CarrierNdaObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "sps", "n", "m", NULL };
  double             bn             = 0.01;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long sps_raw        = 8;
  int                n              = 4;
  int                m              = 4;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKii", kwlist, &bn, &zeta,
                                    &init_norm_freq, &sps_raw, &n, &m))
    return -1;
  size_t sps   = (size_t)sps_raw;
  self->handle = carrier_nda_create (bn, zeta, init_norm_freq, sps, n, m);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "carrier_nda_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CarrierNdaObj_steps_max_out (CarrierNdaObject *self,
                             PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_nda_steps_max_out (self->handle));
}

static PyObject *
CarrierNdaObj_steps (CarrierNdaObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = carrier_nda_steps_max_out (self->handle);
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
        n_out = carrier_nda_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = carrier_nda_steps_max_out (self->handle);
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
    n_out = carrier_nda_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
CarrierNdaObj_set_telemetry (CarrierNdaObject *self, PyObject *args,
                             PyObject *kwds)
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
  int      _rc = carrier_nda_set_telemetry (self->handle, tlm, prefix, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_telemetry failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_configure_lock (CarrierNdaObject *self, PyObject *args,
                              PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "up_thresh", "down_thresh", "n_up", "n_down", NULL };
  double        up_thresh   = 0.0;
  double        down_thresh = 0.0;
  unsigned long n_up_raw    = 0UL;
  unsigned long n_down_raw  = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ddkk", _kwlist, &up_thresh,
                                    &down_thresh, &n_up_raw, &n_down_raw))
    return NULL;
  uint32_t n_up   = (uint32_t)n_up_raw;
  uint32_t n_down = (uint32_t)n_down_raw;
  carrier_nda_configure_lock (self->handle, up_thresh, down_thresh, n_up,
                              n_down);
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_reset (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  carrier_nda_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_state_bytes (CarrierNdaObject *self,
                           PyObject         *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_nda_state_bytes (self->handle));
}

static PyObject *
CarrierNdaObj_get_state (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = carrier_nda_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  carrier_nda_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CarrierNdaObj_set_state (CarrierNdaObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != carrier_nda_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (carrier_nda_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CarrierNda_getprop_norm_freq (CarrierNdaObject *self,
                              void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_norm_freq (self->handle));
}
static int
CarrierNda_setprop_norm_freq (CarrierNdaObject *self, PyObject *value,
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
  carrier_nda_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
CarrierNda_getprop_lock (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_lock (self->handle));
}
static PyObject *
CarrierNda_getprop_locked (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(carrier_nda_get_locked (self->handle)));
}
static PyObject *
CarrierNda_getprop_last_error (CarrierNdaObject *self,
                               void             *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_last_error (self->handle));
}
static PyObject *
CarrierNda_getprop_bn (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_nda_get_bn (self->handle));
}
static int
CarrierNda_setprop_bn (CarrierNdaObject *self, PyObject *value,
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
  carrier_nda_set_bn (self->handle, v);
  return 0;
}
static PyObject *
CarrierNda_getprop_m (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_nda_get_m (self->handle));
}
static PyObject *
CarrierNda_getprop_n (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_nda_get_n (self->handle));
}
static PyObject *
CarrierNda_getprop_sps (CarrierNdaObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)carrier_nda_get_sps (self->handle));
}

static PyGetSetDef CarrierNda_getset[]
    = { { "norm_freq", (getter)CarrierNda_getprop_norm_freq,
          (setter)CarrierNda_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock", (getter)CarrierNda_getprop_lock, NULL,
          "EMA of the lock signal (1 = locked).\n", NULL },
        { "locked", (getter)CarrierNda_getprop_locked, NULL,
          "Current lock decision: True after the verify count of consecutive "
          "above-threshold samples, False again after the drop count of "
          "consecutive below-threshold ones (see configure_lock).\n",
          NULL },
        { "last_error", (getter)CarrierNda_getprop_last_error, NULL,
          "last phase discriminator (loop stress).\n", NULL },
        { "bn", (getter)CarrierNda_getprop_bn, (setter)CarrierNda_setprop_bn,
          "PLL loop noise bandwidth (retained).\n", NULL },
        { "m", (getter)CarrierNda_getprop_m, NULL,
          "constellation order M (2, 4, 8).\n", NULL },
        { "n", (getter)CarrierNda_getprop_n, NULL,
          "sets the MA window (= a 1/n-symbol box).\n", NULL },
        { "sps", (getter)CarrierNda_getprop_sps, NULL, "samples per symbol.\n",
          NULL },
        { NULL } };

static PyObject *
CarrierNdaObj_destroy (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      carrier_nda_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierNdaObj_enter (CarrierNdaObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CarrierNdaObj_exit (CarrierNdaObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      carrier_nda_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CarrierNdaObj_methods[] = {

  { "steps", (PyCFunction)(void *)CarrierNdaObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier and return the "
    "de-rotated samples (one per input sample). Internally the loop runs a "
    "non-data-aided M-th-power discriminator on an I/Q arm integrate-and-dump "
    "at n dumps per symbol and steers the NCO, so it acquires the carrier "
    "with no symbol timing and no data present (it strips the M-PSK "
    "modulation by raising the arm sample to the Mth power). It locks to one "
    "of m phases (M-fold ambiguity), resolved downstream. Read norm_freq for "
    "the tracked carrier and lock for the carrier lock metric.\n"
    "\n"
    "Runs the non-data-aided carrier loop over the block: each sample is\n"
    "wiped off by the integer-phase NCO, the de-rotated sample slides the "
    "I/Q\n"
    "moving-average arm, and the M-th-power discriminator (which strips the\n"
    "M-PSK data modulation) steers the NCO frequency and phase. Because the\n"
    "discriminator is data- and timing-independent, this acquires the "
    "carrier\n"
    "with no symbol timing and no data present — a bare carrier, or a\n"
    "modulated carrier before timing lock. It resolves to one of m carrier\n"
    "phases (M-fold ambiguity, resolved downstream). Read norm_freq for the\n"
    "tracked carrier (cycles/sample) and lock for the carrier lock metric.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input samples (average power at or below unity).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of de-rotated samples written to out (equals x_len).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import CarrierNda\n"
    ">>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, "
    "m=4)\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> k = np.arange(40000)\n"
    ">>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (\n"
    "...      rng.standard_normal(k.size)\n"
    "...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)\n"
    ">>> y = c.steps(x)                 # de-rotated toward DC\n"
    ">>> y.shape[0]\n"
    "40000\n"
    ">>> round(c.norm_freq, 4)          # tracked carrier, cycles/sample\n"
    "0.001\n"
    ">>> c.lock > 0.5                    # carrier lock metric, ~1 at lock\n"
    "True\n" },
  { "steps_max_out", (PyCFunction)CarrierNdaObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "set_telemetry", (PyCFunction)(void *)CarrierNdaObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(tlm, prefix, decim) -> int\n"
    "\n"
    "Attach (or detach) a telemetry context and register the carrier loop's "
    "probes on it — including the embedded arm AGC's. Registers four probes "
    "of its own, emitted once per input sample (this is a sample-rate loop — "
    "use decim to thin the stream) plus the embedded AGC's "
    "\"<prefix>.agc.gain_db\" (emitted at the AGC's own amortized gain-update "
    "rate): \"<prefix>.lock\" (the lock-signal EMA, ~1 when phase-locked), "
    "\"<prefix>.e\" (the M-th-power phase discriminator — the loop stress), "
    "\"<prefix>.freq\" (the tracked carrier frequency, cycles/sample) and "
    "\"<prefix>.locked\" (the verify-counted lockdet decision, 0/1).  Passing "
    "NULL detaches the loop and the embedded AGC. Setup path, never hot: call "
    "before the producer thread starts stepping; the context is borrowed and "
    "must outlive the attachment (SPSC rules in telemetry/telemetry.h).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : object | None\n"
    "    Telemetry context to attach, or NULL to detach.\n"
    "prefix : str\n"
    "    Probe-name prefix, e.g. \"car\" or \"rx.car\".\n"
    "decim : int\n"
    "    Emit every decim-th sample; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import CarrierNda\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 14)\n"
    ">>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)\n"
    ">>> c.set_telemetry(tlm, \"car\", decim=8)\n"
    ">>> sorted(tlm.probe_names())\n"
    "['car.agc.gain_db', 'car.e', 'car.freq', 'car.lock', 'car.locked']\n"
    ">>> x = np.exp(2j * np.pi * 0.005 * "
    "np.arange(4096)).astype(np.complex64)\n"
    ">>> _ = c.steps(x)\n"
    ">>> recs = tlm.read()\n"
    ">>> len(recs[recs[\"probe\"] == tlm.probe_id(\"car.e\")]) == 4096 // 8\n"
    "True\n" },
  { "configure_lock", (PyCFunction)(void *)CarrierNdaObj_configure_lock,
    METH_VARARGS | METH_KEYWORDS,
    "configure_lock(up_thresh, down_thresh, n_up, n_down) -> None\n"
    "\n"
    "Re-tune the carrier lock detector: locked flips up after n_up "
    "consecutive samples with the lock-signal EMA above up_thresh, and drops "
    "after n_down consecutive samples below down_thresh (level + time "
    "hysteresis; see detection.LockDet). Defaults (0.5/0.4, 8 up / 32 down) "
    "mirror MpskReceiver's own pre-existing acquisition<->tracking handover, "
    "which already steps a lockdet on this exact statistic and is validated "
    "by that receiver's BER regression gate. A live lock survives the "
    "re-tune; the in-flight verify run restarts.\n"
    "\n"
    "Full lockdet control, mirroring costas_configure_lock(): a split\n"
    "declare/drop threshold pair on the lock-signal EMA (level hysteresis)\n"
    "and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up / 32\n"
    "down) start from MpskReceiver's own pre-existing acquisition<-> "
    "tracking\n"
    "handover thresholds, but size n_up independently: `lock` is a fast\n"
    "per-sample EMA, so consecutive looks are highly autocorrelated and\n"
    "MpskReceiver's own n_up=8 does not compound the false-declare rate the\n"
    "way it would for independent looks (direct Monte Carlo against a\n"
    "noise-only, no-carrier input found real false locks at n_up=8; n_up=64\n"
    "was the smallest verify count that reliably eliminated them -- see\n"
    "carrier_nda_core.c's CARRIER_NDA_LOCK_DEFAULT_* comment for the exact\n"
    "trial data). A live lock survives the re-tune; the in-flight verify run\n"
    "restarts.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "up_thresh : float\n"
    "    Declare threshold on the lock-signal EMA.\n"
    "down_thresh : float\n"
    "    Drop threshold; choose <= up_thresh for level hysteresis.\n"
    "n_up : int\n"
    "    Consecutive above-threshold samples to declare; clamped >= 1.\n"
    "n_down : int\n"
    "    Consecutive below-threshold samples to drop; clamped >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import CarrierNda\n"
    ">>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)\n"
    ">>> c.locked\n"
    "False\n"
    ">>> c.configure_lock(0.6, 0.5, 16, 64)   # tighter declare, slower "
    "drop\n" },
  { "reset", (PyCFunction)CarrierNdaObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "Restores the object to its post-create state: the carrier NCO is reset\n"
    "to the seed frequency it was constructed with (init_norm_freq) with "
    "zero\n"
    "phase, the moving-average arm, AGC, loop-filter integrator and lock EMA\n"
    "are cleared, and the lock detector is dropped. The configured (bn,\n"
    "zeta), the arm geometry (sps, n) and the constellation order m are\n"
    "preserved, so the same object can re-acquire a fresh capture.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.track import CarrierNda\n"
    ">>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, "
    "m=4)\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> k = np.arange(40000)\n"
    ">>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (\n"
    "...      rng.standard_normal(k.size)\n"
    "...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)\n"
    ">>> _ = c.steps(x)\n"
    ">>> round(c.norm_freq, 4), round(c.lock, 2)   # acquired the carrier\n"
    "(0.001, 0.99)\n"
    ">>> c.reset()\n"
    ">>> round(c.norm_freq, 4), round(c.lock, 2)   # back to the seed, "
    "unlocked\n"
    "(0.0, 0.0)\n" },
  { "state_bytes", (PyCFunction)CarrierNdaObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CarrierNdaObj has already been "
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CarrierNdaObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CarrierNdaObj has already been "
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CarrierNdaObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CarrierNdaObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)CarrierNdaObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)CarrierNdaObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a CarrierNda be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "CarrierNda\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CarrierNdaObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the CarrierNda.\n"
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

static PyTypeObject CarrierNdaObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.CarrierNda",
  .tp_basicsize                           = sizeof (CarrierNdaObject),
  .tp_dealloc                             = (destructor)CarrierNdaObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an NDA carrier loop instance.\n",
  .tp_methods = CarrierNdaObj_methods,
  .tp_getset  = CarrierNda_getset,
  .tp_new     = CarrierNdaObj_new,
  .tp_init    = (initproc)CarrierNdaObj_init,
};
