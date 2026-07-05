/*
 * analyzer_ext_specan.c — Specan type for the analyzer module.
 *
 * Included by analyzer_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only analyzer_ext.c is compiled.
 */
/* ======================================================== */
/* SpecanObject — wraps specan_state_t *       */
/* ======================================================== */

#include "specan/specan_core.h"

typedef struct
{
  PyObject_HEAD specan_state_t *handle;
  float *_execute_buf;     /* pre-allocated output for execute */
  size_t _execute_buf_cap; /* allocated capacity for execute */
  void **_execute_retired; /* gh-219 deferred free */
  size_t _execute_retired_n;
  size_t _execute_retired_cap;
} SpecanObject;

static void
SpecanObj_dealloc (SpecanObject *self)
{
  if (self->handle)
    specan_destroy (self->handle);
  free (self->_execute_buf);
  for (size_t _i = 0; _i < self->_execute_retired_n; _i++)
    free (self->_execute_retired[_i]);
  free (self->_execute_retired);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
SpecanObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  SpecanObject *self = (SpecanObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
SpecanObj_init (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "fs",        "span",       "rbw",  "window", "src_center", "center",
          "offset_db", "full_scale", "bits", "navg",   NULL };
  double             fs         = 2.048e6;
  double             span       = 200e3;
  double             rbw        = 500.0;
  const char        *window_str = "kaiser";
  double             src_center = 0.0;
  double             center     = 0.0;
  double             offset_db  = 0.0;
  double             full_scale = 1.0;
  unsigned long long bits_raw   = 0;
  unsigned long long navg_raw   = 1;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "ddd|sddddKK", kwlist, &fs, &span, &rbw, &window_str,
          &src_center, &center, &offset_db, &full_scale, &bits_raw, &navg_raw))
    return -1;
  int window = 0;
  if (strcmp (window_str, "hann") == 0)
    window = 0;
  else if (strcmp (window_str, "kaiser") == 0)
    window = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "window must be one of \"hann\", \"kaiser\", got '%s'",
                    window_str);
      return -1;
    }
  size_t bits  = (size_t)bits_raw;
  size_t navg  = (size_t)navg_raw;
  self->handle = specan_create (fs, span, rbw, src_center, center, offset_db,
                                full_scale, bits, window, navg);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "specan_create returned NULL");
      return -1;
    }
  {
    size_t _max = specan_execute_max_out (self->handle);
    if (_max)
      {
        self->_execute_buf = malloc (_max * sizeof (float));
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
SpecanObj_execute_max_out (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (specan_execute_max_out (self->handle));
}

static PyObject *
SpecanObj_execute (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char   *_kwlist[] = { "x", "out", NULL };
  PyObject      *x_obj     = NULL;
  PyArrayObject *x_arr     = NULL;
  PyObject      *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O|O", _kwlist, &x_obj,
                                    &out_obj))
    return NULL;
  x_arr = (PyArrayObject *)PyArray_FROM_OTF (x_obj, NPY_COMPLEX64,
                                             NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    return NULL;
  if (out_obj && out_obj != Py_None)
    {
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_FLOAT, NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          Py_DECREF (x_arr);
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = specan_execute_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)PyArray_SIZE (x_arr)
                            ? _omax
                            : ((size_t)PyArray_SIZE (x_arr));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          Py_DECREF (x_arr);
          return NULL;
        }
      /* nogil: GIL released across the pure-C kernel — sound only when
       * this object is not shared across threads concurrently (one
       * object per stream); the kernel touches only this object's
       * state/buffers and the caller's input. */
      const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
      size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
      float               *_ng2 = (float *)PyArray_DATA (out_arr);
      size_t               n_out;
      Py_BEGIN_ALLOW_THREADS
        n_out = specan_execute (self->handle, _ng0, _ng1, _ng2, _cap);
      Py_END_ALLOW_THREADS
      Py_DECREF (x_arr);
      if (!n_out)
        {
          Py_DECREF (out_arr);
          Py_RETURN_NONE;
        }
      npy_intp  _odim  = (npy_intp)n_out;
      PyObject *_oview = PyArray_SimpleNewFromData (1, &_odim, NPY_FLOAT,
                                                    PyArray_DATA (out_arr));
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = (size_t)PyArray_SIZE (x_arr);
  if (!self->_execute_buf || self->_execute_buf_cap < _need)
    {
      size_t _max = specan_execute_max_out (self->handle);
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
              Py_DECREF (x_arr);
              PyErr_NoMemory ();
              return NULL;
            }
          self->_execute_retired     = _rt;
          self->_execute_retired_cap = _rcap;
        }
      float *_tmp = malloc (_max * sizeof (float));
      if (!_tmp)
        {
          Py_DECREF (x_arr);
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_execute_buf)
        self->_execute_retired[self->_execute_retired_n++]
            = self->_execute_buf;
      self->_execute_buf     = _tmp;
      self->_execute_buf_cap = _max;
    }
  /* nogil: GIL released across the pure-C kernel — sound only when
   * this object is not shared across threads concurrently (one
   * object per stream); the kernel touches only this object's
   * state/buffers and the caller's input. */
  const float complex *_ng0 = (const float complex *)PyArray_DATA (x_arr);
  size_t               _ng1 = (size_t)PyArray_SIZE (x_arr);
  size_t               n_out;
  Py_BEGIN_ALLOW_THREADS
    n_out = specan_execute (self->handle, _ng0, _ng1, self->_execute_buf,
                            self->_execute_buf_cap);
  Py_END_ALLOW_THREADS
  if (!n_out)
    Py_RETURN_NONE;
  npy_intp  dim = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_FLOAT, self->_execute_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  Py_DECREF (x_arr);
  return arr;
}

