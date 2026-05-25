/*
 * accumulator_ext.c — Python extension module accumulator
 *
 * Objects: AccF32, AccCf64
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "accumulator_ext_acc_f32.c"
#include "accumulator_ext_acc_cf64.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef accumulator_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "accumulator",
    .m_doc     = "Accumulator module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_accumulator(void)
{
    import_array();
    if (PyType_Ready(&AccF32Type) < 0) return NULL;
    if (PyType_Ready(&AccCf64Type) < 0) return NULL;
    PyObject *m = PyModule_Create(&accumulator_moduledef);
    if (!m) return NULL;
    Py_INCREF(&AccF32Type);
    if (PyModule_AddObject(m, "AccF32", (PyObject *)&AccF32Type) < 0) {
        Py_DECREF(&AccF32Type); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&AccCf64Type);
    if (PyModule_AddObject(m, "AccCf64", (PyObject *)&AccCf64Type) < 0) {
        Py_DECREF(&AccCf64Type); Py_DECREF(m); return NULL;
    }
    return m;
}
