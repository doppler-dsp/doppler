/*
 * track_ext_carrier_mpsk.c — CarrierMpsk type for the track module.
 *
 * Included by track_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only track_ext.c is compiled.
 */
/* ======================================================== */
/* CarrierMpskObject — wraps carrier_mpsk_state_t *       */
/* ======================================================== */

#include "carrier_mpsk/carrier_mpsk_core.h"

typedef struct
{
  PyObject_HEAD carrier_mpsk_state_t *handle;
} CarrierMpskObject;

static void
CarrierMpskObj_dealloc (CarrierMpskObject *self)
{
  if (self->handle)
    carrier_mpsk_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CarrierMpskObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CarrierMpskObject *self = (CarrierMpskObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CarrierMpskObj_init (CarrierMpskObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "bn", "zeta", "init_norm_freq", "tsamps", "bn_fll", "m", NULL };
  double             bn             = 0.05;
  double             zeta           = 0.707;
  double             init_norm_freq = 0.0;
  unsigned long long tsamps_raw     = 64;
  double             bn_fll         = 0.0;
  int                m              = 4;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddKdi", kwlist, &bn, &zeta,
                                    &init_norm_freq, &tsamps_raw, &bn_fll, &m))
    return -1;
  size_t tsamps = (size_t)tsamps_raw;
  self->handle
      = carrier_mpsk_create (bn, zeta, init_norm_freq, tsamps, bn_fll, m);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "carrier_mpsk_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CarrierMpskObj_steps_max_out (CarrierMpskObject *self,
                              PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_mpsk_steps_max_out (self->handle));
}

