/*
 * capture_ext_rawcapture.c — RawCapture type for the capture module.
 *
 * Included by capture_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only capture_ext.c is compiled.
 */
/* ======================================================== */
/* RawCaptureObject — wraps capture_state_t *       */
/* ======================================================== */

#include "capture/capture_core.h"

typedef struct
{
  PyObject_HEAD capture_state_t *handle;
} RawCaptureObject;

static void
RawCaptureObj_dealloc (RawCaptureObject *self)
{
  if (self->handle)
    capture_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
RawCaptureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  RawCaptureObject *self = (RawCaptureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
RawCaptureObj_init (RawCaptureObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "path", "sample_type", "endian", "fs", "fc", NULL };
  PyObject   *path            = NULL; /* fspath -> bytes */
  const char *sample_type_str = "ci16";
  const char *endian_str      = "le";
  double      fs              = 1.0;
  double      fc              = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&|ssdd", kwlist,
                                    PyUnicode_FSConverter, &path,
                                    &sample_type_str, &endian_str, &fs, &fc))
    {
      Py_XDECREF (path);
      return -1;
    }
  int sample_type = 0;
  if (strcmp (sample_type_str, "cf32") == 0)
    sample_type = 0;
  else if (strcmp (sample_type_str, "cf64") == 0)
    sample_type = 1;
  else if (strcmp (sample_type_str, "ci32") == 0)
    sample_type = 2;
  else if (strcmp (sample_type_str, "ci16") == 0)
    sample_type = 3;
  else if (strcmp (sample_type_str, "ci8") == 0)
    sample_type = 4;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "sample_type must be one of \"cf32\", \"cf64\", \"ci32\", "
                    "\"ci16\", \"ci8\", got '%s'",
                    sample_type_str);
      Py_XDECREF (path);
      return -1;
    }
  int endian = 0;
  if (strcmp (endian_str, "le") == 0)
    endian = 0;
  else if (strcmp (endian_str, "be") == 0)
    endian = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "endian must be one of \"le\", \"be\", got '%s'",
                    endian_str);
      Py_XDECREF (path);
      return -1;
    }
  self->handle = capture_open_raw (PyBytes_AS_STRING (path), sample_type,
                                   endian, fs, fc);
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
RawCaptureObj_reset (RawCaptureObject *self, PyObject *Py_UNUSED (ignored))
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
RawCaptureObj_read_max_out (RawCaptureObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  Py_ssize_t n = 0;
  if (!PyArg_ParseTuple (args, "n", &n))
    return NULL;
  return PyLong_FromSize_t (capture_read_max_out (self->handle, (size_t)n));
}

static PyObject *
RawCaptureObj_read (RawCaptureObject *self, PyObject *args, PyObject *kwds)
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
      size_t _omax    = capture_read_max_out (self->handle, (size_t)n);
      size_t _min_cap = _omax;
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
  size_t _cap  = capture_read_max_out (self->handle, (size_t)n);
  (void)_need;
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

static PyStructSequence_Field RawCaptureObj_summary_fields[] = {
  { "num_samples", "Samples the reader decoded from the capture." },
  { "fs_hz", "Sample rate (Hz); 0 if the file never stated it." },
  { "fc_hz", "Centre frequency (Hz); 0 if the file was silent." },
  { NULL, NULL },
};
static PyStructSequence_Desc RawCaptureObj_summary_desc
    = { "iqtools.capture.CaptureSummary",
        "A capture at a glance: sample count and the resolved fs/fc.",
        RawCaptureObj_summary_fields, 3 };
static PyTypeObject *RawCaptureObj_summary_type = NULL;

static PyObject *
RawCaptureObj_summary (RawCaptureObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!RawCaptureObj_summary_type)
    {
      RawCaptureObj_summary_type
          = PyStructSequence_NewType (&RawCaptureObj_summary_desc);
      if (!RawCaptureObj_summary_type)
        return NULL;
    }
  capture_summary_t _r = capture_summary (self->handle);
  PyObject         *_o = PyStructSequence_New (RawCaptureObj_summary_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (
      _o, 0, PyLong_FromUnsignedLongLong ((unsigned long long)_r.num_samples));
  PyStructSequence_SET_ITEM (_o, 1, PyFloat_FromDouble (_r.fs_hz));
  PyStructSequence_SET_ITEM (_o, 2, PyFloat_FromDouble (_r.fc_hz));
  return _o;
}
/* gh-519: strcmp for the enum lookup below. Python.h already
 * pulls in <string.h>, but the include is explicit so the block
 * stands on its own wherever it is spliced. */
