/*
 * impairment_ext.c — Python extension module impairment
 *
 * Objects: DopplerChannel
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
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, less the two inputs
 * it reads as text (gh-1700): a str is refused, never parsed as a number,
 * and for a one-byte element type a byte buffer (bytes, bytearray,
 * memoryview) is its bytes, one element per byte. `name` is the parameter,
 * for the message, and `hint` (NULL for none) is appended to a str's
 * refusal. Returns a new reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  if (PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj)))
    {
      /* `hint` (gh-1756) says where text goes instead: a str only. */
      int say = hint && PyUnicode_Check (obj);
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
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

#include "impairment_ext_doppler_channel.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef impairment_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "impairment",
  .m_doc
  = "Channel impairments: a DopplerChannel applying carrier offset, delay, "
    "and additive white Gaussian noise to a signal for test and simulation.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.impairment import DopplerChannel\n"
    ">>> ch = DopplerChannel(fs=1e6, carrier_hz=1e5, doppler_ppm=10.0)\n"
    ">>> bool(ch.execute(np.ones(64, np.complex64)).size > 0)\n"
    "True\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_impairment (void)
{
  import_array ();
  if (PyType_Ready (&DopplerChannelObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&impairment_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&DopplerChannelObjType);
  if (PyModule_AddObject (m, "DopplerChannel",
                          (PyObject *)&DopplerChannelObjType)
      < 0)
    {
      Py_DECREF (&DopplerChannelObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
