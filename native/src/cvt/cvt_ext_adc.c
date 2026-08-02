/*
 * cvt_ext_adc.c — ADC type for the cvt module.
 *
 * Included by cvt_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only cvt_ext.c is compiled.
 */
/* ======================================================== */
/* ADCObject — wraps adc_state_t *       */
/* ======================================================== */

#include "adc/adc_core.h"

typedef struct
{
  PyObject_HEAD adc_state_t *handle;
} ADCObject;

static void
ADCObj_dealloc (ADCObject *self)
{
  if (self->handle)
    adc_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ADCObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ADCObject *self = (ADCObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ADCObj_init (ADCObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "bits", "dbfs", "dithering", NULL };
  int          bits      = 16;
  float        dbfs      = -10.0f;
  int          dithering = 0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|ifi", kwlist, &bits, &dbfs,
                                    &dithering))
    return -1;
  self->handle = adc_create (bits, dbfs, dithering);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "adc_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
ADCObj_reset (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  adc_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ADC_step (ADCObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float x;
  if (!PyArg_ParseTuple (args, "f", &x))
    return NULL;
  int64_t y = adc_step (self->handle, x);
  return PyLong_FromLongLong ((long long)y);
}

static PyObject *
ADC_steps (ADCObject *self, PyObject *args, PyObject *kwds)
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
      in_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;

  Py_ssize_t n = PyArray_SIZE (in_arr);

  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_INT64
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_INT64, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
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
      adc_steps (self->handle, (const float *)PyArray_DATA (in_arr),
                 (int64_t *)PyArray_DATA (out_arr), (size_t)n);
      Py_DECREF (in_arr);
      return (PyObject *)out_arr;
    }

  npy_intp  dims[]  = { n };
  PyObject *out_arr = PyArray_SimpleNew (1, dims, NPY_INT64);
  if (!out_arr)
    {
      Py_DECREF (in_arr);
      return NULL;
    }

  adc_steps (self->handle, (const float *)PyArray_DATA (in_arr),
             (int64_t *)PyArray_DATA ((PyArrayObject *)out_arr), (size_t)n);

  Py_DECREF (in_arr);
  return out_arr;
}

static PyObject *
ADCObj_state_bytes (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (adc_state_bytes (self->handle));
}

static PyObject *
ADCObj_get_state (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = adc_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  adc_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
ADCObj_set_state (ADCObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != adc_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (adc_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
ADC_getprop_clipped (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}
static PyObject *
ADC_getprop_scale (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->scale);
}
static PyObject *
ADC_getprop_bits (ADCObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)self->handle->bits);
}

static PyGetSetDef ADC_getset[]
    = { { "clipped", (getter)ADC_getprop_clipped, NULL, "Clipped.\n", NULL },
        { "scale", (getter)ADC_getprop_scale, NULL, "Scale.\n", NULL },
        { "bits", (getter)ADC_getprop_bits, NULL, "Bits.\n", NULL },
        { NULL } };

static PyObject *
ADCObj_destroy (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      adc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ADCObj_enter (ADCObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ADCObj_exit (ADCObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      adc_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ADCObj_methods[] = {
  { "reset", (PyCFunction)ADCObj_reset, METH_NOARGS,
    "Clear the clip flag and re-seed the dither PRNG for a reproducible "
    "run." },
  { "step", (PyCFunction)ADC_step, METH_VARARGS,
    "step(x) -> int64_t\n"
    "\n"
    "Quantise one float sample to a signed N-bit ADC code.\n"
    "\n"
    "Multiplies x by the pre-computed double-precision scale, optionally "
    "adds\n"
    "TPDF dither (when the object was built with dithering enabled), rounds\n"
    "with llround, and clamps to the signed integer range `[clip_min,\n"
    "clip_max]`. Latches the sticky clipped flag if the sample saturated. A\n"
    "sample at amplitude 10^(dbfs/20) reaches full scale.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input sample, normally a normalised float in `[-1, +1]`.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Signed ADC code in `[-(2^(bits-1)), 2^(bits-1)-1]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import ADC\n"
    ">>> adc = ADC(bits=8, dbfs=0.0, dithering=0)  # 8-bit, full scale at 0 "
    "dBFS\n"
    ">>> adc.step(0.5)            # 0.5 * 128 codes\n"
    "64\n"
    ">>> adc.step(2.0)            # beyond full scale -> clamps to +127\n"
    "127\n"
    ">>> adc.clipped              # sticky flag latched by the clamp\n"
    "True\n"
    "\n" },
  { "steps", (PyCFunction)(void *)ADC_steps, METH_VARARGS | METH_KEYWORDS,
    "steps(x[, out]) -> ndarray\n"
    "\n"
    "Process a block of float samples to int64.\n"
    "\n"
    "When dithering is disabled the float-to-double multiply can use SIMD\n"
    "widening (jm_simd.h); the int64_t conversion and clamp remain scalar.\n"
    "When dithering is enabled the loop is scalar to preserve sequential "
    "PRNG\n"
    "state. Accepts an optional pre-allocated output array; allocates a "
    "fresh\n"
    "one when output is NULL.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.float32]\n"
    "    Input sample.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.int64]\n"
    "    Output sample.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.cvt import ADC\n"
    ">>> import numpy as np\n"
    ">>> # ideal 12-bit ADC: full scale spans +-2**11 codes\n"
    ">>> ADC(12, 0.0, 0).steps(np.array([0.0, 0.5, 0.999, -1.0],\n"
    "...                                dtype=np.float32)).tolist()\n"
    "[0, 1024, 2046, -2048]\n"
    "\n" },

  { "state_bytes", (PyCFunction)ADCObj_state_bytes, METH_NOARGS,
    "Size in bytes of this object's serialized state.\n"
    "\n"
    "The exact length `get_state` returns and `set_state` requires. It\n"
    "depends on how the object was constructed (state arrays are sized at\n"
    "construction), so read it from the instance rather than assuming a\n"
    "constant.\n"
    "\n"
    "Raises ``RuntimeError`` if the ADCObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Byte length of one serialized state blob.\n" },
  { "get_state", (PyCFunction)ADCObj_get_state, METH_NOARGS,
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
    "Raises ``RuntimeError`` if the ADCObj has already been destroyed.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Opaque snapshot, `state_bytes()` bytes long.\n" },
  { "set_state", (PyCFunction)ADCObj_set_state, METH_O,
    "Restore mutable state from a `get_state()` blob.\n"
    "\n"
    "Overwrites the live state in place; the object keeps the parameters it\n"
    "was constructed with. Length is validated against `state_bytes()` "
    "before\n"
    "the blob is handed to the C core, and the core may reject it as well.\n"
    "\n"
    "Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its\n"
    "length differs from `state_bytes()` or the core rejects it, and\n"
    "``RuntimeError`` if the ADCObj has already been destroyed.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "blob : bytes\n"
    "    A `get_state()` blob from this type, exactly `state_bytes()` "
    "long.\n" },
  { "destroy", (PyCFunction)ADCObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)ADCObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Adc be used in a `with` statement so its C resources are "
    "released\n"
    "deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Adc\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)ADCObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Adc.\n"
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

static PyTypeObject ADCObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "cvt.ADC",
  .tp_basicsize                           = sizeof (ADCObject),
  .tp_dealloc                             = (destructor)ADCObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Create an ADC instance.\n",
  .tp_methods                             = ADCObj_methods,
  .tp_getset                              = ADC_getset,
  .tp_new                                 = ADCObj_new,
  .tp_init                                = (initproc)ADCObj_init,
};
