/*
 * capture_ext.c — Python extension module capture
 *
 * Objects: Capture, RawCapture
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "capture_ext_capture.c"
#include "capture_ext_rawcapture.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef capture_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "capture",
    .m_doc     = "Capture module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_capture(void)
{
    import_array();
    if (PyType_Ready(&CaptureObjType) < 0) return NULL;
    if (!CaptureObj_summary_type) {
        CaptureObj_summary_type = PyStructSequence_NewType(&CaptureObj_summary_desc);
        if (!CaptureObj_summary_type) return NULL;
    }
    if (PyType_Ready(&RawCaptureObjType) < 0) return NULL;
    if (!RawCaptureObj_summary_type) {
        RawCaptureObj_summary_type = PyStructSequence_NewType(&RawCaptureObj_summary_desc);
        if (!RawCaptureObj_summary_type) return NULL;
    }
    PyObject *m = PyModule_Create(&capture_moduledef);
    if (!m) return NULL;
    Py_INCREF(&CaptureObjType);
    if (PyModule_AddObject(m, "Capture", (PyObject *)&CaptureObjType) < 0) {
        Py_DECREF(&CaptureObjType); Py_DECREF(m); return NULL;
    }
    if (PyModule_AddObject(m, "CaptureSummary", (PyObject *)CaptureObj_summary_type) < 0) {
        Py_DECREF(CaptureObj_summary_type);
        Py_DECREF(m);
        return NULL;
    }
    Py_INCREF(&RawCaptureObjType);
    if (PyModule_AddObject(m, "RawCapture", (PyObject *)&RawCaptureObjType) < 0) {
        Py_DECREF(&RawCaptureObjType); Py_DECREF(m); return NULL;
    }
    if (PyModule_AddObject(m, "CaptureSummary", (PyObject *)RawCaptureObj_summary_type) < 0) {
        Py_DECREF(RawCaptureObj_summary_type);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
