/*
 * accumulator_ext.c — Python extension module accumulator
 *
 * Objects: AccF32, AccCf64, AccTrace
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "accumulator_ext_acc_cf64.c"
#include "accumulator_ext_acc_f32.c"
#include "accumulator_ext_acc_trace.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef accumulator_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "accumulator",
  .m_doc = "Running accumulators: single- and double-precision complex scalar "
           "sums (AccF32, AccCf64) and a per-tap trace accumulator "
           "(AccTrace), each carrying a running total across calls.\n"
           "\n"
           "Examples\n"
           "--------\n"
           ">>> from doppler.accumulator import AccF32\n"
           ">>> a = AccF32()\n"
           ">>> a.step(1.5); a.step(2.5)\n"
           ">>> a.get()\n"
           "4.0\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_accumulator (void)
{
  import_array ();
  if (PyType_Ready (&AccF32Type) < 0)
    return NULL;
  if (PyType_Ready (&AccCf64Type) < 0)
    return NULL;
  if (PyType_Ready (&AccTraceObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&accumulator_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&AccF32Type);
  if (PyModule_AddObject (m, "AccF32", (PyObject *)&AccF32Type) < 0)
    {
      Py_DECREF (&AccF32Type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&AccCf64Type);
  if (PyModule_AddObject (m, "AccCf64", (PyObject *)&AccCf64Type) < 0)
    {
      Py_DECREF (&AccCf64Type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&AccTraceObjType);
  if (PyModule_AddObject (m, "AccTrace", (PyObject *)&AccTraceObjType) < 0)
    {
      Py_DECREF (&AccTraceObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
