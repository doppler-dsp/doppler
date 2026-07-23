/*
 * wfm_writer_ext_wfm_writer.c — Writer type for the wfm_writer module.
 *
 * Included by wfm_writer_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_writer_ext.c is compiled.
 */
/* ======================================================== */
/* WriterObject — wraps wfm_writer_state_t *       */
/* ======================================================== */

#include "wfm/wfm_keywords.h" /* wfm_kw_elem_size for add_keyword marshaling */
#include "wfm_writer/wfm_writer_core.h"

typedef struct
{
  PyObject_HEAD wfm_writer_state_t *handle;
} WriterObject;

static void
WriterObj_dealloc (WriterObject *self)
{
  if (self->handle)
    {
      /* gh-541: tp_dealloc has no exception context — there
         is no caller to raise to, and an in-flight exception
         must not be clobbered. Discarding the status is the
         only correct choice here; the explicit teardown and
         __exit__ paths do report it. */
      (void)wfm_writer_destroy (self->handle);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
WriterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  WriterObject *self = (WriterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
WriterObj_init (WriterObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "path", "file_type", "sample_type", "endian", "fs",
                            "fc",   "total",     "headroom",    NULL };
  PyObject    *path     = NULL; /* fspath -> bytes */
  const char  *file_type_str   = "raw";
  const char  *sample_type_str = "cf32";
  const char  *endian_str      = "le";
  double       fs              = 1e6;
  double       fc              = 0.0;
  unsigned long long total_raw = 0;
  double             headroom  = 0.0;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O&|sssddKd", kwlist, PyUnicode_FSConverter, &path,
          &file_type_str, &sample_type_str, &endian_str, &fs, &fc, &total_raw,
          &headroom))
    {
      Py_XDECREF (path);
      return -1;
    }
  int file_type = 0;
  if (strcmp (file_type_str, "raw") == 0)
    file_type = 0;
  else if (strcmp (file_type_str, "csv") == 0)
    file_type = 1;
  else if (strcmp (file_type_str, "blue") == 0)
    file_type = 2;
  else if (strcmp (file_type_str, "sigmf") == 0)
    file_type = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "file_type must be one of \"raw\", \"csv\", \"blue\", "
                    "\"sigmf\", got '%s'",
                    file_type_str);
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
  size_t total = (size_t)total_raw;
  self->handle
      = wfm_writer_create (PyBytes_AS_STRING (path), file_type, sample_type,
                           endian, fs, fc, total, headroom);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_OSError,
                       "cannot open the capture for writing (check the path, "
                       "the directory, and permissions)");
      return -1;
    }
  return 0;
}

static PyObject *
WriterObj_write (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float complex *x     = (const float complex *)PyArray_DATA (x_arr);
  size_t               x_len = (size_t)PyArray_SIZE (x_arr);
  size_t               y     = wfm_writer_write (self->handle, x, x_len);
  Py_DECREF (x_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
WriterObj_track_clipping (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "on", NULL };
  int          on        = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", _kwlist, &on))
    return NULL;
  wfm_writer_track_clipping (self->handle, on);
  Py_RETURN_NONE;
}

/* add_keyword(tag, type, value) — attach a BLUE extended-header keyword.
 *
 * Hand-written (gh-426 sacred-fragment method + a `# jm:hand` .pyi stub): the
 * C API takes an untyped `const void *value` whose element type is decided at
 * runtime by the `type` char, so the Python -> C marshaling is data-dependent
 * and jm cannot generate it -- the exact input-side mirror of the read side's
 * `wfm_reader_keyword_value` (value_type="object"), filed as gh-554. `value`
 * is a str (type "A"), a single int/float, or a sequence of them; it is packed
 * into a host-order buffer and handed to wfm_writer_add_keyword, which
 * byte-swaps per element on write. */
static PyObject *
WriterObj_add_keyword (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "tag", "type", "value", NULL };
  const char  *tag = NULL, *type_s = NULL;
  PyObject    *value = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ssO", _kwlist, &tag, &type_s,
                                    &value))
    return NULL;
  if (strlen (type_s) != 1)
    {
      PyErr_SetString (PyExc_ValueError,
                       "type must be a single character code "
                       "(B/I/L/X, F/D, A, or T)");
      return NULL;
    }
  char   type = type_s[0];
  size_t esz  = wfm_kw_elem_size (type);
  if (esz == 0)
    {
      PyErr_Format (
          PyExc_ValueError,
          "unsupported keyword type '%c' (use B/I/L/X, F/D, A, or T)", type);
      return NULL;
    }

  int rc;
  if (type == 'A')
    {
      /* A string: the wire form is raw bytes with an explicit length, no NUL.
       */
      if (!PyUnicode_Check (value))
        {
          PyErr_SetString (PyExc_TypeError, "type 'A' takes a str value");
          return NULL;
        }
      Py_ssize_t  n = 0;
      const char *s = PyUnicode_AsUTF8AndSize (value, &n);
      if (!s)
        return NULL;
      rc = wfm_writer_add_keyword (self->handle, tag, type, s, (size_t)n);
    }
  else
    {
      /* Numeric: a single scalar, or a sequence of them. A str is rejected
         above by type; here a bare number has no length, a list does. */
      int       is_float = (type == 'F' || type == 'D');
      PyObject *seq      = NULL;
      size_t    count    = 1;
      if (PySequence_Check (value) && !PyUnicode_Check (value))
        {
          seq = PySequence_Fast (
              value, "value must be a number or a sequence of numbers");
          if (!seq)
            return NULL;
          count = (size_t)PySequence_Fast_GET_SIZE (seq);
        }
      if (count == 0)
        {
          PyErr_SetString (PyExc_ValueError, "value sequence is empty");
          Py_XDECREF (seq);
          return NULL;
        }
      uint8_t *buf = malloc (count * esz);
      if (!buf)
        {
          PyErr_NoMemory ();
          Py_XDECREF (seq);
          return NULL;
        }
      for (size_t i = 0; i < count; i++)
        {
          PyObject *item
              = seq ? PySequence_Fast_GET_ITEM (seq, i) : value; /* borrowed */
          uint8_t *p = buf + i * esz;
          if (is_float)
            {
              double d = PyFloat_AsDouble (item);
              if (d == -1.0 && PyErr_Occurred ())
                goto item_err;
              if (type == 'F')
                {
                  float f = (float)d;
                  memcpy (p, &f, sizeof f);
                }
              else
                memcpy (p, &d, sizeof d);
            }
          else
            {
              long long v = PyLong_AsLongLong (item);
              if (v == -1 && PyErr_Occurred ())
                goto item_err;
              switch (type)
                {
                case 'B':
                  {
                    int8_t x = (int8_t)v;
                    memcpy (p, &x, sizeof x);
                    break;
                  }
                case 'I':
                  {
                    int16_t x = (int16_t)v;
                    memcpy (p, &x, sizeof x);
                    break;
                  }
                case 'L':
                case 'T':
                  {
                    int32_t x = (int32_t)v;
                    memcpy (p, &x, sizeof x);
                    break;
                  }
                default: /* 'X' */
                  {
                    int64_t x = (int64_t)v;
                    memcpy (p, &x, sizeof x);
                    break;
                  }
                }
            }
          continue;
        item_err:
          free (buf);
          Py_XDECREF (seq);
          return NULL;
        }
      rc = wfm_writer_add_keyword (self->handle, tag, type, buf, count);
      free (buf);
      Py_XDECREF (seq);
    }

  if (rc != 0)
    {
      PyErr_SetString (PyExc_ValueError,
                       "add_keyword failed: the writer is not a BLUE capture, "
                       "the tag is empty or too long, or the buffer could not "
                       "grow");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Writer_getprop_clip_fraction (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_writer_get_clip_fraction (self->handle));
}
static PyObject *
Writer_getprop_peak_dbfs (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_writer_get_peak_dbfs (self->handle));
}
static PyObject *
Writer_getprop_clipped (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(wfm_writer_get_clipped (self->handle)));
}

