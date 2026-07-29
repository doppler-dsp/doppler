/*
 * resample_ext_cic.c — CIC type for the resample module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* CICObject — wraps cic_state_t *       */
/* ======================================================== */

#include "cic/cic_core.h"

typedef struct
{
  PyObject_HEAD cic_state_t *handle;
} CICObject;

static void
CICObj_dealloc (CICObject *self)
{
  if (self->handle)
    cic_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CICObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CICObject *self = (CICObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CICObj_init (CICObject *self, PyObject *args, PyObject *kwds)
{
  static char  *kwlist[] = { "R", NULL };
  unsigned long R_raw    = 16;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|k", kwlist, &R_raw))
    return -1;
  uint32_t R   = (uint32_t)R_raw;
  self->handle = cic_create (R);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "cic_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
CICObj_reset (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  cic_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CICObj_reconfigure (CICObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "R", NULL };
  unsigned long R_raw     = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "k", _kwlist, &R_raw))
    return NULL;
  uint32_t R = (uint32_t)R_raw;
  cic_reconfigure (self->handle, R);
  Py_RETURN_NONE;
}

static PyObject *
CICObj_decimate_max_out (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (cic_decimate_max_out (self->handle));
}

static PyObject *
CICObj_decimate (CICObject *self, PyObject *args, PyObject *kwds)
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
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
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = cic_decimate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = cic_decimate (
          self->handle, (const float complex *)PyArray_DATA (in_arr),
          (size_t)n, (float complex *)PyArray_DATA (out_arr), _cap);
      Py_DECREF (in_arr);
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
  size_t _need = (size_t)n;
  size_t _cap  = cic_decimate_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  float complex *_d0 = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = cic_decimate (self->handle,
                               (const float complex *)PyArray_DATA (in_arr),
                               (size_t)n, _d0, _cap);
  Py_DECREF (in_arr);
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
CICObj_state_bytes (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (cic_state_bytes (self->handle));
}

static PyObject *
CICObj_get_state (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = cic_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  cic_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
CICObj_set_state (CICObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != cic_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (cic_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
CIC_getprop_R (CICObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->R);
}
static PyObject *
CIC_getprop_shift (CICObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLong ((unsigned long)self->handle->shift);
}
static PyObject *
CIC_getprop_clipped (CICObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyBool_FromLong ((long)(self->handle->clipped));
}

static PyGetSetDef CIC_getset[] = {
  { "R", (getter)CIC_getprop_R, NULL, "R.\n", NULL },
  { "shift", (getter)CIC_getprop_shift, NULL, "Shift.\n", NULL },
  { "clipped", (getter)CIC_getprop_clipped, NULL,
    "True if any input component has exceeded the +-1.0 bound since the last "
    "reset(). Sticky, and free to read: the CIC's boundary comparisons run on "
    "every sample anyway, so it records something the sample stream cannot "
    "tell you -- a clipped stream still looks entirely plausible (finite, no "
    "NaN, merely distorted), so this flag is the only reliable check.\n",
    NULL },
  { NULL }
};

static PyObject *
CICObj_destroy (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      cic_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CICObj_enter (CICObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CICObj_exit (CICObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      cic_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CICObj_methods[] = {
  { "reset", (PyCFunction)CICObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "reconfigure", (PyCFunction)(void *)CICObj_reconfigure,
    METH_VARARGS | METH_KEYWORDS,
    "reconfigure(R) -> None\n"
    "\n"
    "Change the decimation ratio in place and reset all filter state. "
    "Recomputes the normalisation shift (CIC_N * log2(R)) and zeros all "
    "accumulators so the filter behaves exactly like a freshly created one "
    "with the new R. Silently ignores R values that are not a power-of-two in "
    "`[2, 4096]` — the state is left unchanged in that case.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CIC\n"
    "    >>> obj = CIC(16)\n"
    "    >>> obj.reconfigure(0)\n" },
  { "decimate", (PyCFunction)(void *)CICObj_decimate,
    METH_VARARGS | METH_KEYWORDS,
    "decimate(x) -> ndarray\n"
    "\n"
    "Decimate a block of CF32 samples through the CIC pipeline. Each sample "
    "is converted to offset-binary UQ16, pushed through CIC_N integrators "
    "(unsigned wrapping), and when the phase counter reaches R the integrated "
    "value is passed through CIC_N M=1 comb stages and converted back to "
    "CF32.  State persists between calls. Feeding blocks that are multiples "
    "of R gives predictable output counts (exactly n_in/R samples per "
    "block).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import CIC\n"
    "    >>> obj = CIC(16)\n"
    "    >>> y = obj.decimate(1.0 + 0.0j)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "decimate_max_out", (PyCFunction)CICObj_decimate_max_out, METH_NOARGS,
    "decimate_max_out() -> int\n\nMax output length decimate() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)CICObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)CICObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)CICObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)CICObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)CICObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)CICObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject CICObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.CIC",
  .tp_basicsize                           = sizeof (CICObject),
  .tp_dealloc                             = (destructor)CICObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct "
    "on the heap and pre-computes the normalisation right-shift (CIC_N * "
    "log2(R) bits). All integrator and comb accumulators are zeroed; the "
    "first output arrives after R input samples. Returns NULL for invalid R "
    "or OOM. Input amplitude is bounded: |Re| and |Im| <= 1.0. A component "
    "beyond +-1.0 is clipped at the boundary before any filtering; the sample "
    "stream gives no sign of it, so check the sticky clipped flag. Unlike "
    "doppler's floating-point blocks this one is not scale-free -- scale the "
    "input into range first.\n",
  .tp_methods = CICObj_methods,
  .tp_getset  = CIC_getset,
  .tp_new     = CICObj_new,
  .tp_init    = (initproc)CICObj_init,
};
