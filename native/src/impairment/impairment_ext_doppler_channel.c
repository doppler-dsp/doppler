/*
 * impairment_ext_doppler_channel.c — DopplerChannel type for the impairment
 * module.
 *
 * Included by impairment_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only impairment_ext.c is compiled.
 */
/* ======================================================== */
/* DopplerChannelObject — wraps doppler_channel_state_t *       */
/* ======================================================== */

#include "doppler_channel/doppler_channel_core.h"

typedef struct
{
  PyObject_HEAD doppler_channel_state_t *handle;
} DopplerChannelObject;

static void
DopplerChannelObj_dealloc (DopplerChannelObject *self)
{
  if (self->handle)
    doppler_channel_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DopplerChannelObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DopplerChannelObject *self
      = (DopplerChannelObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DopplerChannelObj_init (DopplerChannelObject *self, PyObject *args,
                        PyObject *kwds)
{
  static char *kwlist[]
      = { "fs", "carrier_hz", "doppler_ppm", "doppler_rate_ppm_s", NULL };
  double fs                 = 1000000.0;
  double carrier_hz         = 0.0;
  double doppler_ppm        = 0.0;
  double doppler_rate_ppm_s = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dddd", kwlist, &fs,
                                    &carrier_hz, &doppler_ppm,
                                    &doppler_rate_ppm_s))
    return -1;
  self->handle = doppler_channel_create (fs, carrier_hz, doppler_ppm,
                                         doppler_rate_ppm_s);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "doppler_channel_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
DopplerChannelObj_execute_max_out (DopplerChannelObject *self,
                                   PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (doppler_channel_execute_max_out (self->handle));
}

static PyObject *
DopplerChannelObj_execute (DopplerChannelObject *self, PyObject *args,
                           PyObject *kwds)
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
      size_t _omax    = doppler_channel_execute_max_out (self->handle);
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
        n_out = doppler_channel_execute (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = doppler_channel_execute_max_out (self->handle);
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
    n_out = doppler_channel_execute (self->handle, _ng0, _ng1, _d0, _cap);
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
DopplerChannelObj_reset (DopplerChannelObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  doppler_channel_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DopplerChannelObj_state_bytes (DopplerChannelObject *self,
                               PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (doppler_channel_state_bytes (self->handle));
}

static PyObject *
DopplerChannelObj_get_state (DopplerChannelObject *self,
                             PyObject             *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = doppler_channel_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  doppler_channel_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DopplerChannelObj_set_state (DopplerChannelObject *self, PyObject *arg)
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
      != doppler_channel_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (doppler_channel_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DopplerChannel_getprop_fs (DopplerChannelObject *self,
                           void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
DopplerChannel_getprop_carrier_hz (DopplerChannelObject *self,
                                   void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->carrier_hz);
}
static PyObject *
DopplerChannel_getprop_doppler_ppm (DopplerChannelObject *self,
                                    void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_ppm);
}
static PyObject *
DopplerChannel_getprop_doppler_rate_ppm_s (DopplerChannelObject *self,
                                           void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_rate_ppm_s);
}
static PyObject *
DopplerChannel_getprop_elapsed_s (DopplerChannelObject *self,
                                  void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (doppler_channel_get_elapsed_s (self->handle));
}
static PyObject *
DopplerChannel_getprop_offset_hz (DopplerChannelObject *self,
                                  void                 *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (doppler_channel_get_offset_hz (self->handle));
}

static PyGetSetDef DopplerChannel_getset[]
    = { { "fs", (getter)DopplerChannel_getprop_fs, NULL, "Fs.\n", NULL },
        { "carrier_hz", (getter)DopplerChannel_getprop_carrier_hz, NULL,
          "Carrier hz.\n", NULL },
        { "doppler_ppm", (getter)DopplerChannel_getprop_doppler_ppm, NULL,
          "Doppler ppm.\n", NULL },
        { "doppler_rate_ppm_s",
          (getter)DopplerChannel_getprop_doppler_rate_ppm_s, NULL,
          "Doppler rate ppm s.\n", NULL },
        { "elapsed_s", (getter)DopplerChannel_getprop_elapsed_s, NULL,
          "Receive time in seconds consumed so far, the `t` every Doppler "
          "quantity is evaluated at. Advances by `n/fs` per `execute(x)` call "
          "and is zeroed by `reset()`.\n",
          NULL },
        { "offset_hz", (getter)DopplerChannel_getprop_offset_hz, NULL,
          "Instantaneous carrier offset `fc * d(t)` in Hz at the current "
          "`elapsed_s` -- the frequency a receiver would have to tune out "
          "right now. Read-only diagnostic; with a non-zero "
          "`doppler_rate_ppm_s` it ramps as the stream advances.\n",
          NULL },
        { NULL } };

static PyObject *
DopplerChannelObj_destroy (DopplerChannelObject *self,
                           PyObject             *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      doppler_channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DopplerChannelObj_enter (DopplerChannelObject *self,
                         PyObject             *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DopplerChannelObj_exit (DopplerChannelObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      doppler_channel_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DopplerChannelObj_methods[] = {

  { "execute", (PyCFunction)(void *)DopplerChannelObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Apply clock Doppler to a block of complex baseband.\n"
    "\n"
    "Resamples x by `1/(1+d(t))` and multiplies the result by the coherent\n"
    "carrier `exp(j*2*pi*fc*excess(t))`. State persists across calls, so\n"
    "feeding a stream in blocks gives the same samples as one large call\n"
    "(subject to `DOPPLER_CHANNEL_MAX_BLOCK`).\n"
    "\n"
    "Output length is approximately `x_len/(1+d)` and varies by a sample "
    "from\n"
    "call to call as the fractional resampling accumulator crosses — that\n"
    "variation is the dilation itself, not a defect.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input block.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Samples written to out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.impairment import DopplerChannel\n"
    ">>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)\n"
    ">>> y = ch.execute(np.ones(1000, dtype=np.complex64))\n"
    ">>> y.shape                   # ~ 1000 / (1 + 20e-6): the time-base "
    "dilation\n"
    "(999,)\n"
    ">>> round(ch.offset_hz, 1)    # fc * d = 2.5e9 * 20e-6, in Hz\n"
    "50000.0\n" },
  { "execute_max_out", (PyCFunction)DopplerChannelObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "reset", (PyCFunction)DopplerChannelObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Reset DopplerChannel to its post-create state.\n"
    "\n"
    "Zeroes both sample clocks (so `elapsed_s` and the carrier phase restart\n"
    "at zero) and clears the resampler's delay line and fractional\n"
    "accumulator. The configured\n"
    "`fs`/`carrier_hz`/`doppler_ppm`/`doppler_rate_ppm_s` are kept.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.impairment import DopplerChannel\n"
    ">>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)\n"
    ">>> _ = ch.execute(np.ones(1000, dtype=np.complex64))\n"
    ">>> round(ch.elapsed_s, 6)    # receive time consumed: 999 / 1e6\n"
    "0.000999\n"
    ">>> ch.reset()                # both sample clocks back to zero\n"
    ">>> ch.elapsed_s\n"
    "0.0\n" },
  { "state_bytes", (PyCFunction)DopplerChannelObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DopplerChannelObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DopplerChannelObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DopplerChannelObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DopplerChannelObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DopplerChannelObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DopplerChannelObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)DopplerChannelObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DopplerChannel be used in a `with` statement so its C resources\n"
    "are released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DopplerChannel\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DopplerChannelObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DopplerChannel.\n"
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

static PyTypeObject DopplerChannelObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "impairment.DopplerChannel",
  .tp_basicsize                           = sizeof (DopplerChannelObject),
  .tp_dealloc = (destructor)DopplerChannelObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "DopplerChannel type.\n",
  .tp_methods = DopplerChannelObj_methods,
  .tp_getset  = DopplerChannel_getset,
  .tp_new     = DopplerChannelObj_new,
  .tp_init    = (initproc)DopplerChannelObj_init,
};
