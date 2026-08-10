/*
 * analyzer_ext_specan.c — Specan type for the analyzer module.
 *
 * Included by analyzer_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only analyzer_ext.c is compiled.
 */
/* ======================================================== */
/* SpecanObject — wraps specan_state_t *       */
/* ======================================================== */

#include "specan/specan_core.h"

typedef struct
{
  PyObject_HEAD specan_state_t *handle;
} SpecanObject;

static void
SpecanObj_dealloc (SpecanObject *self)
{
  if (self->handle)
    specan_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SpecanObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  SpecanObject *self = (SpecanObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SpecanObj_init (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "fs",         "span", "rbw",    "src_center", "center", "offset_db",
          "full_scale", "bits", "window", "navg",       NULL };
  double             fs         = 2.048e6;
  double             span       = 200e3;
  double             rbw        = 500.0;
  double             src_center = 0.0;
  double             center     = 0.0;
  double             offset_db  = 0.0;
  double             full_scale = 1.0;
  unsigned long long bits_raw   = 0;
  const char        *window_str = "kaiser";
  unsigned long long navg_raw   = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "ddd|ddddKsK", kwlist, &fs, &span, &rbw, &src_center,
          &center, &offset_db, &full_scale, &bits_raw, &window_str, &navg_raw))
    return -1;
  size_t bits   = (size_t)bits_raw;
  int    window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", got '%s'",
                    window_str);
      return -1;
    }
  size_t navg  = (size_t)navg_raw;
  self->handle = specan_create (fs, span, rbw, src_center, center, offset_db,
                                full_scale, bits, window, navg);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "specan_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
SpecanObj_execute_max_out (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (specan_execute_max_out (self->handle));
}

static PyObject *
SpecanObj_execute (SpecanObject *self, PyObject *args, PyObject *kwds)
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
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_FLOAT
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
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = specan_execute_max_out (self->handle);
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
      float               *_ng2 = (float *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = specan_execute (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
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
  size_t _cap  = specan_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = specan_execute (self->handle, _ng0, _ng1, _d0, _cap);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
  if (!n_out)
    {
      Py_DECREF (arr0);
      Py_RETURN_NONE;
    }
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
SpecanObj_retune (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "center", NULL };
  double       center    = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &center))
    return NULL;
  specan_retune (self->handle, center);
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_reset (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  specan_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_state_bytes (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (specan_state_bytes (self->handle));
}

static PyObject *
SpecanObj_get_state (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = specan_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  specan_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
SpecanObj_set_state (SpecanObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != specan_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (specan_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Specan_getprop_fs_out (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs_out);
}
static PyObject *
Specan_getprop_span (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->span);
}
static PyObject *
Specan_getprop_rbw (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->psd->enbw * self->handle->fs_out
                             / (double)self->handle->n);
}
static PyObject *
Specan_getprop_center (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->center);
}
static PyObject *
Specan_getprop_beta (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->beta);
}
static PyObject *
Specan_getprop_n (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
Specan_getprop_nfft (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
Specan_getprop_navg (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->navg);
}
static PyObject *
Specan_getprop_display_size (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->disp_n);
}

static PyGetSetDef Specan_getset[]
    = { { "fs_out", (getter)Specan_getprop_fs_out, NULL,
          "Decimated rate, Hz (= span·1.28, ≤ fs_in).\n", NULL },
        { "span", (getter)Specan_getprop_span, NULL, "Display span, Hz.\n",
          NULL },
        { "rbw", (getter)Specan_getprop_rbw, NULL,
          "Requested resolution bandwidth, Hz.\n", NULL },
        { "center", (getter)Specan_getprop_center, NULL,
          "Display center frequency, Hz.\n", NULL },
        { "beta", (getter)Specan_getprop_beta, NULL,
          "Kaiser beta realising rbw.\n", NULL },
        { "n", (getter)Specan_getprop_n, NULL,
          "Segment / window length (samples).\n", NULL },
        { "nfft", (getter)Specan_getprop_nfft, NULL,
          "Zero-padded transform length.\n", NULL },
        { "navg", (getter)Specan_getprop_navg, NULL,
          "Segments averaged per emitted frame.\n", NULL },
        { "display_size", (getter)Specan_getprop_display_size, NULL,
          "Display size.\n", NULL },
        { NULL } };

static PyObject *
SpecanObj_destroy (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      specan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_enter (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SpecanObj_exit (SpecanObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      specan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef SpecanObj_methods[] = {

  { "execute", (PyCFunction)(void *)SpecanObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Mix, decimate, average; return one DC-centred dB display frame, or "
    "None.\n"
    "\n"
    "Feeds x through the Ddc, buffers the decimated output, and once "
    "`n·navg`\n"
    "decimated samples are available windows + FFTs + averages them into a\n"
    "fresh frame, crops the central ±span/2 band and writes it in dB (+\n"
    "ref_db). Returns 0 (writing nothing) until a frame is ready — the\n"
    "binding maps that to Python ``None``.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    cf32 input block (C-only; the binding passes it).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32]\n"
    "    Display bins written (disp_n), or 0 if no frame is ready yet.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.analyzer import Specan\n"
    ">>> import numpy as np\n"
    ">>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, navg=1)\n"
    ">>> sa.execute(np.zeros(64, dtype=np.complex64)) is None  # too few\n"
    "True\n"
    ">>> frame = sa.execute(np.zeros(65536, dtype=np.complex64))\n"
    ">>> frame.shape, frame.dtype\n"
    "((801,), dtype('float32'))\n" },
  { "execute_max_out", (PyCFunction)SpecanObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "retune", (PyCFunction)(void *)SpecanObj_retune,
    METH_VARARGS | METH_KEYWORDS,
    "retune(center) -> None\n"
    "\n"
    "Move the display center frequency (seamless LO retune; no rebuild).\n"
    "\n"
    "Updates the Ddc LO phase increment (seamless across blocks — no\n"
    "resampler or window reset) and drops pending samples so the next frame\n"
    "reflects only the new tuning. Changing the span or RBW requires a\n"
    "destroy + create (the decimation rate and window length change).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "center : float\n"
    "    New display center frequency (Hz).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Specan\n"
    "    >>> obj = Specan(fs=2.048e6, span=200e3, rbw=500.0, src_center=0.0, "
    "center=0.0, offset_db=0.0, full_scale=1.0, bits=0, window=\"kaiser\", "
    "navg=1)\n"
    "    >>> obj.retune(0.0)\n" },
  { "reset", (PyCFunction)SpecanObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Drop pending samples and the running average; zero LO/filter history.\n"
    "\n"
    "    >>> from doppler import Specan\n"
    "    >>> obj = Specan(fs=2.048e6, span=200e3, rbw=500.0, src_center=0.0, "
    "center=0.0, offset_db=0.0, full_scale=1.0, bits=0, window=\"kaiser\", "
    "navg=1)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)SpecanObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the Specan has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)SpecanObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the Specan has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)SpecanObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()`\n"
    "before the blob is handed to the C core, and the core may reject it as\n"
    "well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the Specan has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)SpecanObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)SpecanObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Specan be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Specan\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)SpecanObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Specan.\n"
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

static PyTypeObject SpecanObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "analyzer.Specan",
  .tp_basicsize                           = sizeof (SpecanObject),
  .tp_dealloc                             = (destructor)SpecanObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a natural-parameter spectrum analyzer.\n",
  .tp_methods = SpecanObj_methods,
  .tp_getset  = Specan_getset,
  .tp_new     = SpecanObj_new,
  .tp_init    = (initproc)SpecanObj_init,
};