#include <string.h>

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index_RawCapture (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_RawCapture_metadata_source[] = {
  "none",
  "file",
  "supplied",
  NULL,
};

static PyObject *
RawCapture_getprop_fs (RawCaptureObject *self, void *Py_UNUSED (closure))
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
RawCapture_getprop_fc (RawCaptureObject *self, void *Py_UNUSED (closure))
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
RawCapture_getprop_num_samples (RawCaptureObject *self,
                                void             *Py_UNUSED (closure))
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
RawCapture_getprop_metadata_source (RawCaptureObject *self,
                                    void             *Py_UNUSED (closure))
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
  return PyUnicode_FromString (_enum_RawCapture_metadata_source[_v]);
}

static PyGetSetDef RawCapture_getset[] = {
  { "fs", (getter)RawCapture_getprop_fs, NULL,
    "Sample rate in Hz. Read from the file for BLUE/SigMF; supplied by you "
    "for a `RawCapture`.\n",
    NULL },
  { "fc", (getter)RawCapture_getprop_fc, NULL,
    "Centre frequency in Hz. Read from the file for BLUE/SigMF; supplied by "
    "you for a `RawCapture`.\n",
    NULL },
  { "num_samples", (getter)RawCapture_getprop_num_samples, NULL,
    "Total samples in the capture.\n", NULL },
  { "metadata_source", (getter)RawCapture_getprop_metadata_source, NULL,
    "Where `fs`/`fc` came from -- `\"file\"` when the capture declared them "
    "(BLUE, SigMF), `\"supplied\"` when you passed them to `RawCapture`, or "
    "`\"none\"` when neither. This is the property the view exists to make "
    "honest: with a plain `Capture` over a headerless file the numbers are "
    "defaults, and without this you cannot tell a default from a reading.\n",
    NULL },
  { NULL }
};

static PyObject *
RawCaptureObj_destroy (RawCaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
RawCaptureObj_enter (RawCaptureObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
RawCaptureObj_exit (RawCaptureObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      capture_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef RawCaptureObj_methods[] = {
  { "reset", (PyCFunction)RawCaptureObj_reset, METH_NOARGS,
    "Reset state to post-create defaults.\n" },

  { "read", (PyCFunction)(void *)RawCaptureObj_read,
    METH_VARARGS | METH_KEYWORDS,
    "read(count=1) -> ndarray\n"
    "\n"
    "Read up to `count` samples as unit-scale complex64; an empty array at "
    "end of file.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.complex64]\n"
    "    Output.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from iqtools import RawCapture\n"
    "    >>> obj = RawCapture(path=..., sample_type=\"ci16\", endian=\"le\", "
    "fs=1.0, fc=0.0)\n"
    "    >>> y = obj.read(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "read_max_out", (PyCFunction)RawCaptureObj_read_max_out, METH_VARARGS,
    "read_max_out(n) -> int\n"
    "\n"
    "Largest number of samples read() can return for n inputs.\n"
    "\n"
    "Size an `out=` buffer with this before calling read(), or use it to\n"
    "allocate one up front. The bound is this object's own: what it depends\n"
    "on is a property of the algorithm, so a header block on read_max_out()\n"
    "replaces this text.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Number of input samples read() will be given.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Upper bound on the output length; the actual call may return "
    "fewer.\n" },
  { "summary", (PyCFunction)RawCaptureObj_summary, METH_VARARGS,
    "summary() -> CaptureSummary record (num_samples, fs_hz, fc_hz)." },
  { "destroy", (PyCFunction)RawCaptureObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)RawCaptureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Capture be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Capture\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)RawCaptureObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Capture.\n"
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

static PyTypeObject RawCaptureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "capture.RawCapture",
  .tp_basicsize                           = sizeof (RawCaptureObject),
  .tp_dealloc                             = (destructor)RawCaptureObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "RawCapture type.\n",
  .tp_methods                             = RawCaptureObj_methods,
  .tp_getset                              = RawCapture_getset,
  .tp_new                                 = RawCaptureObj_new,
  .tp_init                                = (initproc)RawCaptureObj_init,
};
