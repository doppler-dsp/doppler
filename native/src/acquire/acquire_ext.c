/*
 * acquire_ext.c — Python extension module acquire
 *
 * Objects: CarrierAcquisition
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "acquire_ext_carrier_acq.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef acquire_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "acquire",
  .m_doc  = "Cold-start carrier acquisition: a coarse frequency/phase search "
            "(CarrierAcquisition) that seeds a downstream tracking loop.\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> import numpy as np\n"
            ">>> from doppler.acquire import CarrierAcquisition\n"
            ">>> ca = CarrierAcquisition(sample_rate_hz=8000.0, "
            "symbol_rate_hz=1000.0,\n"
            "...                          resolution_hz=5.0)\n"
            ">>> ca.ready()\n"
            "False\n",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_acquire (void)
{
  import_array ();
  if (PyType_Ready (&CarrierAcquisitionObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&acquire_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&CarrierAcquisitionObjType);
  if (PyModule_AddObject (m, "CarrierAcquisition",
                          (PyObject *)&CarrierAcquisitionObjType)
      < 0)
    {
      Py_DECREF (&CarrierAcquisitionObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
