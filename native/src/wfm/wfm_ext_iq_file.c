/*
 * wfm_ext_iq_file.c — IqFile type for the wfm module.
 *
 * Included by wfm_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_ext.c is compiled.
 */
/* ======================================================== */
/* IqFileObject — wraps iq_file_state_t *       */
/* ======================================================== */

#include "iq_file/iq_file_core.h"

typedef struct
{
  PyObject_HEAD iq_file_state_t *handle;
} IqFileObject;

static void
IqFile_dealloc (IqFileObject *self)
{
  if (self->handle)
    iq_file_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
IqFile_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  IqFileObject *self = (IqFileObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
IqFile_init (IqFileObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]        = { "filepath", "sample_type", "endian", NULL };
  const char  *filepath        = NULL;
  const char  *sample_type_str = "cf32";
  const char  *endian_str      = "le";

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|ss", kwlist, &filepath,
                                    &sample_type_str, &endian_str))
    return -1;
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
      return -1;
    }
  self->handle = iq_file_create (filepath, sample_type, endian);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_MemoryError, "iq_file_create returned NULL");
      return -1;
    }
  return 0;
}

static PyObject *
IqFile_reset (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  iq_file_reset (self->handle);
  Py_RETURN_NONE;
}

static PyObject *
IqFile_get_fd (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)iq_file_get_fd (self->handle));
}

static PyObject *
IqFile_set_fd (IqFileObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  iq_file_set_fd (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
IqFile_get_position (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)iq_file_get_position (self->handle));
}

static PyObject *
IqFile_set_position (IqFileObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned long long v_raw = 0ULL;
  if (!PyArg_ParseTuple (args, "K", &v_raw))
    return NULL;
  size_t v = (size_t)v_raw;
  iq_file_set_position (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
IqFile_get_nsamples (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)iq_file_get_nsamples (self->handle));
}

static PyObject *
IqFile_set_nsamples (IqFileObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  unsigned long long v_raw = 0ULL;
  if (!PyArg_ParseTuple (args, "K", &v_raw))
    return NULL;
  size_t v = (size_t)v_raw;
  iq_file_set_nsamples (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
IqFile_get_sample_type (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)iq_file_get_sample_type (self->handle));
}

static PyObject *
IqFile_set_sample_type (IqFileObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  iq_file_set_sample_type (self->handle, v);
  Py_RETURN_NONE;
}

static PyObject *
IqFile_get_endian (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromLong ((long)iq_file_get_endian (self->handle));
}

static PyObject *
IqFile_set_endian (IqFileObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int v = 0;
  if (!PyArg_ParseTuple (args, "i", &v))
    return NULL;
  iq_file_set_endian (self->handle, v);
  Py_RETURN_NONE;
}
static PyObject *
IqFile_read (IqFileObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  size_t    n       = (size_t)n_raw;
  npy_intp  _dims[] = { (npy_intp)n };
  PyObject *_out    = PyArray_EMPTY (1, _dims, NPY_COMPLEX64, 0);
  if (!_out)
    {
      return NULL;
    }
  iq_file_read (self->handle, n,
                (float complex *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

static PyObject *
IqFile_close (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  float complex y = iq_file_close (self->handle);
  return PyComplex_FromDoubles ((double)crealf (y), (double)cimagf (y));
}
static PyObject *
IqFile_getprop_nsamples (IqFileObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)self->handle->nsamples);
}

static PyGetSetDef IqFile_getset[]
    = { { "nsamples", (getter)IqFile_getprop_nsamples, NULL, "Nsamples.\n",
          NULL },
        { NULL } };

static PyObject *
IqFile_destroy (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      iq_file_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
IqFile_enter (IqFileObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
IqFile_exit (IqFileObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      iq_file_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef IqFile_methods[] = {
  { "reset", (PyCFunction)IqFile_reset, METH_NOARGS,
    "Reset state to post-create defaults." },

  { "get_fd", (PyCFunction)IqFile_get_fd, METH_NOARGS, "Get fd." },
  { "set_fd", (PyCFunction)IqFile_set_fd, METH_VARARGS, "Set fd." },
  { "get_position", (PyCFunction)IqFile_get_position, METH_NOARGS,
    "Get position." },
  { "set_position", (PyCFunction)IqFile_set_position, METH_VARARGS,
    "Set position." },
  { "get_nsamples", (PyCFunction)IqFile_get_nsamples, METH_NOARGS,
    "Get nsamples." },
  { "set_nsamples", (PyCFunction)IqFile_set_nsamples, METH_VARARGS,
    "Set nsamples." },
  { "get_sample_type", (PyCFunction)IqFile_get_sample_type, METH_NOARGS,
    "Get sample_type." },
  { "set_sample_type", (PyCFunction)IqFile_set_sample_type, METH_VARARGS,
    "Set sample_type." },
  { "get_endian", (PyCFunction)IqFile_get_endian, METH_NOARGS, "Get endian." },
  { "set_endian", (PyCFunction)IqFile_set_endian, METH_VARARGS,
    "Set endian." },
  { "read", (PyCFunction)(void *)IqFile_read, METH_VARARGS | METH_KEYWORDS,
    "read(n) -> ndarray\n"
    "\n"
    "read.\n"
    "\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import IqFile\n"
    "    >>> obj = IqFile(\"\")\n"
    "    >>> y = obj.read(0)\n"
    "    >>> y.ndim\n"
    "    1\n" },
  { "close", (PyCFunction)IqFile_close, METH_NOARGS,
    "close() -> complex\n"
    "\n"
    "close.\n"
    "\n"
    "    >>> from doppler import IqFile\n"
    "    >>> obj = IqFile(\"\")\n"
    "    >>> obj.close()\n"
    "    0j\n" },
  { "destroy", (PyCFunction)IqFile_destroy, METH_NOARGS,
    "Release resources." },
  { "__enter__", (PyCFunction)IqFile_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)IqFile_exit, METH_VARARGS, NULL },
  { NULL }
};

static PyTypeObject IqFileType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm.IqFile",
  .tp_basicsize                           = sizeof (IqFileObject),
  .tp_dealloc                             = (destructor)IqFile_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "IqFile type.\n",
  .tp_methods                             = IqFile_methods,
  .tp_getset                              = IqFile_getset,
  .tp_new                                 = IqFile_new,
  .tp_init                                = (initproc)IqFile_init,
};
