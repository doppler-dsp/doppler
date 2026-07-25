/*
 * ddc_ext.c — Python extension module ddc
 *
 * Objects: DDC, Ddcr, MatchedDDC, MatchedDdcr
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "ddc_ext_ddc.c"
#include "ddc_ext_ddcr.c"
#include "ddc_ext_matchedddc.c"
#include "ddc_ext_matchedddcr.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef ddc_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "ddc",
    .m_doc     = "Ddc module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_ddc(void)
{
    import_array();
    if (PyType_Ready(&DDCObjType) < 0) return NULL;
    if (PyType_Ready(&DdcrObjType) < 0) return NULL;
    if (PyType_Ready(&MatchedDDCObjType) < 0) return NULL;
    if (PyType_Ready(&MatchedDdcrObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&ddc_moduledef);
    if (!m) return NULL;
    Py_INCREF(&DDCObjType);
    if (PyModule_AddObject(m, "DDC", (PyObject *)&DDCObjType) < 0) {
        Py_DECREF(&DDCObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&DdcrObjType);
    if (PyModule_AddObject(m, "Ddcr", (PyObject *)&DdcrObjType) < 0) {
        Py_DECREF(&DdcrObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&MatchedDDCObjType);
    if (PyModule_AddObject(m, "MatchedDDC", (PyObject *)&MatchedDDCObjType) < 0) {
        Py_DECREF(&MatchedDDCObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&MatchedDdcrObjType);
    if (PyModule_AddObject(m, "MatchedDdcr", (PyObject *)&MatchedDdcrObjType) < 0) {
        Py_DECREF(&MatchedDdcrObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
