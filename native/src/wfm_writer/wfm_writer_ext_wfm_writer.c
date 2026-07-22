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

#include "wfm_writer/wfm_writer_core.h"

typedef struct
{
  PyObject_HEAD wfm_writer_state_t *handle;
} WriterObject;

static void
WriterObj_dealloc (WriterObject *self)
{
  if (self->handle)
    wfm_writer_destroy (self->handle);
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
WriterObj_reset (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  (void)self;
  /* Hand-written. jm's object shape always emits a reset(), but a writer has
   * nothing coherent to reset: the samples are already on disk, and the
   * written count drives the BLUE data_size patch that close() applies, so
   * clearing it would corrupt the header. Refusing beats silently doing
   * nothing -- a caller reaching for reset() wants a fresh capture. */
  PyErr_SetString (PyExc_NotImplementedError,
                   "a Writer cannot be reset -- construct a new Writer for a "
                   "new capture");
  return NULL;
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
      wfm_writer_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

/* `close()` is how this type has always been finalised, and unlike destroy()
 * it reports failure. That matters: the BLUE data_size patch and the extended
 * header are both written during close, so a write error there means the file
 * on disk is not the file you asked for. Hand-written -- jm's object shape has
 * only the void destructor. */
static PyObject *
WriterObj_close (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc       = wfm_writer_close (self->handle);
      self->handle = NULL; /* freed by close() either way */
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
  /* Deliberately close(), not destroy(): a with-block must still raise when
   * the final flush fails. */
  return WriterObj_close (self, NULL);
}

static PyMethodDef WriterObj_methods[] = {
  { "close", (PyCFunction)WriterObj_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Finalise the capture: flush, patch the BLUE data_size from the actual\n"
    "sample count, append any extended header, and release the file.\n"
    "Raises OSError if that final write fails. Idempotent.\n" },

  { "reset", (PyCFunction)WriterObj_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

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
