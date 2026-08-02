/*
 * track_ext.c — Python extension module track
 *
 * Objects: LoopFilter, Costas, Dll, SymbolSync, RateSync, CarrierMpsk, CarrierNda, MpskReceiver, MpskReceiverR
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
#include "track_ext_symsync.c"
#include "track_ext_ratesync.c"
#include "track_ext_carrier_mpsk.c"
#include "track_ext_carrier_nda.c"
#include "track_ext_mpsk_receiver.c"
#include "track_ext_mpsk_receiver_r.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef track_moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "track",
    .m_doc     = "Carrier and timing tracking: loop filters, Costas / non-data-aided / MPSK carrier recovery, DLL code tracking, symbol-timing and rate sync, and full MPSK receivers.\n"
     "\n"
     "Examples\n"
     "--------\n"
     ">>> import numpy as np\n"
     ">>> from doppler.track import LoopFilter\n"
     ">>> y = LoopFilter(bn=0.05, zeta=0.707).steps(np.ones(50))\n"
     ">>> bool(y[-1] > y[0])\n"
     "True\n",
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
    if (PyType_Ready(&SymbolSyncObjType) < 0) return NULL;
    if (PyType_Ready(&RateSyncObjType) < 0) return NULL;
    if (PyType_Ready(&CarrierMpskObjType) < 0) return NULL;
    if (PyType_Ready(&CarrierNdaObjType) < 0) return NULL;
    if (PyType_Ready(&MpskReceiverObjType) < 0) return NULL;
    if (PyType_Ready(&MpskReceiverRObjType) < 0) return NULL;
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
    Py_INCREF(&SymbolSyncObjType);
    if (PyModule_AddObject(m, "SymbolSync", (PyObject *)&SymbolSyncObjType) < 0) {
        Py_DECREF(&SymbolSyncObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&RateSyncObjType);
    if (PyModule_AddObject(m, "RateSync", (PyObject *)&RateSyncObjType) < 0) {
        Py_DECREF(&RateSyncObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CarrierMpskObjType);
    if (PyModule_AddObject(m, "CarrierMpsk", (PyObject *)&CarrierMpskObjType) < 0) {
        Py_DECREF(&CarrierMpskObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&CarrierNdaObjType);
    if (PyModule_AddObject(m, "CarrierNda", (PyObject *)&CarrierNdaObjType) < 0) {
        Py_DECREF(&CarrierNdaObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&MpskReceiverObjType);
    if (PyModule_AddObject(m, "MpskReceiver", (PyObject *)&MpskReceiverObjType) < 0) {
        Py_DECREF(&MpskReceiverObjType); Py_DECREF(m); return NULL;
    }
    Py_INCREF(&MpskReceiverRObjType);
    if (PyModule_AddObject(m, "MpskReceiverR", (PyObject *)&MpskReceiverRObjType) < 0) {
        Py_DECREF(&MpskReceiverRObjType); Py_DECREF(m); return NULL;
    }
    return m;
}
