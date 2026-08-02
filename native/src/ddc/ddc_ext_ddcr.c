/*
 * ddc_ext_ddcr.c — Ddcr type for the ddc module.
 *
 * Included by ddc_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only ddc_ext.c is compiled.
 */
/* ======================================================== */
/* DdcrObject — wraps ddcr_state_t *       */
/* ======================================================== */

#include "ddcr/ddcr_core.h"

typedef struct
{
  PyObject_HEAD ddcr_state_t *handle;
} DdcrObject;

static void
DdcrObj_dealloc (DdcrObject *self)
{
  if (self->handle)
    ddcr_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DdcrObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  DdcrObject *self = (DdcrObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DdcrObj_init (DdcrObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "norm_freq", "rate", NULL };
  double       norm_freq = 0.0;
  double       rate      = 0.25;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dd", kwlist, &norm_freq,
                                    &rate))
    return -1;
  self->handle = ddcr_create (norm_freq, rate);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "Ddcr: invalid parameter (need 0 < rate < 0.5, 0 <= "
                       "beta <= 1, span >= 1, pulse_sps > 0, num_phases a "
                       "power of two >= 2)");
      return -1;
    }
  return 0;
}

static PyObject *
DdcrObj_execute_max_out (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ddcr_execute_max_out (self->handle));
}

static PyObject *
DdcrObj_execute (DdcrObject *self, PyObject *args, PyObject *kwds)
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
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
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
      size_t _omax    = ddcr_execute_max_out (self->handle);
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
      const float   *_ng0 = (const float *)PyArray_DATA (x_arr);
      size_t         _ng1 = (size_t)PyArray_SIZE (x_arr);
      float complex *_ng2 = (float complex *)PyArray_DATA (out_arr);
      size_t         n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = ddcr_execute (self->handle, _ng0, _ng1, _ng2, _cap);
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
  size_t _cap  = ddcr_execute_max_out (self->handle);
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
  const float *_ng0 = (const float *)PyArray_DATA (x_arr);
  size_t       _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t       n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = ddcr_execute (self->handle, _ng0, _ng1, _d0, _cap);
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
DdcrObj_execute_ctrl (DdcrObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "rate_ctrl", "freq_ctrl", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  double         rate_ctrl = 0;
  double         freq_ctrl = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Odd", _kwlist, &x_obj,
                                    &rate_ctrl, &freq_ctrl))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_FLOAT,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = ddcr_execute_ctrl_max_out (self->handle);
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
  const float *_ng0 = (const float *)PyArray_DATA (x_arr);
  size_t       _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t       n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = ddcr_execute_ctrl (self->handle, _ng0, _ng1, rate_ctrl, freq_ctrl,
                               _d0, _cap);
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
DdcrObj_execute_ctrl_push (DdcrObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "rate_ctrl", "freq_ctrl", NULL };
  float        x         = 0;
  double       rate_ctrl = 0;
  double       freq_ctrl = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "fdd", _kwlist, &x, &rate_ctrl,
                                    &freq_ctrl))
    return NULL;
  size_t _need = ddcr_execute_ctrl_push_max_out (self->handle);
  size_t _cap  = ddcr_execute_ctrl_push_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = ddcr_execute_ctrl_push (self->handle, x, rate_ctrl, freq_ctrl,
                                         _d0, _cap);
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
DdcrObj_reset (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  ddcr_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DdcrObj_state_bytes (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (ddcr_state_bytes (self->handle));
}

static PyObject *
DdcrObj_get_state (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = ddcr_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  ddcr_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DdcrObj_set_state (DdcrObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != ddcr_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (ddcr_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Ddcr_getprop_norm_freq (DdcrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddcr_get_norm_freq (self->handle));
}
static int
Ddcr_setprop_norm_freq (DdcrObject *self, PyObject *value,
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
  ddcr_set_norm_freq (self->handle, v);
  return 0;
}
static PyObject *
Ddcr_getprop_rate (DdcrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (ddcr_get_rate (self->handle));
}
static PyObject *
Ddcr_getprop_clipped (DdcrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ddcr_get_clipped (self->handle)));
}
static PyObject *
Ddcr_getprop_narrow_pulse (DdcrObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(ddcr_get_narrow_pulse (self->handle)));
}

