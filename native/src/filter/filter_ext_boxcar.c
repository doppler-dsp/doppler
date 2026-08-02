/*
 * filter_ext_boxcar.c — MovingAverage type for the filter module.
 *
 * Included by filter_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only filter_ext.c is compiled.
 */
/* ======================================================== */
/* MovingAverageObject — wraps boxcar_state_t *       */
/* ======================================================== */

#include "boxcar/boxcar_core.h"

typedef struct
{
  PyObject_HEAD boxcar_state_t *handle;
} MovingAverageObject;

static void
MovingAverageObj_dealloc (MovingAverageObject *self)
{
  if (self->handle)
    boxcar_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MovingAverageObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MovingAverageObject *self = (MovingAverageObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MovingAverageObj_init (MovingAverageObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char       *kwlist[] = { "len", "gain", NULL };
  unsigned long long len_raw  = 4;
  double             gain     = 1.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kd", kwlist, &len_raw,
                                    &gain))
    return -1;
  size_t len   = (size_t)len_raw;
  self->handle = boxcar_create (len, gain);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "boxcar_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
MovingAverage_step (MovingAverageObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_complex x_raw = { 0.0, 0.0 };
  if (!PyArg_ParseTuple (args, "D", &x_raw))
    return NULL;
  float complex x = (float)x_raw.real + (float)x_raw.imag * I;
  float complex y = boxcar_step (self->handle, x);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}

static PyObject *
MovingAverage_steps (MovingAverageObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *kwlist[] = { "x", "out", NULL };
  PyObject    *in_obj   = NULL;
  PyObject    *out_obj  = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", kwlist, &in_obj,
                                    &out_obj))
    return NULL;

  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      if (PyArray_SIZE (out_arr) != n)
        {
          PyErr_Format (PyExc_ValueError, "out length %zd != input length %zd",
                        (Py_ssize_t)PyArray_SIZE (out_arr), (Py_ssize_t)n);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      boxcar_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
                    (float complex *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  boxcar_steps (self->handle, (const float complex *)PyArray_DATA (in_arr),
                (float complex *)PyArray_DATA ((PyArrayObject *)out_arr),
                (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
MovingAverageObj_reset (MovingAverageObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  boxcar_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
MovingAverageObj_state_bytes (MovingAverageObject *self,
                              PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (boxcar_state_bytes (self->handle));
}

static PyObject *
MovingAverageObj_get_state (MovingAverageObject *self,
                            PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = boxcar_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  boxcar_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
MovingAverageObj_set_state (MovingAverageObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != boxcar_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (boxcar_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MovingAverage_getprop_len (MovingAverageObject *self,
                           void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->len);
}
static PyObject *
MovingAverage_getprop_gain (MovingAverageObject *self,
                            void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (boxcar_get_gain (self->handle));
}
static int
MovingAverage_setprop_gain (MovingAverageObject *self, PyObject *value,
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
  boxcar_set_gain (self->handle, v);
  return 0;
}

static PyGetSetDef MovingAverage_getset[]
    = { { "len", (getter)MovingAverage_getprop_len, NULL,
          "window length (1 .. BOXCAR_MAX_LEN).\n", NULL },
        { "gain", (getter)MovingAverage_getprop_gain,
          (setter)MovingAverage_setprop_gain, "Current output gain.\n", NULL },
        { NULL } };

static PyObject *
MovingAverageObj_destroy (MovingAverageObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      boxcar_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MovingAverageObj_enter (MovingAverageObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MovingAverageObj_exit (MovingAverageObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      boxcar_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef MovingAverageObj_methods[] = {
  { "step", (PyCFunction)MovingAverage_step, METH_VARARGS,
    "step(x) -> float complex\n"
    "\n"
    "Slide the window by one sample; return the gained moving average.\n"
    "\n"
    "O(1): add x, drop the sample leaving the window, return `acc · scale` "
    "(=\n"
    "`gain · acc / len`) — one multiply.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : complex\n"
    "    One input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "complex\n"
    "    The gained window mean after admitting x.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.filter import MovingAverage\n"
    ">>> ma = MovingAverage(2)   # 2-sample sliding window, unit gain\n"
    ">>> [round(ma.step(v).real, 4) for v in (1 + 0j, 3 + 0j, 3 + 0j)]\n"
    "[0.5, 2.0, 3.0]\n"
    "\n" },
  { "steps", (PyCFunction)(void *)MovingAverage_steps,
    METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Filter a block: write the gained moving average of each sample.\n"
    "\n"
    "Applies boxcar_step() to each input sample in turn, so the window sum\n"
    "and ring carry across the block exactly as they would sample by sample "
    "—\n"
    "a stream can be processed in frames of any size with no seam.\n"
    "Immediately after a reset the first len-1 outputs average over a "
    "partial\n"
    "(still filling) window and ramp in.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input samples.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.filter import MovingAverage\n"
    ">>> ma = MovingAverage(3)                          # 3-sample window\n"
    ">>> x = np.ones(5, np.complex64)                   # unit step input\n"
    ">>> [round(v, 4) for v in ma.steps(x).real.tolist()]\n"
    "[0.3333, 0.6667, 1.0, 1.0, 1.0]\n"
    "\n" },

  { "reset", (PyCFunction)MovingAverageObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Clear the window (zero the ring and the running sum); keep the "
    "configured length and gain.\n"
    "\n"
    "Returns the filter to its just-constructed state: the delay ring and "
    "the\n"
    "running window sum are zeroed while len and gain are preserved, so the\n"
    "next len-1 outputs ramp in over a partial window exactly as they did on\n"
    "a fresh instance. Call it at a segment boundary so samples from one\n"
    "capture do not average into an unrelated next one.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.filter import MovingAverage\n"
    ">>> ma = MovingAverage(2)                         # 2-sample window\n"
    ">>> _ = ma.steps(np.ones(4, np.complex64))        # fill the window\n"
    ">>> ma.reset()                                    # clear it\n"
    ">>> round(ma.step(1 + 0j).real, 4)                # ramps in from empty\n"
    "0.5\n" },
  { "state_bytes", (PyCFunction)MovingAverageObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the MovingAverageObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)MovingAverageObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the MovingAverageObj has already been\n"
    "destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)MovingAverageObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the MovingAverageObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)MovingAverageObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)MovingAverageObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Boxcar be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Boxcar\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MovingAverageObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Boxcar.\n"
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

static PyTypeObject MovingAverageObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "filter.MovingAverage",
  .tp_basicsize                           = sizeof (MovingAverageObject),
  .tp_dealloc = (destructor)MovingAverageObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "MovingAverage type.\n",
  .tp_methods = MovingAverageObj_methods,
  .tp_getset  = MovingAverage_getset,
  .tp_new     = MovingAverageObj_new,
  .tp_init    = (initproc)MovingAverageObj_init,
};
