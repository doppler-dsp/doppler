/*
 * cvt_ext.c — Python extension module cvt
 *
 * Objects: F32ToI16, I16ToF32, F32ToI16U32, F32ToI16U64, I16U32ToF32, I16U64ToF32
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "cvt_ext_f32_to_i16.c"
#include "cvt_ext_i16_to_f32.c"
#include "cvt_ext_f32_to_i16u32.c"
#include "cvt_ext_f32_to_i16u64.c"
#include "cvt_ext_i16u32_to_f32.c"
#include "cvt_ext_i16u64_to_f32.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef cvt_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "cvt",
    .m_doc     = "Cvt module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_cvt(void)
{
    import_array();
    if (PyType_Ready(&F32ToI16ObjType) < 0) return NULL;
    if (PyType_Ready(&I16ToF32ObjType) < 0) return NULL;
    if (PyType_Ready(&F32ToI16U32ObjType) < 0) return NULL;
    if (PyType_Ready(&F32ToI16U64ObjType) < 0) return NULL;
    if (PyType_Ready(&I16U32ToF32ObjType) < 0) return NULL;
    if (PyType_Ready(&I16U64ToF32ObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&cvt_moduledef);
    if (!m) return NULL;
    Py_INCREF(&F32ToI16ObjType);
    if (PyModule_AddObject(m, "F32ToI16", (PyObject *)&F32ToI16ObjType) < 0) {
        Py_DECREF(&F32ToI16ObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&I16ToF32ObjType);
    if (PyModule_AddObject(m, "I16ToF32", (PyObject *)&I16ToF32ObjType) < 0) {
        Py_DECREF(&I16ToF32ObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&F32ToI16U32ObjType);
    if (PyModule_AddObject(m, "F32ToI16U32", (PyObject *)&F32ToI16U32ObjType) < 0) {
        Py_DECREF(&F32ToI16U32ObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&F32ToI16U64ObjType);
    if (PyModule_AddObject(m, "F32ToI16U64", (PyObject *)&F32ToI16U64ObjType) < 0) {
        Py_DECREF(&F32ToI16U64ObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&I16U32ToF32ObjType);
    if (PyModule_AddObject(m, "I16U32ToF32", (PyObject *)&I16U32ToF32ObjType) < 0) {
        Py_DECREF(&I16U32ToF32ObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&I16U64ToF32ObjType);
    if (PyModule_AddObject(m, "I16U64ToF32", (PyObject *)&I16U64ToF32ObjType) < 0) {
        Py_DECREF(&I16U64ToF32ObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
