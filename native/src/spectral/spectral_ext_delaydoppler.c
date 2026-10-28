/* jm:generated spectral_ext_delaydoppler.c */
/*
 * spectral_ext_delaydoppler.c — DelayDoppler type for the spectral module.
 *
 * Included by spectral_ext.c (the module aggregator).
 * jm regenerates this file on every apply; do not edit it.
 * Hand-written code belongs in spectral_ext_delaydoppler_extra.c.
 * Do NOT compile this file directly — only spectral_ext.c is compiled.
 */
/* ======================================================== */
/* DelayDopplerObject — wraps dp_corr2d_state_t *       */
/* ======================================================== */

#include "doppler/corr2d/corr2d_core.h"

typedef struct
{
  PyObject_HEAD dp_corr2d_state_t *handle;
} DelayDopplerObject;

static void
DelayDopplerObj_dealloc (DelayDopplerObject *self)
{
  if (self->handle)
    dp_corr2d_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
DelayDopplerObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  /* tp_new allocates only; __init__ reads the arguments. */
  (void)args;
  (void)kwds;
  DelayDopplerObject *self = (DelayDopplerObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
DelayDopplerObj_init (DelayDopplerObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "code", "ny", "dwell", "nthreads", "ny_out", NULL };
  PyObject          *code_obj   = NULL;
  PyObject          *ny_obj     = NULL;
  unsigned long long ny_raw     = 1;
  PyObject          *dwell_obj  = NULL;
  unsigned long long dwell_raw  = 1;
  int                nthreads   = 1;
  PyObject          *ny_out_obj = NULL;
  unsigned long long ny_out_raw = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|OOiO", kwlist, &code_obj,
                                    &ny_obj, &dwell_obj, &nthreads,
                                    &ny_out_obj))
    return -1;
  if (ny_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (ny_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      ny_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((ny_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || ny_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "ny: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t ny = (size_t)ny_raw;
  if (dwell_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (dwell_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      dwell_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((dwell_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || dwell_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "dwell: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t dwell = (size_t)dwell_raw;
  if (ny_out_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (ny_out_obj);
      if (_ix == NULL)
        {
          return -1;
        }
      ny_out_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((ny_out_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || ny_out_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "ny_out: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return -1;
        }
      Py_DECREF (_ix);
    }
  size_t         ny_out = (size_t)ny_out_raw;
  PyArrayObject *code_arr
      = jm_array_arg (code_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "code");
  if (!code_arr)
    {
      return -1;
    }
  size_t code_len = (size_t)PyArray_SIZE (code_arr);
  self->handle    = dp_corr2d_create_delay_doppler (
      (const float _Complex *)PyArray_DATA (code_arr), code_len, ny, dwell,
      nthreads, ny_out);
  Py_DECREF (code_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "DelayDoppler: invalid parameter (need a non-empty "
                       "code, ny >= 1, dwell >= 1, and ny_out 0 or at least "
                       "ny)");
      return -1;
    }
  return 0;
}

static PyObject *
DelayDopplerObj_reset (DelayDopplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  dp_corr2d_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
DelayDopplerObj_execute_max_out (DelayDopplerObject *self,
                                 PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_corr2d_execute_max_out (self->handle));
}

static PyObject *
DelayDopplerObj_execute (DelayDopplerObject *self, PyObject *args,
                         PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj    = NULL;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &in_obj,
                                    &out_obj))
    return NULL;
  PyArrayObject *in_arr
      = jm_array_arg (in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "x");
  if (!in_arr)
    {
      return NULL;
    }
  Py_ssize_t n = PyArray_SIZE (in_arr);
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
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr
          = jm_array_arg (out_obj, NPY_COMPLEX64,
                          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE, "out");
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap = (size_t)PyArray_SIZE (out_arr);
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t _omax    = dp_corr2d_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      if (!self->handle)
        {
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          PyErr_SetString (PyExc_RuntimeError, "destroyed");
          return NULL;
        }
      size_t n_out = dp_corr2d_execute (
          self->handle, (const float _Complex *)PyArray_DATA (in_arr),
          (size_t)n, (float _Complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
      if (n_out == (SIZE_MAX))
        {
          Py_DECREF (out_arr);
          PyErr_SetString (PyExc_ValueError,
                           "input length is not ny*nx; execute takes exactly "
                           "one frame");
          return NULL;
        }
      if ((size_t)(n_out) > (size_t)(_cap))
        {
          Py_DECREF (out_arr);
          PyErr_Format (
              PyExc_RuntimeError,
              "DelayDoppler.execute: wrote %zu elements into a buffer of %zu",
              (size_t)(n_out), (size_t)(_cap));
          return NULL;
        }
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX64,
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
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t _need = (size_t)n;
  size_t _cap  = dp_corr2d_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  size_t _adim_need = (size_t)(_cap);
  if (_adim_need > (size_t)NPY_MAX_INTP)
    {
      Py_DECREF (in_arr);
      PyErr_Format (
          PyExc_OverflowError,
          "DelayDoppler.execute: output of %zu elements is too large",
          _adim_need);
      return NULL;
    }
  npy_intp  _adim = (npy_intp)_adim_need;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float _Complex *_d0 = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr0);
  if (!self->handle)
    {
      Py_DECREF (arr0);
      Py_DECREF (in_arr);
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t n_out = dp_corr2d_execute (
      self->handle, (const float _Complex *)PyArray_DATA (in_arr), (size_t)n,
      _d0, _cap);
  Py_DECREF (in_arr);
  if (n_out == (SIZE_MAX))
    {
      Py_DECREF (arr0);
      PyErr_SetString (PyExc_ValueError,
                       "input length is not ny*nx; execute takes exactly one "
                       "frame");
      return NULL;
    }
  if ((size_t)(n_out) > (size_t)(_cap))
    {
      Py_DECREF (arr0);
      PyErr_Format (
          PyExc_RuntimeError,
          "DelayDoppler.execute: wrote %zu elements into a buffer of %zu",
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
DelayDopplerObj_state_bytes (DelayDopplerObject *self,
                             PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_corr2d_state_bytes (self->handle));
}

static PyObject *
DelayDopplerObj_get_state (DelayDopplerObject *self,
                           PyObject           *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_corr2d_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  dp_corr2d_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
DelayDopplerObj_set_state (DelayDopplerObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != dp_corr2d_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (dp_corr2d_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
DelayDoppler_getprop_ny (DelayDopplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->ny);
}
static PyObject *
DelayDoppler_getprop_nx (DelayDopplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nx);
}
static PyObject *
DelayDoppler_getprop_ny_out (DelayDopplerObject *self,
                             void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->ny_out);
}
static PyObject *
DelayDoppler_getprop_n_out (DelayDopplerObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n_out);
}
static PyObject *
DelayDoppler_getprop_dwell (DelayDopplerObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->dwell);
}
static PyObject *
DelayDoppler_getprop_count (DelayDopplerObject *self,
                            void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->count);
}
static PyObject *
DelayDoppler_getprop_doppler_step (DelayDopplerObject *self,
                                   void               *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->doppler_step);
}

static PyGetSetDef DelayDoppler_getset[]
    = { { "ny", (getter)DelayDoppler_getprop_ny, NULL, "Row count.\n", NULL },
        { "nx", (getter)DelayDoppler_getprop_nx, NULL, "Column count.\n",
          NULL },
        { "ny_out", (getter)DelayDoppler_getprop_ny_out, NULL,
          "Output rows (== ny unless decoupled).\n", NULL },
        { "n_out", (getter)DelayDoppler_getprop_n_out, NULL,
          "ny_out * nx_out — output element count.\n", NULL },
        { "dwell", (getter)DelayDoppler_getprop_dwell, NULL,
          "Integration depth.\n", NULL },
        { "count", (getter)DelayDoppler_getprop_count, NULL,
          "Frames accumulated (0 … dwell-1).\n", NULL },
        { "doppler_step", (getter)DelayDoppler_getprop_doppler_step, NULL,
          "Doppler spacing between adjacent output rows, cycles/sample: 1 / "
          "(ny_out * nx).  0 for Corr2D.\n",
          NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
DelayDopplerObj_destroy (DelayDopplerObject *self,
                         PyObject           *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_corr2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
DelayDopplerObj_enter (DelayDopplerObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
DelayDopplerObj_exit (DelayDopplerObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_corr2d_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef DelayDopplerObj_methods[] = {
  { "reset", (PyCFunction)DelayDopplerObj_reset, METH_NOARGS,
    "Zero the accumulator and reset the integration counter to 0.\n"
    "Equivalent to starting a fresh dwell cycle without rebuilding FFT plans\n"
    "or recomputing ref_spec.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=3)\n"
    ">>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))\n"
    ">>> c.count\n"
    "1\n"
    ">>> c.reset()\n"
    ">>> c.count\n"
    "0\n" },

  { "execute", (PyCFunction)(void *)DelayDopplerObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out) -> ndarray | None\n"
    "\n"
    "Correlate one 2-D frame and optionally dump the coherent\n"
    "accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with\n"
    "ref_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷\n"
    "ny*nx). Accumulating in the frequency domain and inverting once is\n"
    "exactly the per-frame inverse summed, by linearity of the IFFT — valid\n"
    "because the dwell is **coherent** (a complex sum); a non-coherent\n"
    "(magnitude) integration could not defer the inverse. The Python wrapper\n"
    "accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx\n"
    "ndarray, a no-dump returns None.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : npt.NDArray[np.complex64]\n"
    "    Input.\n"
    "out : npt.NDArray[np.complex64] | None\n"
    "    Output buffer for the correlation map (CF32, length n_out:\n"
    "    ny_out*nx_out, or ny with col_out); written only on a dump call.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64] | None\n"
    "    n_out on a dump (or max_out if smaller), 0 otherwise (None in\n"
    "    Python). If n_in is not the required length the call is REFUSED\n"
    "    with nothing read, written or counted: SIZE_MAX in C, ValueError in\n"
    "    Python (#1925). None when the C call writes no output.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns ``SIZE_MAX``, its refusal value, in place of\n"
    "    a count. The exception message is ``input length is not ny*nx;\n"
    "    execute takes exactly one frame``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.spectral import Corr2D\n"
    ">>> import numpy as np\n"
    ">>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0\n"
    ">>> c = Corr2D(ref=ref, dwell=2)\n"
    ">>> x = np.ones((2, 2), dtype=np.complex64)\n"
    ">>> c.execute(x) is None   # frame 1 — no dump\n"
    "True\n"
    ">>> c.execute(x).tolist()  # frame 2 — dump\n"
    "[(2+0j), (2+0j), (2+0j), (2+0j)]\n" },
  { "execute_max_out", (PyCFunction)DelayDopplerObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n"
    "\n"
    "Maximum output samples per execute call (dp_corr2d_state_t::n_out --\n"
    "ny_out*nx_out normally, or ny when a single dp_corr2d_state_t::col_out\n"
    "was selected).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    n_out, fixed at create().\n" },
  { "state_bytes", (PyCFunction)DelayDopplerObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the DelayDoppler has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)DelayDopplerObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the DelayDoppler has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)DelayDopplerObj_set_state, METH_O,
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
    "``RuntimeError`` if the DelayDoppler has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)DelayDopplerObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)DelayDopplerObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Corr2D be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Corr2D\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)DelayDopplerObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Corr2D.\n"
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

static PyTypeObject DelayDopplerObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.spectral.DelayDoppler",
  .tp_basicsize                           = sizeof (DelayDopplerObject),
  .tp_dealloc = (destructor)DelayDopplerObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate the delay-Doppler flavor: a code correlator that maps a raw\n"
    "stream of ny code periods onto (Doppler, delay).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : NDArray[np.complex64]\n"
    "    The code replica, code_len samples (one period).\n"
    "ny : int, default 1\n"
    "    Periods per frame, the slow-time length; must be >= 1.\n"
    "dwell : int, default 1\n"
    "    Frames coherently accumulated per dump; must be >= 1.\n"
    "nthreads : int, default 1\n"
    "    Workers: 1 serial, N for N (the caller included), <= 0 every online\n"
    "    core; capped at the most chunks a stage can use, `max(ny, "
    "ceil(code_len\n"
    "    / 16))`.\n"
    "ny_out : int, default 0\n"
    "    Doppler bins in the map; 0 means ny. Must be >= ny.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.spectral import DelayDoppler\n"
    ">>> rng = np.random.default_rng(0)\n"
    ">>> ny, nx, d, k = 16, 64, 5, 3       # delay 5, Doppler 3 bins\n"
    ">>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)\n"
    ">>> n = np.arange(ny * nx)\n"
    ">>> f = k / (ny * nx)                 # cycles/sample\n"
    ">>> x = (np.tile(np.roll(code, d), ny)\n"
    "...      * np.exp(2j * np.pi * f * n)).astype(np.complex64)\n"
    ">>> dd = DelayDoppler(code, ny=ny, dwell=2)\n"
    ">>> out = np.empty(dd.execute_max_out(), np.complex64)\n"
    ">>> for _ in range(2):                # one preallocated map per dump\n"
    "...     m = dd.execute(x, out=out)\n"
    ">>> m is not None and m.base is out\n"
    "True\n"
    ">>> row, col = np.unravel_index(np.argmax(np.abs(out)), (ny, nx))\n"
    ">>> int(row) - ny // 2, int(col)      # (Doppler bins, delay)\n"
    "(3, 5)\n"
    ">>> dd.doppler_step == 1 / (ny * nx)\n"
    "True\n",
  .tp_methods = DelayDopplerObj_methods,
  .tp_getset  = DelayDoppler_getset,
  .tp_new     = DelayDopplerObj_new,
  .tp_init    = (initproc)DelayDopplerObj_init,
};