static PyGetSetDef Writer_getset[]
    = { { "clip_fraction", (getter)Writer_getprop_clip_fraction, NULL,
          "Clip fraction.\n", NULL },
        { "peak_dbfs", (getter)Writer_getprop_peak_dbfs, NULL, "Peak dbfs.\n",
          NULL },
        { "clipped", (getter)Writer_getprop_clipped, NULL, "Clipped.\n",
          NULL },
        { NULL } };

static PyObject *
WriterObj_destroy (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc = wfm_writer_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_OSError,
                           "failed to finalise the capture: the trailing "
                           "header patch or extended header was not written");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
WriterObj_enter (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
WriterObj_exit (WriterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      int rc = wfm_writer_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_OSError,
                           "failed to finalise the capture: the trailing "
                           "header patch or extended header was not written");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyMethodDef WriterObj_methods[] = {

  { "write", (PyCFunction)(void *)WriterObj_write,
    METH_VARARGS | METH_KEYWORDS,
    "write(x) -> int\n"
    "\n"
    "Convert and write `n` complex samples.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Writer\n"
    "    >>> obj = Writer(..., \"raw\", \"cf32\", \"le\", 1e6, 0.0, 0, 0.0)\n"
    "    >>> obj.write(np.zeros(4, dtype=np.complex64))\n"
    "    0\n" },
  { "track_clipping", (PyCFunction)(void *)WriterObj_track_clipping,
    METH_VARARGS | METH_KEYWORDS,
    "track_clipping(on) -> None\n"
    "\n"
    "Enable the per-component clip *counter* (off by default; peak is always "
    "on).\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Writer\n"
    "    >>> obj = Writer(..., \"raw\", \"cf32\", \"le\", 1e6, 0.0, 0, 0.0)\n"
    "    >>> obj.track_clipping(0)\n" },
  { "add_keyword", (PyCFunction)(void *)WriterObj_add_keyword,
    METH_VARARGS | METH_KEYWORDS,
    "add_keyword(tag, type, value) -> None\n"
    "\n"
    "Attach a BLUE extended-header keyword (BLUE only). `type` is a single\n"
    "character: B/I/L/X (8/16/32/64-bit int), F/D (32/64-bit float), A\n"
    "(string), or the deprecated T (32-bit int). `value` is a str for A, a\n"
    "single int/float, or a sequence of them. Keywords are buffered and\n"
    "written at close(), after the samples. The read side is "
    "Reader.keywords.\n" },
  { "close", (PyCFunction)WriterObj_destroy, METH_NOARGS,
    "Release resources." },
  { "destroy", (PyCFunction)WriterObj_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)WriterObj_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)WriterObj_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject WriterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm_writer.Writer",
  .tp_basicsize                           = sizeof (WriterObject),
  .tp_dealloc                             = (destructor)WriterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Path-opening + FILE-owning ctor for the generated `Writer` "
                "handle (jm kind=\"handle\"): opens `path` (\"wb\"), delegates to "
                "wfm_writer_open, and marks the FILE owned so wfm_writer_close "
                "fclose's it. Returns NULL on open failure.\n",
  .tp_methods = WriterObj_methods,
  .tp_getset  = Writer_getset,
  .tp_new     = WriterObj_new,
  .tp_init    = (initproc)WriterObj_init,
};
