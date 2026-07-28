/*
 * interp_ext_interp_table.c — InterpolatedTable type for the interp module.
 *
 * Included by interp_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only interp_ext.c is compiled.
 */
/* ======================================================== */
/* InterpolatedTableObject — wraps interp_table_state_t *       */
/* ======================================================== */

#include "interp_table/interp_table_core.h"

typedef struct
{
  PyObject_HEAD interp_table_state_t *handle;
  double complex *_execute_buf;     /* pre-allocated output for execute */
  size_t          _execute_buf_cap; /* allocated capacity for execute */
  void          **_execute_retired; /* gh-219 deferred free */
  size_t          _execute_retired_n;
  size_t          _execute_retired_cap;
  PyObject       *_execute_view_ref; /* gh-437 last returned view */
} InterpolatedTableObject;

static void
InterpolatedTableObj_dealloc (InterpolatedTableObject *self)
{
  if (self->handle)
    interp_table_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_XDECREF (self->_execute_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
InterpolatedTableObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  InterpolatedTableObject *self
      = (InterpolatedTableObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
InterpolatedTableObj_init (InterpolatedTableObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[]   = { "table", "method", NULL };
  PyObject    *table_obj  = NULL;
  const char  *method_str = "linear";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|s", kwlist, &table_obj,
                                    &method_str))
    return -1;
  int method = 0;
  if (strcmp (method_str, "floor") == 0)
    method = 0;
  else if (strcmp (method_str, "nearest") == 0)
    method = 1;
  else if (strcmp (method_str, "linear") == 0)
    method = 2;
  else
    {
      PyErr_Format (
          PyExc_ValueError,
          "method must be one of \"floor\", \"nearest\", \"linear\", got '%s'",
          method_str);
      return -1;
    }
  PyArrayObject *table_arr = (PyArrayObject *)PyArray_FROM_OTF (
      table_obj, NPY_COMPLEX128, NPY_ARRAY_C_CONTIGUOUS);
  if (!table_arr)
    {
      return -1;
    }
  size_t table_len = (size_t)PyArray_SIZE (table_arr);
  self->handle     = interp_table_create (
      (const double complex *)PyArray_DATA (table_arr), table_len, method);
  Py_DECREF (table_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "interp_table_create returned NULL");
      return -1;
    }
  {
    size_t _max = interp_table_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (double complex));
        if (!self->_execute_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_execute_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
InterpolatedTableObj_reset (InterpolatedTableObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  interp_table_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
InterpolatedTableObj_execute_max_out (InterpolatedTableObject *self,
                                      PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (interp_table_execute_max_out (self->handle));
}

static PyObject *
InterpolatedTableObj_execute (InterpolatedTableObject *self, PyObject *args,
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
  PyArrayObject *in_arr = (PyArrayObject *)PyArray_FROM_OTF (
      in_obj, NPY_DOUBLE, NPY_ARRAY_C_CONTIGUOUS);
  if (!in_arr)
    return NULL;
  Py_ssize_t n = PyArray_SIZE (in_arr);
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact output dtype — no silent cast (a cast writes
       * into a temp copy instead of the caller's buffer). */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX128
          || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
        {
          PyErr_SetString (
              PyExc_TypeError,
              "out must be a writable ndarray of the output dtype");
          Py_DECREF (in_arr);
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX128,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = interp_table_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (in_arr);
          return NULL;
        }
      size_t n_out = interp_table_execute (
          self->handle, (const double *)PyArray_DATA (in_arr), (size_t)n,
          (double complex *)PyArray_DATA (out_arr));
      Py_DECREF (in_arr);
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_COMPLEX128,
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
  if (self->_execute_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_execute_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_execute_view_ref) != Py_None;
#endif
    }
  if (!self->_execute_buf || self->_execute_buf_cap < _need || _view_live)
    {
      size_t _max = interp_table_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_execute_buf
          && self->_execute_retired_n == self->_execute_retired_cap)
        {
          size_t _rcap = self->_execute_retired_cap
                             ? self->_execute_retired_cap * 2
                             : 4;
          void **_rt
              = realloc (self->_execute_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              Py_DECREF (in_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      double complex *_tmp = malloc (_max * sizeof (double complex));
      if (!_tmp)
        {
          Py_DECREF (in_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  size_t   n_out = interp_table_execute (self->handle,
                                         (const double *)PyArray_DATA (in_arr),
                                         (size_t)n, self->_execute_buf);
  npy_intp dim   = (npy_intp)n_out;
  PyObject *arr  = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX128,
                                              self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_execute_view_ref);
  self->_execute_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_execute_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  Py_DECREF (in_arr);
  return arr;
}
static PyObject *
InterpolatedTable_getprop_n (InterpolatedTableObject *self,
                             void                    *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}

static PyGetSetDef InterpolatedTable_getset[]
    = { { "n", (getter)InterpolatedTable_getprop_n, NULL,
          "Table length (one period), read-only.\n", NULL },
        { NULL } };

static PyObject *
InterpolatedTableObj_destroy (InterpolatedTableObject *self,
                              PyObject                *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      interp_table_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
InterpolatedTableObj_enter (InterpolatedTableObject *self,
                            PyObject                *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
InterpolatedTableObj_exit (InterpolatedTableObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      interp_table_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef InterpolatedTableObj_methods[] = {
  { "reset", (PyCFunction)InterpolatedTableObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "execute", (PyCFunction)InterpolatedTableObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Evaluate the table at each of n_in points via periodic interpolation.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import InterpolatedTable\n"
    "    >>> obj = InterpolatedTable(np.zeros(1, dtype=np.complex128), "
    "\"linear\")\n"
    "    >>> y = obj.execute(1.0)\n"
    "    >>> y.dtype\n"
    "    dtype('complex128')\n" },
  { "execute_max_out", (PyCFunction)InterpolatedTableObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)InterpolatedTableObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)InterpolatedTableObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)InterpolatedTableObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject InterpolatedTableObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "interp.InterpolatedTable",
  .tp_basicsize                           = sizeof (InterpolatedTableObject),
  .tp_dealloc = (destructor)InterpolatedTableObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create an InterpolatedTable instance.\n",
  .tp_methods = InterpolatedTableObj_methods,
  .tp_getset  = InterpolatedTable_getset,
  .tp_new     = InterpolatedTableObj_new,
  .tp_init    = (initproc)InterpolatedTableObj_init,
};
