/*
 * track_ext.c — Python extension module track
 *
 * Objects: LoopFilter, Costas, Dll, Channel, SymbolSync, CarrierMpsk, CarrierNda
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include <complex.h>


#include "track_ext_loop_filter.c"
#include "track_ext_costas.c"
#include "track_ext_dll.c"
#include "track_ext_channel.c"
#include "track_ext_symsync.c"
#include "track_ext_carrier_mpsk.c"
#include "track_ext_carrier_nda.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef track_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "track",
    .m_doc     = "Track module.",
    .m_size    = -1,
    .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_track(void)
{
    import_array();
    if (PyType_Ready(&LoopFilterObjType) < 0) return NULL;
    if (PyType_Ready(&CostasObjType) < 0) return NULL;
    if (PyType_Ready(&DllObjType) < 0) return NULL;
    if (PyType_Ready(&ChannelObjType) < 0) return NULL;
    if (PyType_Ready(&SymbolSyncObjType) < 0) return NULL;
    if (PyType_Ready(&CarrierMpskObjType) < 0) return NULL;
    if (PyType_Ready(&CarrierNdaObjType) < 0) return NULL;
    PyObject *m = PyModule_Create(&track_moduledef);
    if (!m) return NULL;
    Py_INCREF(&LoopFilterObjType);
    if (PyModule_AddObject(m, "LoopFilter", (PyObject *)&LoopFilterObjType) < 0) {
        Py_DECREF(&LoopFilterObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CostasObjType);
    if (PyModule_AddObject(m, "Costas", (PyObject *)&CostasObjType) < 0) {
        Py_DECREF(&CostasObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&DllObjType);
    if (PyModule_AddObject(m, "Dll", (PyObject *)&DllObjType) < 0) {
        Py_DECREF(&DllObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&ChannelObjType);
    if (PyModule_AddObject(m, "Channel", (PyObject *)&ChannelObjType) < 0) {
        Py_DECREF(&ChannelObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&SymbolSyncObjType);
    if (PyModule_AddObject(m, "SymbolSync", (PyObject *)&SymbolSyncObjType) < 0) {
        Py_DECREF(&SymbolSyncObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CarrierMpskObjType);
    if (PyModule_AddObject(m, "CarrierMpsk", (PyObject *)&CarrierMpskObjType) < 0) {
        Py_DECREF(&CarrierMpskObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CarrierNdaObjType);
    if (PyModule_AddObject(m, "CarrierNda", (PyObject *)&CarrierNdaObjType) < 0) {
        Py_DECREF(&CarrierNdaObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
