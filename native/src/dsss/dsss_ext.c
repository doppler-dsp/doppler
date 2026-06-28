/*
 * dsss_ext.c — Python extension module dsss
 *
 * Objects: Despreader, Acquisition, PolyPhaseEstimator, BurstDemod
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "dsss_ext_despreader.c"
#include "dsss_ext_acq.c"
#include "dsss_ext_ppe.c"
#include "dsss_ext_burst_demod.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef dsss_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "dsss",
    .m_doc     = "Dsss module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_dsss(void)
{
    import_array();
    if (PyType_Ready(&DespreaderObjType) < 0) return NULL;
    if (PyType_Ready(&AcquisitionObjType) < 0) return NULL;
    if (PyType_Ready(&PolyPhaseEstimatorObjType) < 0) return NULL;
    if (PyType_Ready(&BurstDemodObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&dsss_moduledef);
    if (!m) return NULL;
    Py_INCREF(&DespreaderObjType);
    if (PyModule_AddObject(m, "Despreader", (PyObject *)&DespreaderObjType) < 0) {
        Py_DECREF(&DespreaderObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&AcquisitionObjType);
    if (PyModule_AddObject(m, "Acquisition", (PyObject *)&AcquisitionObjType) < 0) {
        Py_DECREF(&AcquisitionObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&PolyPhaseEstimatorObjType);
    if (PyModule_AddObject(m, "PolyPhaseEstimator", (PyObject *)&PolyPhaseEstimatorObjType) < 0) {
        Py_DECREF(&PolyPhaseEstimatorObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&BurstDemodObjType);
    if (PyModule_AddObject(m, "BurstDemod", (PyObject *)&BurstDemodObjType) < 0) {
        Py_DECREF(&BurstDemodObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
