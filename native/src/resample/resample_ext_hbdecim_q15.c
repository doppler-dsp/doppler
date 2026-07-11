/*
 * resample_ext_hbdecim_q15.c — HalfbandDecimatorQ15 type for the resample
 * module.
 *
 * Included by resample_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only resample_ext.c is compiled.
 */
/* ======================================================== */
/* HalfbandDecimatorQ15Object — wraps hbdecim_q15_state_t *       */
/* ======================================================== */

#include "hbdecim_q15/hbdecim_q15_core.h"

typedef struct
{
  PyObject_HEAD hbdecim_q15_state_t *handle;
  int16_t *_execute_buf;     /* pre-allocated output for execute */
  size_t   _execute_buf_cap; /* allocated capacity for execute */
} HalfbandDecimatorQ15Object;

static void
HalfbandDecimatorQ15Obj_dealloc (HalfbandDecimatorQ15Object *self)
{
  if (self->handle)
    hbdecim_q15_destroy (self->handle);
  free (self->_execute_buf);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
HalfbandDecimatorQ15Obj_new (PyTypeObject *type, PyObject *args,
                             PyObject *kwds)
{
  HalfbandDecimatorQ15Object *self
      = (HalfbandDecimatorQ15Object *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
HalfbandDecimatorQ15Obj_init (HalfbandDecimatorQ15Object *self, PyObject *args,
                              PyObject *kwds)
{
  static char *kwlist[] = { "h", NULL };
  PyObject    *h_obj    = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", kwlist, &h_obj))
    return -1;
  PyArrayObject *h_arr = (PyArrayObject *)PyArray_FROM_OTF (
      h_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS);
  if (!h_arr)
    {
      return -1;
    }
  size_t h_len = (size_t)PyArray_SIZE (h_arr);
  self->handle
      = hbdecim_q15_create (h_len, (const float *)PyArray_DATA (h_arr));
  Py_DECREF (h_arr);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "hbdecim_q15_create returned NULL");
      return -1;
    }
  {
    size_t _max = hbdecim_q15_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (int16_t));
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
HalfbandDecimatorQ15Obj_execute (HalfbandDecimatorQ15Object *self,
                                 PyObject                   *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  PyObject      *x_obj = NULL;
  PyArrayObject *x_arr = NULL;
  if (!PyArg_ParseTuple (args, "O", &x_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_INT16,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_execute_buf || self->_execute_buf_cap < _need)
    {
      size_t _max = hbdecim_q15_execute_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      int16_t *_tmp = realloc (self->_execute_buf, _max * sizeof (int16_t));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  size_t n_in
      = (size_t)PyArray_SIZE (x_arr); /* int16_t count = 2 * complex samples */
  size_t n_out = hbdecim_q15_execute (
      self->handle, (const int16_t *)PyArray_DATA (x_arr),
      n_in / 2, /* complex sample count */
      self->_execute_buf, self->_execute_buf_cap / 2); /* complex capacity */
  /* NumPy owns the output: an independent array per call, copied from the
   * internal grow-on-demand buffer.  Returning a view of _execute_buf instead
   * dangled when a later, larger execute realloc'd it (the view pins self, not
   * the buffer) — the gh-219 use-after-free.  Matches the numpy-owned source
   * objects (lo/nco/awgn). */
  npy_intp  dim = (npy_intp)(n_out * 2); /* back to int16_t element count */
  PyObject *arr = PyArray_SimpleNew (1, &dim, NPY_INT16);
  if (!arr)
    {
      Py_DECREF (x_arr);
      return NULL;
    }
  memcpy (PyArray_DATA ((PyArrayObject *)arr), self->_execute_buf,
          (size_t)dim * sizeof (int16_t));
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
HalfbandDecimatorQ15Obj_reset (HalfbandDecimatorQ15Object *self,
                               PyObject                   *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  hbdecim_q15_reset (self->handle);
  Py_RETURN_NONE;
}
static PyObject *
HalfbandDecimatorQ15_getprop_num_taps (HalfbandDecimatorQ15Object *self,
                                       void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)hbdecim_q15_get_num_taps (self->handle));
}
static PyObject *
HalfbandDecimatorQ15_getprop_rate (HalfbandDecimatorQ15Object *self,
                                   void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (hbdecim_q15_get_rate (self->handle));
}

static PyGetSetDef HalfbandDecimatorQ15_getset[]
    = { { "num_taps", (getter)HalfbandDecimatorQ15_getprop_num_taps, NULL,
          "Returns num_taps as supplied to hbdecim_q15_create.\n", NULL },
        { "rate", (getter)HalfbandDecimatorQ15_getprop_rate, NULL,
          "Always returns 0.5.\n", NULL },
        { NULL } };

static PyObject *
HalfbandDecimatorQ15Obj_destroy (HalfbandDecimatorQ15Object *self,
                                 PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      hbdecim_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorQ15Obj_enter (HalfbandDecimatorQ15Object *self,
                               PyObject                   *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
HalfbandDecimatorQ15Obj_exit (HalfbandDecimatorQ15Object *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      hbdecim_q15_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorQ15Obj_state_bytes (HalfbandDecimatorQ15Object *self,
                                     PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (hbdecim_q15_state_bytes (self->handle));
}

static PyObject *
HalfbandDecimatorQ15Obj_get_state (HalfbandDecimatorQ15Object *self,
                                   PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = hbdecim_q15_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  hbdecim_q15_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
HalfbandDecimatorQ15Obj_set_state (HalfbandDecimatorQ15Object *self,
                                   PyObject                   *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != hbdecim_q15_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (hbdecim_q15_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
HalfbandDecimatorQ15Obj_execute_max_out (HalfbandDecimatorQ15Object *self,
                                         PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (hbdecim_q15_execute_max_out (self->handle));
}

static PyMethodDef HalfbandDecimatorQ15Obj_methods[] = {

  { "execute", (PyCFunction)HalfbandDecimatorQ15Obj_execute, METH_VARARGS,
    "execute(x) -> ndarray\n"
    "\n"
    "Decimate a block of interleaved IQ int16 samples by 2.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import HalfbandDecimatorQ15\n"
    "    >>> obj = HalfbandDecimatorQ15(np.zeros(1, dtype=np.float32))\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('int16')\n" },
  { "reset", (PyCFunction)HalfbandDecimatorQ15Obj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Zero all delay rings and clear the pending-sample flag.\n"
    "\n"
    "    >>> from doppler import HalfbandDecimatorQ15\n"
    "    >>> obj = HalfbandDecimatorQ15(np.zeros(1, dtype=np.float32))\n"
    "    >>> obj.reset()\n" },
  { "destroy", (PyCFunction)HalfbandDecimatorQ15Obj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)HalfbandDecimatorQ15Obj_enter, METH_NOARGS,
    NULL },
  { "__exit__", (PyCFunction)HalfbandDecimatorQ15Obj_exit, METH_VARARGS,
    NULL },
  { "state_bytes", (PyCFunction)HalfbandDecimatorQ15Obj_state_bytes,
    METH_NOARGS, "Serialized state size in bytes." },
  { "get_state", (PyCFunction)HalfbandDecimatorQ15Obj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)HalfbandDecimatorQ15Obj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "execute_max_out", (PyCFunction)HalfbandDecimatorQ15Obj_execute_max_out,
    METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { NULL }
};

static PyTypeObject HalfbandDecimatorQ15ObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "resample.HalfbandDecimatorQ15",
  .tp_basicsize = sizeof (HalfbandDecimatorQ15Object),
  .tp_dealloc   = (destructor)HalfbandDecimatorQ15Obj_dealloc,
  .tp_flags     = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Allocate and initialise a fixed-point halfband 2:1 decimator.\n",
  .tp_methods = HalfbandDecimatorQ15Obj_methods,
  .tp_getset  = HalfbandDecimatorQ15_getset,
  .tp_new     = HalfbandDecimatorQ15Obj_new,
  .tp_init    = (initproc)HalfbandDecimatorQ15Obj_init,
};
