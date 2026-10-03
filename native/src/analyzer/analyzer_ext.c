/*
 * analyzer_ext.c — Python extension module analyzer
 *
 * Objects: Specan
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

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

#include "analyzer_ext_specan.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef analyzer_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "analyzer",
  .m_doc
  = "Spectrum analysis: Specan renders a windowed, averaged power spectral "
    "density (dBFS) over a chosen span and resolution bandwidth, mirroring a "
    "hardware spectrum analyser's span / RBW / averaging controls.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.analyzer import Specan\n"
    ">>> sp = Specan(fs=1.024e6, span=200e3, rbw=2e3, src_center=0.0,\n"
    "...             center=0.0, offset_db=0.0, full_scale=1.0, bits=0,\n"
    "...             window=\"kaiser\", navg=1)\n"
    ">>> sp.execute(np.ones(4096, np.complex64)).shape\n"
    "(201,)\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_analyzer (void)
{
  import_array ();
  if (PyType_Ready (&SpecanObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&analyzer_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&SpecanObjType);
  if (PyModule_AddObject (m, "Specan", (PyObject *)&SpecanObjType) < 0)
    {
      Py_DECREF (&SpecanObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