static PyObject *
SpecanObj_retune (SpecanObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "center", NULL };
  double       center    = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "d", _kwlist, &center))
    return NULL;
  specan_retune (self->handle, center);
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_reset (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  specan_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_state_bytes (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (specan_state_bytes (self->handle));
}

static PyObject *
SpecanObj_get_state (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = specan_state_bytes (self->handle);
  PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t)_n);
  if (!_b)
    return NULL;
  specan_get_state (self->handle, PyBytes_AS_STRING (_b));
  return _b;
}

static PyObject *
SpecanObj_set_state (SpecanObject *self, PyObject *arg)
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
  if ((size_t)PyBytes_GET_SIZE (arg) != specan_state_bytes (self->handle))
    {
      PyErr_SetString (PyExc_ValueError, "state blob size mismatch");
      return NULL;
    }
  if (specan_set_state (self->handle, PyBytes_AS_STRING (arg)) != 0)
    {
      PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
Specan_getprop_fs_out (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs_out);
}
static PyObject *
Specan_getprop_span (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->span);
}
static PyObject *
Specan_getprop_rbw (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->psd->enbw * self->handle->fs_out
                             / (double)self->handle->n);
}
static PyObject *
Specan_getprop_center (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->center);
}
static PyObject *
Specan_getprop_beta (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->beta);
}
static PyObject *
Specan_getprop_n (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->n);
}
static PyObject *
Specan_getprop_nfft (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->nfft);
}
static PyObject *
Specan_getprop_navg (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong ((unsigned long long)self->handle->navg);
}
static PyObject *
Specan_getprop_display_size (SpecanObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->disp_n);
}

static PyGetSetDef Specan_getset[]
    = { { "fs_out", (getter)Specan_getprop_fs_out, NULL, "Fs out.\n", NULL },
        { "span", (getter)Specan_getprop_span, NULL, "Span.\n", NULL },
        { "rbw", (getter)Specan_getprop_rbw, NULL, "Rbw.\n", NULL },
        { "center", (getter)Specan_getprop_center, NULL, "Center.\n", NULL },
        { "beta", (getter)Specan_getprop_beta, NULL, "Beta.\n", NULL },
        { "n", (getter)Specan_getprop_n, NULL, "N.\n", NULL },
        { "nfft", (getter)Specan_getprop_nfft, NULL, "Nfft.\n", NULL },
        { "navg", (getter)Specan_getprop_navg, NULL, "Navg.\n", NULL },
        { "display_size", (getter)Specan_getprop_display_size, NULL,
          "Display size.\n", NULL },
        { NULL } };

static PyObject *
SpecanObj_destroy (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      specan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
SpecanObj_enter (SpecanObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
SpecanObj_exit (SpecanObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      specan_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef SpecanObj_methods[] = {

  { "execute", (PyCFunction)SpecanObj_execute, METH_VARARGS | METH_KEYWORDS,
    "execute(x) -> ndarray\n"
    "\n"
    "Mix, decimate, average; return one DC-centred dB display frame, or "
    "None.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Specan\n"
    "    >>> obj = Specan(2.048e6, 200e3, 500.0, \"kaiser\", 0.0, 0.0, 0.0, "
    "1.0, 0, 1)\n"
    "    >>> y = obj.execute(np.zeros(4))\n"
    "    >>> y.dtype\n"
    "    dtype('float32')\n" },
  { "execute_max_out", (PyCFunction)SpecanObj_execute_max_out, METH_NOARGS,
    "execute_max_out() -> int\n\nMax output length execute() can produce for "
    "the current state.\nUse to size the ``out=`` buffer." },
  { "retune", (PyCFunction)(void *)SpecanObj_retune,
    METH_VARARGS | METH_KEYWORDS,
    "retune(center) -> None\n"
    "\n"
    "Move the display center frequency (seamless LO retune; no rebuild).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Specan\n"
    "    >>> obj = Specan(2.048e6, 200e3, 500.0, \"kaiser\", 0.0, 0.0, 0.0, "
    "1.0, 0, 1)\n"
    "    >>> obj.retune(0.0)\n" },
  { "reset", (PyCFunction)SpecanObj_reset, METH_NOARGS,
    "reset() -> None\n"
    "\n"
    "Drop pending samples and the running average; zero LO/filter history.\n"
    "\n"
    "    >>> from doppler import Specan\n"
    "    >>> obj = Specan(2.048e6, 200e3, 500.0, \"kaiser\", 0.0, 0.0, 0.0, "
    "1.0, 0, 1)\n"
    "    >>> obj.reset()\n" },
  { "state_bytes", (PyCFunction)SpecanObj_state_bytes, METH_NOARGS,
    "Serialized state size in bytes." },
  { "get_state", (PyCFunction)SpecanObj_get_state, METH_NOARGS,
    "Serialize the engine's mutable state to bytes." },
  { "set_state", (PyCFunction)SpecanObj_set_state, METH_O,
    "Restore mutable state from a get_state() blob." },
  { "destroy", (PyCFunction)SpecanObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)SpecanObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)SpecanObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject SpecanObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "analyzer.Specan",
  .tp_basicsize                           = sizeof (SpecanObject),
  .tp_dealloc                             = (destructor)SpecanObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Create a natural-parameter spectrum analyzer.\n",
  .tp_methods = SpecanObj_methods,
  .tp_getset  = Specan_getset,
  .tp_new     = SpecanObj_new,
  .tp_init    = (initproc)SpecanObj_init,
};
