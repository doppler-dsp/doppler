/*
 * wfm_ext_pn.c — PN type for the wfmgen module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* PNObject — wraps pn_state_t *       */
/* ======================================================== */

#include "pn/pn_core.h"

typedef struct
{
  PyObject_HEAD pn_state_t *handle;
  uint8_t *_generate_buf;     /* pre-allocated output for generate */
  size_t   _generate_buf_cap; /* allocated capacity for generate */
} PNObject;

static void
PNObj_dealloc (PNObject *self)
{
  if (self->handle)
    pn_destroy (self->handle);
  free (self->_generate_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
PNObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PNObject *self = (PNObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
PNObj_init (PNObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]   = { "poly", "seed", "length", "lfsr", NULL };
  unsigned long long poly_raw   = 0ULL;
  unsigned long long seed_raw   = 0ULL;
  unsigned long      length_raw = 0UL;
  const char        *lfsr_str   = "galois";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKks", kwlist, &poly_raw,
                                    &seed_raw, &length_raw, &lfsr_str))
    return -1;
  int lfsr = 0;
  if (strcmp (lfsr_str, "galois") == 0)
    lfsr = 0;
  else if (strcmp (lfsr_str, "fibonacci") == 0)
    lfsr = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "lfsr must be \"galois\" or \"fibonacci\", got '%s'",
                    lfsr_str);
      return -1;
    }
  uint64_t poly   = (uint64_t)poly_raw;
  uint64_t seed   = (uint64_t)seed_raw;
  uint32_t length = (uint32_t)length_raw;
  /* poly=0 means auto-select the MLS primitive polynomial for the given
   * length, matching the Synth(pn_poly=0) convention. The MLS table starts
   * at n=2, so guard length >= 2 — for length < 2 there is no maximal-length
   * sequence and poly stays 0. */
  if (poly == 0 && length >= 2)
    poly = mls_poly (length);
  self->handle = pn_create (poly, seed, length, lfsr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "pn_create returned NULL");
      return -1;
    }
  {
    size_t _max = pn_generate_max_out (self->handle);
    if (_max)
      {
        self->_generate_buf = malloc (_max * sizeof (uint8_t));
        if (!self->_generate_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_generate_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
PNObj_reset (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  pn_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
PNObj_generate (PNObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 1;
  if (!PyArg_ParseTuple (args, "|n", &n))
    return NULL;
  size_t _need = (size_t)n;
  if (!self->_generate_buf || self->_generate_buf_cap < _need)
    {
      size_t _max = pn_generate_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      uint8_t *_tmp = realloc (self->_generate_buf, _max * sizeof (uint8_t));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      self->_generate_buf     = _tmp;
      self->_generate_buf_cap = _max;
    }
  size_t    n_out = pn_generate (self->handle, (size_t)n, self->_generate_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_generate_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  return arr;
}

static PyObject *
PNObj_destroy (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      pn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
PNObj_enter (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
PNObj_exit (PNObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      pn_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* serializable (gh-400): state-blob triplet, sibling to reset.  Hand-added
 * (this fragment is sacred — the view-returning generate); mirrors jm's
 * generated form for the `serializable` flag, which also emits the matching
 * wfm.pyi stubs. */
static PyObject *
PNObj_state_bytes (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (pn_state_bytes (self->handle));
}

static PyObject *
PNObj_get_state (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = pn_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  pn_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
PNObj_set_state (PNObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != pn_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (pn_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef PNObj_methods[]
    = { { "reset", (PyCFunction)PNObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "generate", (PyCFunction)PNObj_generate, METH_VARARGS,
          "generate(n=1) -> ndarray\n"
          "\n"
          "Zero-copy view into pre-allocated output buffer.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from doppler import PN\n"
          "    >>> obj = PN(96, 1, 7)\n"
          "    >>> y = obj.generate(4)\n"
          "    >>> y.dtype\n"
          "    dtype('uint8')\n" },
        { "state_bytes", (PyCFunction)PNObj_state_bytes, METH_NOARGS,
          "Serialized state size in bytes." },
        { "get_state", (PyCFunction)PNObj_get_state, METH_NOARGS,
          "Serialize the LFSR register to bytes." },
        { "set_state", (PyCFunction)PNObj_set_state, METH_O,
          "Restore the LFSR register from a get_state() blob." },
        { "destroy", (PyCFunction)PNObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)PNObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)PNObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject PNObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfmgen.PN",
  .tp_basicsize                           = sizeof (PNObject),
  .tp_dealloc                             = (destructor)PNObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Allocate and initialise a maximal-length-sequence LFSR. The register is "
    "seeded from ``seed`` and will produce a pseudo-random binary sequence "
    "with period 2^length - 1 for any primitive ``poly``. Both Galois and "
    "Fibonacci realizations share the same primitive polynomial and therefore "
    "the same period; they differ only in chip ordering/phase.\n",
  .tp_methods = PNObj_methods,
  .tp_new     = PNObj_new,
  .tp_init    = (initproc)PNObj_init,
};
