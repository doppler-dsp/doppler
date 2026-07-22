/*
 * wfm_writer_ext.c — Python extension module wfm_writer
 *
 * Objects: Writer
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "wfm_writer_ext_wfm_writer.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef wfm_writer_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "wfm_writer",
    .m_doc     = "WfmWriter module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_wfm_writer(void)
{
    import_array();
    if (PyType_Ready(&WriterObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&wfm_writer_moduledef);
    if (!m) return NULL;
    Py_INCREF(&WriterObjType);
    if (PyModule_AddObject(m, "Writer", (PyObject *)&WriterObjType) < 0) {
        Py_DECREF(&WriterObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
