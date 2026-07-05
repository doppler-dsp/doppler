/*
 * wfm_ext_pn.c — PN type for the wfm module.
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
  void   **_generate_retired; /* gh-219 deferred free */
  size_t   _generate_retired_n;
  size_t   _generate_retired_cap;
} PNObject;

static void
PNObj_dealloc (PNObject *self)
{
  if (self->handle)
    pn_destroy (self->handle);
  free (self->_generate_buf);
  for (size_t _i = 0; _i < self->_generate_retired_n; _i++)
    free (self->_generate_retired[_i]);
  free (self->_generate_retired);
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
  /* Hand-patched param order: jm's regeneration sorts the string_enum
   * param (lfsr) to the front of the positional list, breaking the
   * manifest-declared poly/seed/length/lfsr order that PN(96, 1, 7)-style
   * positional construction (and every existing caller/test) relies on.
   * Defaults also hand-patched: the manifest's poly=96/seed=1/length=7
   * predate the poly=0-means-auto-select-MLS fix (#191) and don't match
   * shipped/tested behavior — poly omitted must equal poly=0. */
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
                    "lfsr must be one of \"galois\", \"fibonacci\", got '%s'",
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
PNObj_generate_max_out (PNObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (pn_generate_max_out (self->handle));
}

static PyObject *
PNObj_generate (PNObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "count", "out", NULL };
  Py_ssize_t   n         = 1;
  PyObject    *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|nO", _kwlist, &n, &out_obj))
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = pn_generate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t    n_out  = pn_generate (self->handle, (size_t)n,
                                      (uint8_t *)PyArray_DATA (out_arr));
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_UINT8,
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
  if (!self->_generate_buf || self->_generate_buf_cap < _need)
    {
      size_t _max = pn_generate_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_generate_buf
          && self->_generate_retired_n == self->_generate_retired_cap)
        {
          size_t _rcap = self->_generate_retired_cap
                             ? self->_generate_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_generate_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_generate_retired     = _rt;
          self->_generate_retired_cap = _rcap;
        }
      uint8_t *_tmp = malloc (_max * sizeof (uint8_t));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_generate_buf)
        self->_generate_retired[self->_generate_retired_n++]
            = self->_generate_buf;
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

static PyMethodDef PNObj_methods[] = {
  { "reset", (PyCFunction)PNObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "generate", (PyCFunction)PNObj_generate, METH_VARARGS | METH_KEYWORDS,
    "generate(n=1) -> ndarray\n"
    "\n"
    "Generate ``n`` chips into ``out`` and advance the LFSR by ``n`` "
    "positions.  Each element of ``out`` is 0 or 1.  Requesting more than one "
    "MLS period is valid — the sequence simply wraps around.  The Python "
    "binding returns a zero-copy NumPy uint8 view over a pre-allocated "
    "buffer; copy the result before calling generate again if you need a "
    "snapshot.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import PN\n"
    "    >>> obj = PN(\"galois\", 96, 1, 7)\n"
    "    >>> y = obj.generate(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "generate_max_out", (PyCFunction)PNObj_generate_max_out, METH_NOARGS,
    "generate_max_out() -> int\n\nMax output length generate() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)PNObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)PNObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)PNObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)PNObj_destroy, METH_NOARGS, "Release resources." },
  { "__enter__", (PyCFunction)PNObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)PNObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject PNObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.PN",
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
