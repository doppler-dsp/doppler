/*
 * interp_ext.c — Python extension module interp
 *
 * Objects: InterpolatedTable
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

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
