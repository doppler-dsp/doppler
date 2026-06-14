/*
 * measure_ext.c — Python extension module measure
 *
 * Objects: ToneMeasure
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "measure_ext_tonemeas.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef measure_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "measure",
    .m_doc     = "Measure module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_measure(void)
{
    import_array();
    if (PyType_Ready(&ToneMeasureObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&measure_moduledef);
    if (!m) return NULL;
    Py_INCREF(&ToneMeasureObjType);
    if (PyModule_AddObject(m, "ToneMeasure", (PyObject *)&ToneMeasureObjType) < 0) {
        Py_DECREF(&ToneMeasureObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
