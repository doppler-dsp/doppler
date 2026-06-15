/*
 * analyzer_ext.c — Python extension module analyzer
 *
 * Objects: Specan
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "analyzer_ext_specan.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef analyzer_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "analyzer",
    .m_doc     = "Analyzer module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_analyzer(void)
{
    import_array();
    if (PyType_Ready(&SpecanObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&analyzer_moduledef);
    if (!m) return NULL;
    Py_INCREF(&SpecanObjType);
    if (PyModule_AddObject(m, "Specan", (PyObject *)&SpecanObjType) < 0) {
        Py_DECREF(&SpecanObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
