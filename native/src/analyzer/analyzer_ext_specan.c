/* jm:generated analyzer_ext_specan.c */
/*
 * analyzer_ext_specan.c — Specan type for the analyzer module.
 *
 * Included by analyzer_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in analyzer_ext_specan_extra.c.
 * Do NOT compile this file directly — only analyzer_ext.c is compiled.
 */
/* ======================================================== */
/* SpecanObject — wraps dp_specan_state_t *       */
/* ======================================================== */

#include "doppler/specan/specan_core.h"

typedef struct
{
  PyObject_HEAD dp_specan_state_t *handle;
  jm_inflight_t _jm_inflight; /* gh-2272: GIL-free calls running */
} SpecanObject;

static void
SpecanObj_dealloc (SpecanObject *self)
{
  if (self->handle)
    dp_specan_destroy (self->handle);
  jm_inflight_free (&self->_jm_inflight);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SpecanObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  SpecanObject *self = (SpecanObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SpecanObj_init (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "fs",        "span",       "rbw",  "src_center", "center",
          "offset_db", "full_scale", "bits", "navg",       NULL };
  double             fs         = 2.048e6;
  double             span       = 200e3;
  double             rbw        = 500.0;
  double             src_center = 0.0;
  double             center     = 0.0;
  double             offset_db  = 0.0;
  double             full_scale = 1.0;
  PyObject          *bits_obj   = NULL;
  unsigned long long bits_raw   = 0;
  PyObject          *navg_obj   = NULL;
  unsigned long long navg_raw   = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "ddd|ddddOO", kwlist, &fs, &span, &rbw, &src_center,
          &center, &offset_db, &full_scale, &bits_obj, &navg_obj))
    return -1;
  if (bits_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (bits_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      bits_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((bits_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || bits_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "bits: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t bits = (size_t)bits_raw;
  if (navg_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (navg_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      navg_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((navg_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || navg_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "navg: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t navg  = (size_t)navg_raw;
  self->handle = dp_specan_create (fs, span, rbw, src_center, center,
                                   offset_db, full_scale, bits, navg);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "dp_specan_create returned NULL");
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
  return PyLong_FromSize_t (dp_specan_execute_max_out (self->handle));
}

static PyObject *
SpecanObj_execute (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *x_obj     = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!x_arr)
    {
      return NULL;
    }
  const float _Complex *x     = (const float _Complex *)PyArray_DATA (x_arr);
  size_t                x_len = (size_t)PyArray_SIZE (x_arr);
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
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_FLOAT,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_specan_execute_max_out (self->handle);
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
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel. Two calls on
       * one object may run at once only if its kernel allows it; a
       * teardown waits for a running call (gh-2272). */
      float *_ng0 = (float *)PyArray_DATA (out_arr);
      size_t n_out;
      void  *_jm_h = self->handle;
      jm_inflight_enter (&self->_jm_inflight);
      Py_BEGIN_ALLOW_THREADS
        n_out = dp_specan_execute (_jm_h, x, x_len, _ng0, _cap);
      Py_END_ALLOW_THREADS
      jm_inflight_leave (&self->_jm_inflight);
      Py_DECREF (x_arr);
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "Specan.execute: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
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
      if (PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr)
          < 0)
        {
          Py_DECREF (out_arr);
          Py_DECREF (_oview);
          return NULL;
        }
      return _oview;
    }
  if (!self->handle)
    {
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  size_t _cap  = dp_specan_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (x_arr);
      PyErr_Format (PyExc_OverflowError,
                    "Specan.execute: output of %zu elements is too large",
                    _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_FLOAT);
  if (!arr0)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  float *_d0 = (float *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (x_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* nogil: GIL released across the pure-C kernel. Two calls on
   * one object may run at once only if its kernel allows it; a
   * teardown waits for a running call (gh-2272). */
  size_t n_out;
  void  *_jm_h = self->handle;
  jm_inflight_enter (&self->_jm_inflight);
  Py_BEGIN_ALLOW_THREADS
    n_out = dp_specan_execute (_jm_h, x, x_len, _d0, _cap);
  Py_END_ALLOW_THREADS
  jm_inflight_leave (&self->_jm_inflight);
  Py_DECREF (x_arr);
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (PyExc_RuntimeError,
                    "Specan.execute: wrote %zu elements into a buffer of %zu",
                    (size_t)(n_out), (size_t)(_cap));
      return NULL;
    }
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_specan_retune (self->handle, center);
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
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_specan_reset (self->handle);
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
  return PyLong_FromSize_t (dp_specan_state_bytes (self->handle));
}

static PyObject *
SpecanObj_get_state (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_specan_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_specan_get_state (self->handle, PyBytes_AS_STRING (_b));
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dp_specan_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_specan_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
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
  return PyFloat_FromDouble ((self->handle->psd->enbw * self->handle->fs_out
                              / (double)self->handle->n));
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
      (unsigned long long)(self->handle->disp_n));
}

static PyGetSetDef Specan_getset[]
    = { { "fs_out", (getter)Specan_getprop_fs_out, NULL,
          "Decimated rate, Hz (= span·1.28, ≤ fs_in).\n", NULL },
        { "span", (getter)Specan_getprop_span, NULL,
          "Display span, Hz (clamped to fs/1.28).\n", NULL },
        { "rbw", (getter)Specan_getprop_rbw, NULL,
          "Realised resolution bandwidth, Hz.\n", NULL },
        { "center", (getter)Specan_getprop_center, NULL,
          "Display center frequency, Hz.\n", NULL },
        { "beta", (getter)Specan_getprop_beta, NULL,
          "Kaiser beta realising rbw (>= ~12).\n", NULL },
        { "n", (getter)Specan_getprop_n, NULL,
          "Window length, a power of two >= 16.\n", NULL },
        { "nfft", (getter)Specan_getprop_nfft, NULL,
          "Transform length, max(n, 512).\n", NULL },
        { "navg", (getter)Specan_getprop_navg, NULL,
          "Segments averaged per emitted frame.\n", NULL },
        { "display_size", (getter)Specan_getprop_display_size, NULL,
          "Display size.\n", NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
SpecanObj_destroy (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_specan_destroy (_jm_st);
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
      void *_jm_st = self->handle;
      self->handle = NULL;
      /* gh-2272: a GIL-free call may still be running on this
         object; new ones are refused from here, so wait for those. */
      if (jm_inflight_wait (&self->_jm_inflight) < 0)
        {
          self->handle = _jm_st;
          return NULL;
        }
      dp_specan_destroy (_jm_st);
    }
  Py_RETURN_NONE;
}

static PyMethodDef SpecanObj_methods[] = {

  { "execute", (PyCFunction)(void *)SpecanObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray | None\n"
    "\n"
    "Mix, decimate, average; return one DC-centred dB display frame, or "
    "None.\n"
    "\n"
    "Feeds x through the Ddc, buffers the decimated output, and once\n"
    "`n·navg` decimated samples are available windows + FFTs + averages them\n"
    "into a fresh frame, crops the central ±span/2 band and writes it in\n"
    "dBFS (+ offset_db). When more than one frame's worth is pending, only\n"
    "the newest complete window is averaged and older samples are dropped,\n"
    "so a display that cannot keep up shows the latest frame. Returns 0\n"
    "(writing nothing) until a frame is ready — the binding maps that to\n"
    "Python ``None``.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    cf32 input block (C-only; the binding passes it).\n"
    "out : npt.NDArray[np.float32] | None\n"
    "    Display-spectrum buffer, dB (C-only).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.float32] | None\n"
    "    Display bins written (min(disp_n, max_out)), or 0 if no frame is\n"
    "    ready yet. None when the C call writes no output.\n"
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
    "execute_max_out() -> int\n"
    "\n"
    "Output capacity dp_specan_execute() needs: the display length.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    dp_specan_state_t::disp_n, fixed at create().\n" },
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
    "    >>> from doppler.analyzer import Specan\n"
    "    >>> obj = Specan(fs=2.048e6, span=200e3, rbw=500.0, src_center=0.0, "
    "center=0.0, offset_db=0.0, full_scale=1.0, bits=0, navg=1)\n"
    "    >>> obj.retune(0.0)\n" },
  { "reset", (PyCFunction)SpecanObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Drop pending samples and the running average; zero LO/filter history.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler.analyzer import Specan\n"
    "    >>> obj = Specan(fs=2.048e6, span=200e3, rbw=500.0, src_center=0.0, "
    "center=0.0, offset_db=0.0, full_scale=1.0, bits=0, navg=1)\n"
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
    "Captures the state that evolves as the object runs, including any value\n"
    "a setter changed after construction, so a blob taken now and restored\n"
    "later resumes from this point. Construction parameters are not\n"
    "restored: restore into an object built the same way. A core may check\n"
    "some of them and refuse a blob built differently.\n"
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
    "Overwrites the live state in place, including any value a setter\n"
    "changed after construction. Construction parameters are not restored:\n"
    "restore into an object built the same way. Length is validated against\n"
    "`state_bytes()` before the blob is handed to the C core, which may\n"
    "reject it too: a core may check some construction parameters and refuse\n"
    "a blob built differently.\n"
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
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
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

static PyTypeObject SpecanObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.analyzer.Specan",
  .tp_basicsize                           = sizeof (SpecanObject),
  .tp_dealloc                             = (destructor)SpecanObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a natural-parameter spectrum analyzer.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "fs : float\n"
    "    Input sample rate (Hz). Must be > 0.\n"
    "span : float\n"
    "    Display span (Hz). 0 = auto, the whole input band (fs/1.28); "
    "negative\n"
    "    is refused.\n"
    "rbw : float\n"
    "    Resolution bandwidth (Hz). 0 = auto (span/100); negative is "
    "refused.\n"
    "src_center : float, default 0.0\n"
    "    Source center frequency (Hz); the input band is centred here, so "
    "the\n"
    "    analyzer mixes (center − src_center) to DC.\n"
    "center : float, default 0.0\n"
    "    Desired display center frequency (Hz).\n"
    "offset_db : float, default 0.0\n"
    "    Additive dB offset on the display spectrum, applied on top of dBFS\n"
    "    (e.g. a dBm calibration the application computes from a reference\n"
    "    level).\n"
    "full_scale : float, default 1.0\n"
    "    Amplitude that reads 0 dBFS (> 0). Ignored if bits > 0.\n"
    "bits : int, default 0\n"
    "    ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) in the "
    "shared\n"
    "    PSD core (the single source of truth for the dBFS reference).\n"
    "navg : int, default 1\n"
    "    Segments averaged per emitted frame (>= 1).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.analyzer import Specan\n"
    ">>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)\n"
    ">>> sa.fs_out\n"
    "256000.0\n"
    ">>> sa.nfft, sa.display_size\n"
    "(1024, 801)\n"
    ">>> round(sa.rbw)\n"
    "500\n",
  .tp_methods = SpecanObj_methods,
  .tp_getset  = Specan_getset,
  .tp_new     = SpecanObj_new,
  .tp_init    = (initproc)SpecanObj_init,
};
