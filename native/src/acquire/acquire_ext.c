/*
 * acquire_ext.c — Python extension module acquire
 *
 * Objects: CarrierAcquisition
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "acquire_ext_carrier_acq.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef acquire_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "acquire",
    .m_doc     = "Acquire module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_acquire(void)
{
    import_array();
    if (PyType_Ready(&CarrierAcquisitionObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&acquire_moduledef);
    if (!m) return NULL;
    Py_INCREF(&CarrierAcquisitionObjType);
    if (PyModule_AddObject(m, "CarrierAcquisition", (PyObject *)&CarrierAcquisitionObjType) < 0) {
        Py_DECREF(&CarrierAcquisitionObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
