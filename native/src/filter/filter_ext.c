/*
 * filter_ext.c — Python extension module filter
 *
 * Objects: FIR, HBDecimQ15
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "filter_ext_fir.c"
#include "filter_ext_hbdecim_q15.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef filter_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "filter",
    .m_doc     = "Filter module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_filter(void)
{
    import_array();
    if (PyType_Ready(&FIRObjType) < 0) return NULL;
    if (PyType_Ready(&HBDecimQ15ObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&filter_moduledef);
    if (!m) return NULL;
    Py_INCREF(&FIRObjType);
    if (PyModule_AddObject(m, "FIR", (PyObject *)&FIRObjType) < 0) {
        Py_DECREF(&FIRObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&HBDecimQ15ObjType);
    if (PyModule_AddObject(m, "HBDecimQ15", (PyObject *)&HBDecimQ15ObjType) < 0) {
        Py_DECREF(&HBDecimQ15ObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
