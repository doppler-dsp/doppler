/*
 * wfm_reader_ext.c — Python extension module wfm_reader
 *
 * Objects: Reader
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "wfm_reader_ext_wfm_reader.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef wfm_reader_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "wfm_reader",
    .m_doc     = "WfmReader module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_wfm_reader(void)
{
    import_array();
    if (PyType_Ready(&ReaderObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&wfm_reader_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ReaderObjType);
    if (PyModule_AddObject(m, "Reader", (PyObject *)&ReaderObjType) < 0) {
        Py_DECREF(&ReaderObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
