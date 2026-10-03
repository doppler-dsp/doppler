/*
 * wfm_writer_ext.c — Python extension module wfm_writer
 *
 * Objects: Writer
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>
#include <string.h>

#include "doppler/wfm_writer/wfm_writer_core.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, except that for a
 * one-byte element type a byte buffer (bytes, bytearray, memoryview) is its
 * bytes, one element per byte (gh-1700). `name` is the parameter, for the
 * message. `hint` is its declared str_hint, or NULL. Declaring one is the
 * opt-in to refusing text (gh-1824): a str, or a bytes numpy would parse as
 * a number, and a str's refusal ends with the hint (gh-1756). With NULL,
 * numpy converts a str as it converts anything else. Returns a new
 * reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  int text     = PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj));
  if (hint && text)
    {
      /* The hint says where text goes instead: a str's refusal only. */
      int say = PyUnicode_Check (obj);
      PyErr_Format (PyExc_TypeError,
                    "%s must be an array of numbers, not %.200s%s%s", name,
                    Py_TYPE (obj)->tp_name, say ? ": " : "", say ? hint : "");
      return NULL;
    }
  if (one_byte && !PyArray_Check (obj) && PyObject_CheckBuffer (obj))
    {
      PyObject *view = PyMemoryView_FromObject (obj);
      if (!view)
        return NULL;
      if (PyMemoryView_GET_BUFFER (view)->itemsize == 1)
        {
          PyObject *raw = PyArray_FromBuffer (
              view, PyArray_DescrFromType (typenum), -1, 0);
          Py_DECREF (view);
          if (!raw)
            return NULL;
          PyObject *arr = PyArray_FROM_OTF (raw, typenum, requirements);
          Py_DECREF (raw);
          return (PyArrayObject *)arr;
        }
      Py_DECREF (view);
    }
  return (PyArrayObject *)PyArray_FROM_OTF (obj, typenum, requirements);
}
/* `unused` (gh-1747): emitted into every extension translation unit,
 * including one that takes no array, or calls only jm_array_arg_hint above
 * -- which needs no mark, since this wrapper always calls it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

#include "wfm_writer_ext_wfm_writer.c"

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_stype[] = {
  "cf32", "cf64", "ci32", "ci16", "ci8", "f32",
  "f64",  "i32",  "i16",  "i8",   NULL,
};

static const char *const _enum_endian[] = {
  "le",
  "be",
  NULL,
};

static PyObject *
_bind_write_blue_header (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char *_kwlist[]
      = { "path",       "fs",    "sample_type", "endian", "fc",
          "data_start", "total", "detached",    "t0",     NULL };
  PyObject          *path        = NULL; /* fspath -> bytes */
  double             fs          = 0.0;
  const char        *sample_type = "cf32";
  const char        *endian      = "le";
  double             fc          = 0.0;
  double             data_start  = 0.0;
  unsigned long long total_raw   = 0;
  int                detached    = 1;
  double             t0          = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&d|ssddKid", _kwlist,
                                    PyUnicode_FSConverter, &path, &fs,
                                    &sample_type, &endian, &fc, &data_start,
                                    &total_raw, &detached, &t0))
    {
      Py_XDECREF (path);
      return NULL;
    }
  int _arg_sample_type = _enum_index (_enum_stype, sample_type);
  if (_arg_sample_type < 0)
    {
      PyErr_Format (PyExc_ValueError,
                    "invalid sample_type '%s' (choices: cf32, cf64, ci32, "
                    "ci16, ci8, f32, f64, i32, i16, i8)",
                    sample_type);
      Py_XDECREF (path);
      return NULL;
    }
  int _arg_endian = _enum_index (_enum_endian, endian);
  if (_arg_endian < 0)
    {
      PyErr_Format (PyExc_ValueError, "invalid endian '%s' (choices: le, be)",
                    endian);
      Py_XDECREF (path);
      return NULL;
    }
  size_t total = (size_t)total_raw;
  int    _rc   = dp_write_blue_header (PyBytes_AS_STRING (path), fs,
                                       _arg_sample_type, _arg_endian, fc,
                                       data_start, total, detached, t0);
  Py_XDECREF (path);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_RuntimeError, "dp_write_blue_header failed (rc=%d)",
                    (int)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef wfm_writer_module_methods[] = {
  { "write_blue_header", (PyCFunction)(void *)_bind_write_blue_header,
    METH_VARARGS | METH_KEYWORDS,
    "Write a standalone BLUE type-1000 HCB header (the detached .hdr): 512 "
    "bytes carrying the BLUE magic, byte order, data_size (total x "
    "bytes-per-sample), the type-1000 tag and xdelta = 1/fs. Pair it with a "
    "detached .det body of raw interleaved I/Q. Raises on a failed write.\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef wfm_writer_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "wfm_writer",
  .m_doc     = "WfmWriter module.",
  .m_size    = -1,
  .m_methods = wfm_writer_module_methods,
};

PyMODINIT_FUNC
PyInit_wfm_writer (void)
{
  import_array ();
  if (PyType_Ready (&WriterObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&wfm_writer_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&WriterObjType);
  if (PyModule_AddObject (m, "Writer", (PyObject *)&WriterObjType) < 0)
    {
      Py_DECREF (&WriterObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
