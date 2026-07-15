/*
 * wfm_ext_gold.c — Gold type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* GoldObject — wraps gold_state_t *       */
/* ======================================================== */

#include "gold/gold_core.h"

typedef struct
{
  PyObject_HEAD gold_state_t *handle;
  uint8_t  *_generate_buf;     /* pre-allocated output for generate */
  size_t    _generate_buf_cap; /* allocated capacity for generate */
  void    **_generate_retired; /* gh-219 deferred free */
  size_t    _generate_retired_n;
  size_t    _generate_retired_cap;
  PyObject *_generate_view_ref; /* gh-437 last returned view */
} GoldObject;

static void
GoldObj_dealloc (GoldObject *self)
{
  if (self->handle)
    gold_destroy (self->handle);
  free (self->_generate_buf);
  for (size_t _i = 0; _i < self->_generate_retired_n; _i++)
    free (self->_generate_retired[_i]);
  free (self->_generate_retired);
  Py_XDECREF (self->_generate_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
GoldObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  GoldObject *self = (GoldObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
GoldObj_init (GoldObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "taps_a", "seed_a", "taps_b", "seed_b", "length", NULL };
  unsigned long long taps_a_raw = 934;
  unsigned long long seed_a_raw = 350;
  unsigned long long taps_b_raw = 567;
  unsigned long long seed_b_raw = 73;
  unsigned long      length_raw = 10;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KKKKk", kwlist, &taps_a_raw,
                                    &seed_a_raw, &taps_b_raw, &seed_b_raw,
                                    &length_raw))
    return -1;
  uint64_t taps_a = (uint64_t)taps_a_raw;
  uint64_t seed_a = (uint64_t)seed_a_raw;
  uint64_t taps_b = (uint64_t)taps_b_raw;
  uint64_t seed_b = (uint64_t)seed_b_raw;
  uint32_t length = (uint32_t)length_raw;
  self->handle    = gold_create (taps_a, seed_a, taps_b, seed_b, length);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "gold_create returned NULL");
      return -1;
    }
  {
    size_t _max = gold_generate_max_out (self->handle);
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
GoldObj_reset (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  gold_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_generate_max_out (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (gold_generate_max_out (self->handle));
}

static PyObject *
GoldObj_generate (GoldObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = gold_generate_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t    n_out  = gold_generate (self->handle, (size_t)n,
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
  size_t _need      = (size_t)n;
  int    _view_live = 0;
  if (self->_generate_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_generate_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_generate_view_ref) != Py_None;
#endif
    }
  if (!self->_generate_buf || self->_generate_buf_cap < _need || _view_live)
    {
      size_t _max = gold_generate_max_out (self->handle);
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
  size_t n_out  = gold_generate (self->handle, (size_t)n, self->_generate_buf);
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_UINT8, self->_generate_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_generate_view_ref);
  self->_generate_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_generate_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  return arr;
}

static PyObject *
GoldObj_state_bytes (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (gold_state_bytes (self->handle));
}

static PyObject *
GoldObj_get_state (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = gold_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  gold_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
GoldObj_set_state (GoldObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != gold_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (gold_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_destroy (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      gold_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
GoldObj_enter (GoldObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
GoldObj_exit (GoldObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      gold_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef GoldObj_methods[] = {
  { "reset", (PyCFunction)GoldObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "generate", (PyCFunction)GoldObj_generate, METH_VARARGS | METH_KEYWORDS,
    "generate(n=1) -> ndarray\n"
    "\n"
    "Generate ``n`` chips into ``out`` and advance both LFSRs by ``n`` "
    "positions. Each element of ``out`` is 0 or 1. Requesting more than one "
    "period is valid — the sequence simply wraps around. The Python binding "
    "returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy "
    "the result before calling generate again if you need a snapshot.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Gold\n"
    "    >>> obj = Gold(934, 350, 567, 73, 10)\n"
    "    >>> y = obj.generate(4)\n"
    "    >>> y.dtype\n"
    "    dtype('uint8')\n" },
  { "generate_max_out", (PyCFunction)GoldObj_generate_max_out, METH_NOARGS,
    "generate_max_out() -> int\n\nMax output length generate() can produce "
    "for the current state.\nUse to size the ``out=`` buffer." },
  { "state_bytes", (PyCFunction)GoldObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)GoldObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)GoldObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)GoldObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)GoldObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)GoldObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject GoldObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.Gold",
  .tp_basicsize                           = sizeof (GoldObject),
  .tp_dealloc                             = (destructor)GoldObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate and initialise a CCSDS-style Gold code generator. Two "
            "independent Fibonacci LFSRs of the same ``length`` free-run in "
            "lock-step; each output chip is the XOR of both registers' "
            "current top-bit (stage ``length``, i.e. bit ``length-1``). Both "
            "registers shift left one bit per chip: the new bit (parity of "
            "the tapped stages, read *before* the shift) enters at stage 1 "
            "(bit 0), and the old stage-``length`` bit is discarded after "
            "being XORed into the output. The sequence period is ``2^length - "
            "1`` for primitive ``taps_a``/``taps_b``.\n",
  .tp_methods = GoldObj_methods,
  .tp_new     = GoldObj_new,
  .tp_init    = (initproc)GoldObj_init,
};
