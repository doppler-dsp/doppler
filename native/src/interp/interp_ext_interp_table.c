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
} InterpolatedTableObject;

static void
InterpolatedTableObj_dealloc (InterpolatedTableObject *self)
{
  if (self->handle)
    interp_table_destroy (self->handle);
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
      /* Require the exact dtype AND C-contiguity — either mismatch makes
       * the marshal write into a temp copy, not the caller's buffer. */
      if (!PyArray_Check (out_obj)
          || PyArray_TYPE ((PyArrayObject *)out_obj) != NPY_COMPLEX128
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
          (double complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _need = (size_t)n;
  size_t _cap  = interp_table_execute_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX128);
  if (!arr0)
    {
      Py_DECREF (in_arr);
      return NULL;
    }
  double complex *_d0 = (double complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t n_out = interp_table_execute (self->handle,
                                       (const double *)PyArray_DATA (in_arr),
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
    "No-op: InterpolatedTable is purely a function of (table, method, point) "
    "with no running state to reset." },

  { "execute", (PyCFunction)(void *)InterpolatedTableObj_execute,
    METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Evaluate the table at each of n_in points via periodic interpolation.\n"
    "\n"
    "Each point is wrapped mod the table length (any real value, any sign)\n"
    "and evaluated per the configured method:\n"
    "\n"
    "- floor:   nearest index below (`table[floor(point) mod n]`)\n"
    "- nearest: closer of the floor/next index (0.5 ties pick floor)\n"
    "- linear:  linear fit across the two bracketing indices\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : float\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex128]\n"
    "    min(n_in, max_out) interpolated points.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.interp import InterpolatedTable\n"
    ">>> import numpy as np\n"
    ">>> ramp = InterpolatedTable(\n"
    "...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))\n"
    ">>> ramp.execute(np.array([0.5, 1.1]))\n"
    "array([0.5+0.j, 1.1+0.j])\n" },
  { "execute_max_out", (PyCFunction)InterpolatedTableObj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)InterpolatedTableObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)InterpolatedTableObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a InterpTable be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "InterpTable\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)InterpolatedTableObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the InterpTable.\n"
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