static PyGetSetDef Ddcr_getset[]
    = { { "norm_freq", (getter)Ddcr_getprop_norm_freq,
          (setter)Ddcr_setprop_norm_freq,
          "Return the current fine NCO normalised frequency at the "
          "intermediate rate (fs_in/2, cycles/sample).\n",
          NULL },
        { "rate", (getter)Ddcr_getprop_rate, NULL,
          "Return the total configured rate (fs_out / fs_in, read-only). This "
          "is the end-to-end ratio from ADC input to CF32 output.  Change it "
          "by destroying and recreating the DDCR.\n",
          NULL },
        { "clipped", (getter)Ddcr_getprop_clipped, NULL,
          "Has the cascade's CIC clipped its input since the last reset?\n",
          NULL },
        { "narrow_pulse", (getter)Ddcr_getprop_narrow_pulse, NULL,
          "Is this object's rectangular matched filter degenerately narrow?\n",
          NULL },
        { NULL } };

static PyObject *
DdcrObj_destroy (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      ddcr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DdcrObj_enter (DdcrObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DdcrObj_exit (DdcrObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      ddcr_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DdcrObj_methods[] = {

  { "execute", (PyCFunction)(void *)DdcrObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Down-convert a block of real float32 samples to CF32 baseband.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Number of output samples written (C-only).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ddc import Ddcr\n"
    ">>> import numpy as np\n"
    ">>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)\n"
    ">>> t = np.arange(4096)\n"
    ">>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)\n"
    ">>> out = np.empty(len(x), dtype=np.complex64)\n"
    ">>> y = ddcr.execute(x, out)\n"
    ">>> y.shape\n"
    "(1024,)\n"
    ">>> y.dtype\n"
    "dtype('complex64')\n"
    ">>> round(float(abs(y[500])), 2)   # one-sided cosine amplitude ≈ 0.5\n"
    "0.5\n" },
  { "execute_max_out", (PyCFunction)DdcrObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "execute_ctrl", (PyCFunction)(void *)DdcrObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x) -> ndarray\n"
    "\n"
    "Process a real block, steering both control ports.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Ddcr\n"
    "    >>> obj = Ddcr(0.0, 0.25)\n"
    "    >>> y = obj.execute_ctrl(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ctrl_push", (PyCFunction)(void *)DdcrObj_execute_ctrl_push,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl_push(n=1) -> ndarray\n"
    "\n"
    "Push ONE real input sample; emit whatever outputs it completes.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Ddcr\n"
    "    >>> obj = Ddcr(0.0, 0.25)\n"
    "    >>> y = obj.execute_ctrl_push(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "reset", (PyCFunction)DdcrObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero halfband history, LO phase and filter history.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ddc import Ddcr\n"
    ">>> import numpy as np\n"
    ">>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)\n"
    ">>> x = np.ones(64, dtype=np.float32)\n"
    ">>> out = np.empty(64, dtype=np.complex64)\n"
    ">>> y1 = ddcr.execute(x, out).copy()\n"
    ">>> ddcr.reset()\n"
    ">>> y2 = ddcr.execute(x, out)\n"
    ">>> bool(np.array_equal(y1, y2))\n"
    "True\n" },
  { "state_bytes", (PyCFunction)DdcrObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DdcrObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DdcrObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DdcrObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DdcrObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the DdcrObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "close", (PyCFunction)DdcrObj_destroy, METH_NOARGS,
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
  { "destroy", (PyCFunction)DdcrObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)DdcrObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Ddcr be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Ddcr\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DdcrObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Ddcr.\n"
    "\n"
    "Equivalent to calling `close()`. Returns ``None``, so an exception\n"
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

static PyTypeObject DdcrObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "ddc.Ddcr",
  .tp_basicsize                           = sizeof (DdcrObject),
  .tp_dealloc                             = (destructor)DdcrObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a real-input Digital Down-Converter (Architecture D2). The signal "
    "chain is: halfband R2C (2:1, bakes in +fs/4 shift) -> fine LO mix at the "
    "intermediate rate (fs_in/2) -> RateConverter -> CF32 output.  The "
    "halfband stage uses +-1/0 coefficients (no multiplications) and puts the "
    "fine LO and the cascade at fs_in/2.  That is worth ~1.1-1.7x in a whole "
    "receiver (it halves the rate ahead of the polyphase matched filter, so "
    "the gain grows with samples/symbol) and close to nothing for the front "
    "end alone -- see the file header for the measurements.  Use it because "
    "the input IS real.\n",
  .tp_methods = DdcrObj_methods,
  .tp_getset  = Ddcr_getset,
  .tp_new     = DdcrObj_new,
  .tp_init    = (initproc)DdcrObj_init,
};