static PyObject *
CarrierMpskObj_steps (CarrierMpskObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = carrier_mpsk_steps_max_out (self->handle);
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
        n_out = carrier_mpsk_steps (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = carrier_mpsk_steps_max_out (self->handle);
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
    n_out = carrier_mpsk_steps (self->handle, _ng0, _ng1, _d0, _cap);
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
CarrierMpskObj_configure (CarrierMpskObject *self, PyObject *args,
                          PyObject *kwds)
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
  carrier_mpsk_configure (self->handle, bn, zeta);
  Py_RETURN_NONE;
}

static PyObject *
CarrierMpskObj_reset (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  carrier_mpsk_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CarrierMpskObj_state_bytes (CarrierMpskObject *self,
                            PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (carrier_mpsk_state_bytes (self->handle));
}

static PyObject *
CarrierMpskObj_get_state (CarrierMpskObject *self,
                          PyObject          *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = carrier_mpsk_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  carrier_mpsk_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CarrierMpskObj_set_state (CarrierMpskObject *self, PyObject *arg)
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
      != carrier_mpsk_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (carrier_mpsk_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CarrierMpsk_getprop_bn (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_bn (self->handle));
}
static int
CarrierMpsk_setprop_bn (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_bn (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_norm_freq (CarrierMpskObject *self,
                               void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_norm_freq (self->handle));
}
static int
CarrierMpsk_setprop_norm_freq (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_lock_metric (CarrierMpskObject *self,
                                 void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_lock_metric (self->handle));
}
static PyObject *
CarrierMpsk_getprop_last_error (CarrierMpskObject *self,
                                void              *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_last_error (self->handle));
}
static PyObject *
CarrierMpsk_getprop_bn_fll (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (carrier_mpsk_get_bn_fll (self->handle));
}
static int
CarrierMpsk_setprop_bn_fll (CarrierMpskObject *self, PyObject *value,
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
  carrier_mpsk_set_bn_fll (self->handle, v);
  return 0;
}
static PyObject *
CarrierMpsk_getprop_m (CarrierMpskObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromLong ((long)carrier_mpsk_get_m (self->handle));
}

static PyGetSetDef CarrierMpsk_getset[]
    = { { "bn", (getter)CarrierMpsk_getprop_bn, (setter)CarrierMpsk_setprop_bn,
          "PLL loop noise bandwidth (retained).\n", NULL },
        { "norm_freq", (getter)CarrierMpsk_getprop_norm_freq,
          (setter)CarrierMpsk_setprop_norm_freq, "Norm freq.\n", NULL },
        { "lock_metric", (getter)CarrierMpsk_getprop_lock_metric, NULL,
          "EMA of Re(P conj a)/|P| (1 = locked).\n", NULL },
        { "last_error", (getter)CarrierMpsk_getprop_last_error, NULL,
          "last PLL discriminator (loop stress).\n", NULL },
        { "bn_fll", (getter)CarrierMpsk_getprop_bn_fll,
          (setter)CarrierMpsk_setprop_bn_fll,
          "FLL-assist bandwidth (0 = pure PLL).\n", NULL },
        { "m", (getter)CarrierMpsk_getprop_m, NULL,
          "constellation order M (2, 4, 8).\n", NULL },
        { NULL } };

static PyObject *
CarrierMpskObj_destroy (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      carrier_mpsk_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CarrierMpskObj_enter (CarrierMpskObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CarrierMpskObj_exit (CarrierMpskObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      carrier_mpsk_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CarrierMpskObj_methods[] = {

  { "steps", (PyCFunction)(void *)CarrierMpskObj_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x) -> ndarray\n"
    "\n"
    "De-rotate a cf32 block with the integer-NCO carrier, coherently "
    "integrate over each tsamps-sample symbol, run the decision-directed "
    "M-PSK discriminator (slice to the nearest constellation point, error "
    "Im(P*conj(ahat))/|P|), and emit one complex prompt symbol per symbol. "
    "The loop tracks a small residual carrier (bulk Doppler removed "
    "upstream); it locks to one of m phases, so resolve the M-fold ambiguity "
    "downstream (mpsk_diff_demap or a sync word). At m=2 this is exactly the "
    "BPSK Costas loop.\n"
    "\n"
    "The block form of the inline wipeoff/update pair: for each input sample\n"
    "it de-rotates by the carrier NCO and accumulates the coherent\n"
    "integrate-and-dump; every tsamps samples it dumps the prompt, runs the\n"
    "decision-directed M-PSK discriminator (slice to the nearest\n"
    "constellation point, error `Im(P conj(ahat))/|P|`, plus the optional\n"
    "cross-product FLL assist), filters the error, and steers the NCO\n"
    "frequency and phase. Exactly one de-rotated prompt is emitted per\n"
    "completed symbol; a trailing partial symbol is carried in the\n"
    "accumulator to the next call, so a stream can be fed in blocks of any\n"
    "length with no seam.\n"
    "\n"
    "The loop locks to one of m carrier phases — an M-fold ambiguity on the\n"
    "absolute constellation orientation. Resolve it downstream (differential\n"
    "demapping or a sync word); this call only recovers the carrier and\n"
    "returns the prompts. At m = 2 it is exactly the BPSK Costas loop.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input block, one complex baseband sample per element.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    One de-rotated prompt symbol per completed integrate-and-dump\n"
    "    period; the count is `x_len / tsamps`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_map\n"
    ">>> from doppler.track import CarrierMpsk\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> sps = 16\n"
    ">>> labels = rng.integers(0, 4, 400).astype(np.uint8)\n"
    ">>> sig = np.repeat(mpsk_map(labels, 4), sps).astype(np.complex64)\n"
    ">>> k = np.arange(len(sig))\n"
    ">>> rx = (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)\n"
    ">>> c = CarrierMpsk(bn=0.04, zeta=0.707, init_norm_freq=0.0,\n"
    "...                 tsamps=sps, bn_fll=0.02, m=4)\n"
    ">>> prompts = c.steps(rx)          # one prompt per symbol\n"
    ">>> prompts.shape\n"
    "(400,)\n"
    ">>> round(c.norm_freq, 4)       # tracked the residual carrier 0.002\n"
    "0.002\n"
    ">>> round(c.lock_metric, 2)        # decision-aligned lock metric -> 1\n"
    "1.0\n" },
  { "steps_max_out", (PyCFunction)CarrierMpskObj_steps_max_out, METH_NOARGS,
    "steps_max_out() -> int\n\nMax output length steps() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "configure", (PyCFunction)(void *)CarrierMpskObj_configure,
    METH_VARARGS | METH_KEYWORDS,
    "configure(bn, zeta) -> None\n"
    "\n"
    "Recompute the loop gains for a new (bn, zeta); preserves the "
    "frequency/phase estimate.\n"
    "\n"
    "Re-derives the proportional/integral gains of the embedded 2nd-order\n"
    "loop filter for the new noise bandwidth and damping, leaving the "
    "running\n"
    "frequency and phase estimate (the NCO and the loop integrator) "
    "untouched\n"
    "— a live lock survives a re-tune. Use it to widen the loop for fast\n"
    "pull-in and then narrow it for low-jitter tracking, mid-stream.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bn : float\n"
    "    Loop noise bandwidth, normalised to the symbol rate.\n"
    "zeta : float\n"
    "    Damping factor (0.707 = critically damped).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.track import CarrierMpsk\n"
    ">>> c = CarrierMpsk(bn=0.02, zeta=0.707, init_norm_freq=0.01,\n"
    "...                 tsamps=16, bn_fll=0.0, m=4)\n"
    ">>> round(c.bn, 3)\n"
    "0.02\n"
    ">>> c.configure(bn=0.05, zeta=1.0)   # widen the loop mid-stream\n"
    ">>> round(c.bn, 3)\n"
    "0.05\n"
    ">>> round(c.norm_freq, 3)            # frequency estimate preserved\n"
    "0.01\n" },
  { "reset", (PyCFunction)CarrierMpskObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Re-seed the loop to the create-time frequency/phase; preserve config.\n"
    "\n"
    "Returns the NCO to the seed carrier passed at construction, zeroes the\n"
    "integrate-and-dump accumulator, the FLL history, and the lock/error\n"
    "diagnostics, and re-primes the loop integrator to the matching\n"
    "per-symbol frequency — the exact state a fresh carrier_mpsk_create()\n"
    "leaves. The tuning (bn, zeta, bn_fll, tsamps, m) is untouched. Call it\n"
    "at a capture boundary so a lock reached on one segment does not bias an\n"
    "unrelated next one.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.mpsk import mpsk_map\n"
    ">>> from doppler.track import CarrierMpsk\n"
    ">>> rng = np.random.default_rng(1)\n"
    ">>> sig = np.repeat(\n"
    "...     mpsk_map(rng.integers(0, 4, 100).astype(np.uint8), 4),\n"
    "...                 16).astype(np.complex64)\n"
    ">>> rx = (sig * np.exp(2j * np.pi * 0.003 * np.arange(len(sig)))\n"
    "...       ).astype(np.complex64)\n"
    ">>> c = CarrierMpsk(bn=0.04, zeta=0.707, init_norm_freq=0.0,\n"
    "...                 tsamps=16, bn_fll=0.02, m=4)\n"
    ">>> _ = c.steps(rx)\n"
    ">>> round(c.norm_freq, 3)   # loop pulled onto the residual carrier\n"
    "0.003\n"
    ">>> c.reset()               # back to the create-time seed\n"
    ">>> round(c.norm_freq, 3)\n"
    "0.0\n" },
  { "state_bytes", (PyCFunction)CarrierMpskObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the CarrierMpskObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)CarrierMpskObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the CarrierMpskObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)CarrierMpskObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the CarrierMpskObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)CarrierMpskObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)CarrierMpskObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a CarrierMpsk be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "CarrierMpsk\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)CarrierMpskObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the CarrierMpsk.\n"
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

static PyTypeObject CarrierMpskObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "track.CarrierMpsk",
  .tp_basicsize                           = sizeof (CarrierMpskObject),
  .tp_dealloc                             = (destructor)CarrierMpskObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an M-PSK carrier loop instance.\n",
  .tp_methods = CarrierMpskObj_methods,
  .tp_getset  = CarrierMpsk_getset,
  .tp_new     = CarrierMpskObj_new,
  .tp_init    = (initproc)CarrierMpskObj_init,
};
