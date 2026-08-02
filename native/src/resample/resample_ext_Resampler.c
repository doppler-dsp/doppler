/*
 * resample_ext_Resampler.c — Resampler type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* ResamplerObject — wraps Resampler_state_t *       */
/* ======================================================== */

#include "Resampler/Resampler_core.h"
#include "dp_state_pyhelp.h"

typedef struct
{
  PyObject_HEAD Resampler_state_t *handle;
  float complex *_execute_buf;      /* pre-allocated output for execute */
  size_t         _execute_buf_cap;  /* elements allocated above */
  float complex *_execute_ctrl_buf; /* pre-allocated output for execute_ctrl */
  size_t         _execute_ctrl_buf_cap; /* elements allocated above */
} ResamplerObject;

static void
ResamplerObj_dealloc (ResamplerObject *self)
{
  if (self->handle)
    Resampler_destroy (self->handle);
  free (self->_execute_buf);
  free (self->_execute_ctrl_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ResamplerObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ResamplerObject *self = (ResamplerObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ResamplerObj_init (ResamplerObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "rate", "bank", NULL };
  double       rate     = 0.0;
  PyObject    *bank_obj = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|dO", kwlist, &rate,
                                    &bank_obj))
    return -1;

  if (bank_obj && bank_obj != Py_None)
    {
      PyArrayObject *bank_arr = (PyArrayObject *)PyArray_FROM_OTF (
          bank_obj, NPY_FLOAT32, NPY_ARRAY_C_CONTIGUOUS);
      if (!bank_arr)
        return -1;
      if (PyArray_NDIM (bank_arr) != 2)
        {
          Py_DECREF (bank_arr);
          PyErr_SetString (PyExc_ValueError, "bank must be 2-D float32");
          return -1;
        }
      size_t num_phases = (size_t)PyArray_DIM (bank_arr, 0);
      size_t num_taps   = (size_t)PyArray_DIM (bank_arr, 1);
      self->handle      = Resampler_create_custom (
          num_phases, num_taps, (const float *)PyArray_DATA (bank_arr), rate);
      Py_DECREF (bank_arr);
    }
  else
    {
      self->handle = Resampler_create (rate);
    }

  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "Resampler_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
ResamplerObj_execute_max_out (ResamplerObject *self,
                              PyObject        *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (Resampler_execute_max_out (self->handle));
}

static PyObject *
ResamplerObj_execute (ResamplerObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj    = NULL;
  PyObject      *out_obj  = NULL;
  PyArrayObject *x_arr    = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). Hand-written
       * here because this fragment stays hand-owned; keep in step with
       * jm's generated form (gh-581). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
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
      size_t _omax    = Resampler_execute_max_out (self->handle);
      size_t _n_in    = (size_t)PyArray_SIZE (x_arr);
      size_t _min_cap = _omax > _n_in ? _omax : _n_in;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t n_out = Resampler_execute (
          self->handle, (const float complex *)PyArray_DATA (x_arr), _n_in,
          (float complex *)PyArray_DATA (out_arr), _cap);
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

  if (!self->_execute_buf)
    {
      /* Resampler_execute_max_out() always returns the fixed
       * RESAMPLER_MAX_OUT (65536) regardless of input size -- the kernel's
       * own contract caps output there, so one allocation at that size
       * covers every future call; no growth path is needed. */
      size_t _max = Resampler_execute_max_out (self->handle);
      if (!_max)
        _max = (size_t)PyArray_SIZE (x_arr);
      self->_execute_buf     = malloc (_max * sizeof (float complex));
      self->_execute_buf_cap = _max;
      if (!self->_execute_buf)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = Resampler_execute (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr), self->_execute_buf,
      self->_execute_buf_cap);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
ResamplerObj_execute_ctrl_max_out (ResamplerObject *self,
                                   PyObject        *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (Resampler_execute_ctrl_max_out (self->handle));
}

static PyObject *
ResamplerObj_execute_ctrl (ResamplerObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *kwlist[] = { "x", "ctrl", "out", NULL };
  PyObject      *x_obj    = NULL;
  PyArrayObject *x_arr    = NULL;
  PyObject      *ctrl_obj = NULL;
  PyArrayObject *ctrl_arr = NULL;
  PyObject      *out_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OO|O", kwlist, &x_obj,
                                    &ctrl_obj, &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  ctrl_arr = (PyArrayObject *)PyArray_FROM_OTF (ctrl_obj, NPY_COMPLEX64,
                                                NPY_ARRAY_C_CONTIGUOUS);
  if (!ctrl_arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  if (PyArray_SIZE (ctrl_arr) < PyArray_SIZE (x_arr))
    {
      Py_DECREF (x_arr);
      Py_DECREF (ctrl_arr);
      PyErr_SetString (PyExc_ValueError, "ctrl must be at least as long as x");
      return NULL;
    }

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). Hand-written
       * here because this fragment stays hand-owned; keep in step with
       * jm's generated form (gh-581). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = Resampler_execute_ctrl_max_out (self->handle);
      size_t _n_in    = (size_t)PyArray_SIZE (x_arr);
      size_t _min_cap = _omax > _n_in ? _omax : _n_in;
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          return NULL;
        }
      size_t n_out = Resampler_execute_ctrl (
          self->handle, (const float complex *)PyArray_DATA (x_arr), _n_in,
          (const float complex *)PyArray_DATA (ctrl_arr),
          (size_t)PyArray_SIZE (ctrl_arr),
          (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (x_arr);
      Py_DECREF (ctrl_arr);
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

  if (!self->_execute_ctrl_buf)
    {
      /* Resampler_execute_ctrl_max_out() always returns the fixed
       * RESAMPLER_MAX_OUT (65536) regardless of input size -- same
       * fixed-capacity contract as execute(); no growth path needed. */
      size_t _max = Resampler_execute_ctrl_max_out (self->handle);
      if (!_max)
        _max = (size_t)PyArray_SIZE (x_arr);
      self->_execute_ctrl_buf     = malloc (_max * sizeof (float complex));
      self->_execute_ctrl_buf_cap = _max;
      if (!self->_execute_ctrl_buf)
        {
          Py_DECREF (x_arr);
          Py_DECREF (ctrl_arr);
          PyErr_NoMemory ();
          return NULL;
        }
    }
  size_t n_out = Resampler_execute_ctrl (
      self->handle, (const float complex *)PyArray_DATA (x_arr),
      (size_t)PyArray_SIZE (x_arr),
      (const float complex *)PyArray_DATA (ctrl_arr),
      (size_t)PyArray_SIZE (ctrl_arr), self->_execute_ctrl_buf,
      self->_execute_ctrl_buf_cap);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64,
                                             self->_execute_ctrl_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  Py_DECREF (ctrl_arr);
  return arr;
}

static PyObject *
ResamplerObj_reset (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Resampler_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
Resampler_getprop_rate (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (Resampler_get_rate (self->handle));
}
static int
Resampler_setprop_rate (ResamplerObject *self, PyObject *value,
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
  Resampler_set_rate (self->handle, v);
  return 0;
}
static PyObject *
Resampler_getprop_num_phases (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)Resampler_get_num_phases (self->handle));
}
static PyObject *
Resampler_getprop_num_taps (ResamplerObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)Resampler_get_num_taps (self->handle));
}

