/*
 * wfm_reader_ext_wfm_reader.c — Reader type for the wfm_reader module.
 *
 * Included by wfm_reader_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_reader_ext.c is compiled.
 */
/* ======================================================== */
/* ReaderObject — wraps wfm_reader_state_t *       */
/* ======================================================== */

#include "wfm_reader/wfm_reader_core.h"

typedef struct
{
  PyObject_HEAD wfm_reader_state_t *handle;
  float complex *_read_buf;     /* pre-allocated output for read */
  size_t         _read_buf_cap; /* allocated capacity for read */
  void         **_read_retired; /* gh-219 deferred free */
  size_t         _read_retired_n;
  size_t         _read_retired_cap;
  PyObject      *_read_view_ref; /* gh-437 last returned view */
} ReaderObject;

static void
ReaderObj_dealloc (ReaderObject *self)
{
  if (self->handle)
    wfm_reader_destroy (self->handle);
  free (self->_read_buf);
  for (size_t _i = 0; _i < self->_read_retired_n; _i++)
    free (self->_read_retired[_i]);
  free (self->_read_retired);
  Py_XDECREF (self->_read_view_ref);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
ReaderObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  ReaderObject *self = (ReaderObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
ReaderObj_init (ReaderObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]        = { "path", "sample_type", "endian", NULL };
  PyObject    *path            = NULL; /* fspath -> bytes */
  const char  *sample_type_str = "cf32";
  const char  *endian_str      = "le";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&|ss", kwlist,
                                    PyUnicode_FSConverter, &path,
                                    &sample_type_str, &endian_str))
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
  self->handle
      = wfm_reader_create (PyBytes_AS_STRING (path), sample_type, endian);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "cannot open capture: no such file, unrecognised "
                       "container, or an unsupported BLUE format mode (only "
                       "S and C are supported)");
      return -1;
    }
  {
    size_t _max = wfm_reader_read_max_out (self->handle);
    if (_max)
      {
        self->_read_buf = malloc (_max * sizeof (float complex));
        if (!self->_read_buf)
          {
            PyErr_NoMemory ();
            return -1;
          }
        self->_read_buf_cap = _max;
      }
  }
  return 0;
}

static PyObject *
ReaderObj_reset (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  wfm_reader_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
ReaderObj_read_max_out (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (wfm_reader_read_max_out (self->handle));
}

static PyObject *
ReaderObj_read (ReaderObject *self, PyObject *args, PyObject *kwds)
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
          out_obj, NPY_COMPLEX64,
          NPY_ARRAY_C_CONTIGUOUS | NPY_ARRAY_WRITEABLE);
      if (!out_arr)
        {
          return NULL;
        }
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = wfm_reader_read_max_out (self->handle);
      size_t _min_cap = _omax > (size_t)n ? _omax : ((size_t)n);
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = wfm_reader_read (self->handle, (size_t)n,
                                      (float complex *)PyArray_DATA (out_arr));
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
  size_t _need      = (size_t)n;
  int    _view_live = 0;
  if (self->_read_view_ref)
    {
#if PY_VERSION_HEX >= 0x030D0000
      PyObject *_lv = NULL;
      if (PyWeakref_GetRef (self->_read_view_ref, &_lv) == 1)
        {
          Py_DECREF (_lv);
          _view_live = 1;
        }
#else
      _view_live = PyWeakref_GetObject (self->_read_view_ref) != Py_None;
#endif
    }
  if (!self->_read_buf || self->_read_buf_cap < _need || _view_live)
    {
      size_t _max = wfm_reader_read_max_out (self->handle);
      if (!_max || _max < _need)
        _max = _need;
      if (self->_read_buf && self->_read_retired_n == self->_read_retired_cap)
        {
          size_t _rcap
              = self->_read_retired_cap ? self->_read_retired_cap * 2 : 4;
          void **_rt = realloc (self->_read_retired, _rcap * sizeof (void *));
          if (!_rt)
            {
              PyErr_NoMemory ();
              return NULL;
            }
          self->_read_retired     = _rt;
          self->_read_retired_cap = _rcap;
        }
      float complex *_tmp = malloc (_max * sizeof (float complex));
      if (!_tmp)
        {
          PyErr_NoMemory ();
          return NULL;
        }
      if (self->_read_buf)
        self->_read_retired[self->_read_retired_n++] = self->_read_buf;
      self->_read_buf     = _tmp;
      self->_read_buf_cap = _max;
    }
  size_t    n_out = wfm_reader_read (self->handle, (size_t)n, self->_read_buf);
  npy_intp  dim   = (npy_intp)n_out;
  PyObject *arr
      = PyArray_SimpleNewFromData (1, &dim, NPY_COMPLEX64, self->_read_buf);
  if (!arr)
    return NULL;
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)self);
  Py_INCREF (self);
  /* gh-437: remember this view — while the caller holds it the next
   * call retires the buffer instead of reusing it in place. */
  Py_XDECREF (self->_read_view_ref);
  self->_read_view_ref = PyWeakref_NewRef (arr, NULL);
  if (!self->_read_view_ref)
    {
      Py_DECREF (arr);
      return NULL;
    }
  return arr;
}
/* gh-519: strcmp for the enum lookup below. Python.h already
 * pulls in <string.h>, but the include is explicit so the block
 * stands on its own wherever it is spliced. */
