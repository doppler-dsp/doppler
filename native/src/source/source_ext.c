/*
 * source_ext.c — Python extension module source
 *
 * Objects: NCO, LO, AWGN
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "source_ext_nco.c"
#include "source_ext_lo.c"
#include "source_ext_awgn.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef source_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "source",
    .m_doc     = "Source module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_source(void)
{
    import_array();
    if (PyType_Ready(&NCOObjType) < 0) return NULL;
    if (PyType_Ready(&LOObjType) < 0) return NULL;
    if (PyType_Ready(&AWGNObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&source_moduledef);
    if (!m) return NULL;
    Py_INCREF(&NCOObjType);
    if (PyModule_AddObject(m, "NCO", (PyObject *)&NCOObjType) < 0) {
        Py_DECREF(&NCOObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&LOObjType);
    if (PyModule_AddObject(m, "LO", (PyObject *)&LOObjType) < 0) {
        Py_DECREF(&LOObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&AWGNObjType);
    if (PyModule_AddObject(m, "AWGN", (PyObject *)&AWGNObjType) < 0) {
        Py_DECREF(&AWGNObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