static PyGetSetDef Resampler_getset[] = {
  { "rate", (getter)Resampler_getprop_rate, (setter)Resampler_setprop_rate,
    "Get / set the output-to-input sample rate ratio. The setter recomputes "
    "the phase increment immediately; the delay line and phase accumulator "
    "are preserved so in-stream rate changes are glitch-free. Switching sign "
    "of (rate - 1) (i.e. crossing the boundary between interp and decim "
    "modes) requires a fresh create().\n",
    NULL },
  { "num_phases", (getter)Resampler_getprop_num_phases, NULL,
    "Number of polyphase branches in the filter bank. Always a power of two. "
    "The built-in bank has 4096 phases giving sub-sample timing resolution of "
    "1/4096 of an input sample period.\n",
    NULL },
  { "num_taps", (getter)Resampler_getprop_num_taps, NULL,
    "Taps per polyphase branch. Total prototype filter length is num_phases * "
    "num_taps - 1. The built-in bank uses 19 taps per branch.\n",
    NULL },
  { NULL }
};

static PyObject *
ResamplerObj_destroy (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      Resampler_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ResamplerObj_enter (ResamplerObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ResamplerObj_exit (ResamplerObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      Resampler_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): the standard state triplet, generated by the
 * shared macro (see dp_state_pyhelp.h) — byte-identical to jm's output.
 * The matching PyMethodDef rows are below. */
DP_PY_STATE_METHODS (ResamplerObj, ResamplerObject, self->handle, Resampler)

static PyMethodDef ResamplerObj_methods[] = {

  { "execute", (PyCFunction)(void *)ResamplerObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x, out=None) -> ndarray\n"
    "\n"
    "Resample x(0..x_len-1) into out(0..n_out-1). Without out=, the\n"
    "returned array is a view into a buffer reused on the next call\n"
    "(see execute_max_out() to size an out= buffer for an independent,\n"
    "alias-free result).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Resampler\n"
    "    >>> obj = Resampler(0.0)\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_max_out", (PyCFunction)ResamplerObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "execute_ctrl", (PyCFunction)(void *)ResamplerObj_execute_ctrl,
    METH_VARARGS | METH_KEYWORDS,
    "execute_ctrl(x, ctrl, out=None) -> ndarray\n"
    "\n"
    "Resample with per-sample rate deviations. Without out=, the\n"
    "returned array is a view into a buffer reused on the next call\n"
    "(see execute_ctrl_max_out() to size an out= buffer for an\n"
    "independent, alias-free result).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Resampler\n"
    "    >>> obj = Resampler(0.0)\n"
    "    >>> y = obj.execute_ctrl(np.zeros(4), np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "execute_ctrl_max_out", (PyCFunction)ResamplerObj_execute_ctrl_max_out,
    METH_NOARGS,
    "execute_ctrl_max_out() -> int\n\nMax output length execute_ctrl() can "
    "produce for the current state.\nUse to size the ``out=`` buffer." },
  { "reset", (PyCFunction)ResamplerObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero delay line and phase accumulator.  Rate and bank preserved.\n"
    "\n"
    "    >>> from doppler import Resampler\n"
    "    >>> obj = Resampler(0.0)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)ResamplerObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the ResamplerObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)ResamplerObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the ResamplerObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)ResamplerObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the ResamplerObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)ResamplerObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)ResamplerObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Resampler be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Resampler\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ResamplerObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Resampler.\n"
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

static PyTypeObject ResamplerObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.Resampler",
  .tp_basicsize                           = sizeof (ResamplerObject),
  .tp_dealloc                             = (destructor)ResamplerObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a Resampler with the built-in 4096×19 Kaiser bank.\n",
  .tp_methods = ResamplerObj_methods,
  .tp_getset  = Resampler_getset,
  .tp_new     = ResamplerObj_new,
  .tp_init    = (initproc)ResamplerObj_init,
};
