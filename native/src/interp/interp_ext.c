/*
 * interp_ext.c — Python extension module interp
 *
 * Objects: InterpolatedTable
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

#include "interp_ext_interp_table.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef interp_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "interp",
  .m_doc
  = "Interpolation: a table-driven resampling interpolator "
    "(InterpolatedTable) for fractional-delay and rate-change lookups.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.interp import InterpolatedTable\n"
    ">>> t = InterpolatedTable(np.arange(4, dtype=np.complex128))\n"
    ">>> t.execute(np.array([1.5], np.float64)).real.round(2).tolist()\n"
    "[1.5]\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_interp (void)
{
  import_array ();
  if (PyType_Ready (&InterpolatedTableObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&interp_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&InterpolatedTableObjType);
  if (PyModule_AddObject (m, "InterpolatedTable",
                          (PyObject *)&InterpolatedTableObjType)
      < 0)
    {
      Py_DECREF (&InterpolatedTableObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
