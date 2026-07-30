/*
 * capture_ext_capture.c — Capture type for the capture module.
 *
 * Included by capture_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only capture_ext.c is compiled.
 */
/* ======================================================== */
/* CaptureObject — wraps capture_state_t *       */
/* ======================================================== */

#include "capture/capture_core.h"

typedef struct
{
  PyObject_HEAD capture_state_t *handle;
} CaptureObject;

static void
CaptureObj_dealloc (CaptureObject *self)
{
  if (self->handle)
    capture_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
CaptureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  CaptureObject *self = (CaptureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
CaptureObj_init (CaptureObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "path", NULL };
  PyObject    *path     = NULL; /* fspath -> bytes */

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&", kwlist,
                                    PyUnicode_FSConverter, &path))
    {
      Py_XDECREF (path);
      return -1;
    }
  self->handle = capture_create (PyBytes_AS_STRING (path));
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "cannot open capture: no such file, or an "
                       "unrecognised file type");
      return -1;
    }
  return 0;
}

static PyObject *
CaptureObj_reset (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  capture_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
CaptureObj_read_max_out (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (capture_read_max_out (self->handle));
}

static PyObject *
CaptureObj_read (CaptureObject *self, PyObject *args, PyObject *kwds)
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
          return NULL;
        }
      PyArrayObject *out_arr = (PyArrayObject *)PyArray_FROM_OTF (
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = capture_read_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out
          = capture_read (self->handle, (size_t)n,
                          (float complex *)PyArray_DATA (out_arr), _cap);
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
  size_t _cap  = capture_read_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp  _adim = (npy_intp)_cap;
  PyObject *arr0  = PyArray_SimpleNew (1, &_adim, NPY_COMPLEX64);
  if (!arr0)
    {
      return NULL;
    }
  float complex *_d0   = (float complex *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t         n_out = capture_read (self->handle, (size_t)n, _d0, _cap);
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
/* gh-519: strcmp for the enum lookup below. Python.h already
 * pulls in <string.h>, but the include is explicit so the block
 * stands on its own wherever it is spliced. */
#include <string.h>

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index_Capture (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_Capture_metadata_source[] = {
  "none",
  "file",
  "supplied",
  NULL,
};

static PyObject *
Capture_getprop_fs (CaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (capture_get_fs (self->handle));
}
static PyObject *
Capture_getprop_fc (CaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (capture_get_fc (self->handle));
}
static PyObject *
Capture_getprop_num_samples (CaptureObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)capture_get_num_samples (self->handle));
}
static PyObject *
Capture_getprop_metadata_source (CaptureObject *self,
                                 void          *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  long _v = (long)(capture_get_metadata_source (self->handle));
  if (_v < 0 || _v >= 3)
    {
      PyErr_Format (
          PyExc_ValueError,
          "metadata_source holds out-of-range metadata_source value %ld"
          " (valid: 0..2)",
          _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Capture_metadata_source[_v]);
}

static PyGetSetDef Capture_getset[] = {
  { "fs", (getter)Capture_getprop_fs, NULL,
    "Sample rate in Hz. Read from the file for BLUE/SigMF; supplied by you "
    "for a `RawCapture`.\n",
    NULL },
  { "fc", (getter)Capture_getprop_fc, NULL,
    "Centre frequency in Hz. Read from the file for BLUE/SigMF; supplied by "
    "you for a `RawCapture`.\n",
    NULL },
  { "num_samples", (getter)Capture_getprop_num_samples, NULL,
    "Total samples in the capture.\n", NULL },
  { "metadata_source", (getter)Capture_getprop_metadata_source, NULL,
    "Where `fs`/`fc` came from -- `\"file\"` when the capture declared them "
    "(BLUE, SigMF), `\"supplied\"` when you passed them to `RawCapture`, or "
    "`\"none\"` when neither. This is the property the view exists to make "
    "honest: with a plain `Capture` over a headerless file the numbers are "
    "defaults, and without this you cannot tell a default from a reading.\n",
    NULL },
  { NULL }
};

static PyObject *
CaptureObj_destroy (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
CaptureObj_enter (CaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
CaptureObj_exit (CaptureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef CaptureObj_methods[]
    = { { "reset", (PyCFunction)CaptureObj_reset, METH_NOARGS,
          "Reset state to post-create defaults." },

        { "read", (PyCFunction)(void *)CaptureObj_read,
          METH_VARARGS | METH_KEYWORDS,
          "read(n=1) -> ndarray\n"
          "\n"
          "Read up to `count` samples as unit-scale complex64; an empty array "
          "at end of file.\n"
          "\n"
          "    >>> import numpy as np\n"
          "    >>> from iqtools import Capture\n"
          "    >>> obj = Capture(...)\n"
          "    >>> y = obj.read(4)\n"
          "    >>> y.dtype\n"
          "    dtype('complex64')\n" },
        { "read_max_out", (PyCFunction)CaptureObj_read_max_out, METH_NOARGS,
          "read_max_out() -> int\n\nMax output length read() can produce for "
          "the current state.\nUse to size the ``out=`` buffer." },
        { "destroy", (PyCFunction)CaptureObj_destroy, METH_NOARGS,
          "Release resources." },
        { "__enter__", (PyCFunction)CaptureObj_enter, METH_NOARGS, NULL },
        { "__exit__", (PyCFunction)CaptureObj_exit, METH_VARARGS, NULL },
        { NULL } };

static PyTypeObject CaptureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "capture.Capture",
  .tp_basicsize                           = sizeof (CaptureObject),
  .tp_dealloc                             = (destructor)CaptureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Capture type.\n",
  .tp_methods                             = CaptureObj_methods,
  .tp_getset                              = Capture_getset,
  .tp_new                                 = CaptureObj_new,
  .tp_init                                = (initproc)CaptureObj_init,
};