#include <string.h>

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index_Reader (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_Reader_ftype[] = {
  "raw", "csv", "blue", "sigmf", NULL,
};

static const char *const _enum_Reader_stype[] = {
  "cf32", "cf64", "ci32", "ci16", "ci8", NULL,
};

static const char *const _enum_Reader_sample_mode[] = {
  "complex",
  "scalar",
  NULL,
};

static const char *const _enum_Reader_endian[] = {
  "le",
  "be",
  NULL,
};

static PyObject *
Reader_getprop_file_type (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long _v = (long)(self->handle->file_type);
  if (_v < 0 || _v >= 4)
    {
      PyErr_Format (PyExc_ValueError,
                    "file_type holds out-of-range ftype value %ld"
                    " (valid: 0..3)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_ftype[_v]);
}
static PyObject *
Reader_getprop_sample_type (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long _v = (long)(self->handle->sample_type);
  if (_v < 0 || _v >= 5)
    {
      PyErr_Format (PyExc_ValueError,
                    "sample_type holds out-of-range stype value %ld"
                    " (valid: 0..4)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_stype[_v]);
}
static PyObject *
Reader_getprop_mode (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long _v = (long)(self->handle->mode);
  if (_v < 0 || _v >= 2)
    {
      PyErr_Format (PyExc_ValueError,
                    "mode holds out-of-range sample_mode value %ld"
                    " (valid: 0..1)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_sample_mode[_v]);
}
static PyObject *
Reader_getprop_endian (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  long _v = (long)(self->handle->endian);
  if (_v < 0 || _v >= 2)
    {
      PyErr_Format (PyExc_ValueError,
                    "endian holds out-of-range endian value %ld"
                    " (valid: 0..1)",
                    _v);
      return NULL;
    }
  return PyUnicode_FromString (_enum_Reader_endian[_v]);
}
static PyObject *
Reader_getprop_fs (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fs);
}
static PyObject *
Reader_getprop_fc (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyFloat_FromDouble (self->handle->fc);
}
static PyObject *
Reader_getprop_num_samples (ReaderObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->num_samples);
}

static PyGetSetDef Reader_getset[]
    = { { "file_type", (getter)Reader_getprop_file_type, NULL, "File type.\n",
          NULL },
        { "sample_type", (getter)Reader_getprop_sample_type, NULL,
          "Sample type.\n", NULL },
        { "mode", (getter)Reader_getprop_mode, NULL, "Mode.\n", NULL },
        { "endian", (getter)Reader_getprop_endian, NULL, "Endian.\n", NULL },
        { "fs", (getter)Reader_getprop_fs, NULL, "Fs.\n", NULL },
        { "fc", (getter)Reader_getprop_fc, NULL, "Fc.\n", NULL },
        { "num_samples", (getter)Reader_getprop_num_samples, NULL,
          "Num samples.\n", NULL },
        { NULL } };

static PyObject *
ReaderObj_destroy (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      wfm_reader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
ReaderObj_enter (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
ReaderObj_exit (ReaderObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      wfm_reader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* `close()` is the name this type has always released its file under, and it
 * is what every caller and doc example uses. jm's object shape names the
 * destructor `destroy()`; both are kept, idempotent and interchangeable, so
 * the handle-era API survives the migration unchanged. (Hand-written: this
 * fragment is sacred, jm only creates it when missing.) */
static PyObject *
ReaderObj_close (ReaderObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      wfm_reader_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef ReaderObj_methods[] = {
  { "close", (PyCFunction)ReaderObj_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Close the capture and release the file. Idempotent; also called by\n"
    "__exit__ and at deallocation. An alias for destroy().\n" },

  { "reset", (PyCFunction)ReaderObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "read", (PyCFunction)ReaderObj_read, METH_VARARGS | METH_KEYWORDS,
    "read(n=1) -> ndarray\n"
    "\n"
    "Read up to max complex samples into out (unit-scale `float _Complex`), "
    "converting from the wire type. Returns the count read; 0 at end of "
    "file.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Reader\n"
    "    >>> obj = Reader(..., \"cf32\", \"le\")\n"
    "    >>> y = obj.read(4)\n"
    "    >>> y.dtype\n"
    "    dtype('complex64')\n" },
  { "read_max_out", (PyCFunction)ReaderObj_read_max_out, METH_NOARGS,
    "read_max_out() -> int\n\nMax output length read() can produce for the "
    "current state.\nUse to size the ``out=`` buffer." },
  { "destroy", (PyCFunction)ReaderObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)ReaderObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)ReaderObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject ReaderObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm_reader.Reader",
  .tp_basicsize                           = sizeof (ReaderObject),
  .tp_dealloc                             = (destructor)ReaderObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Open a capture, auto-detecting its container.\n",
  .tp_methods = ReaderObj_methods,
  .tp_getset  = Reader_getset,
  .tp_new     = ReaderObj_new,
  .tp_init    = (initproc)ReaderObj_init,
};
