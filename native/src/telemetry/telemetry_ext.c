/*
 * telemetry_ext.c — Python extension module telemetry
 *
 * Objects: Telemetry, MemoryCapture, Capture
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "telemetry_ext_capture.c"
#include "telemetry_ext_dp_tlm.c"
#include "telemetry_ext_dp_tlm_capture.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef telemetry_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "telemetry",
  .m_doc
  = "Telemetry: lightweight in-band probes (Telemetry) that record loop "
    "internals -- error, control, lock -- to a sink for offline analysis.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> t = Telemetry()\n"
    ">>> t.probe('loop.err')\n"
    "0\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_telemetry (void)
{
  import_array ();
  if (PyType_Ready (&TelemetryObjType) < 0)
    return NULL;
  if (PyType_Ready (&MemoryCaptureObjType) < 0)
    return NULL;
  if (PyType_Ready (&CaptureObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&telemetry_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&TelemetryObjType);
  if (PyModule_AddObject (m, "Telemetry", (PyObject *)&TelemetryObjType) < 0)
    {
      Py_DECREF (&TelemetryObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&MemoryCaptureObjType);
  if (PyModule_AddObject (m, "MemoryCapture",
                          (PyObject *)&MemoryCaptureObjType)
      < 0)
    {
      Py_DECREF (&MemoryCaptureObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CaptureObjType);
  if (PyModule_AddObject (m, "Capture", (PyObject *)&CaptureObjType) < 0)
    {
      Py_DECREF (&CaptureObjType);
      Py_DECREF (m);
      return NULL;
    } /* gh-1117: adopt dp_interrupt_guard's process-global state from its
         owner. */
  {
    void     *dp_interrupt_guard_state_ptr (void);
    void      dp_interrupt_guard_state_adopt (void *shared);
    PyObject *_own = PyImport_ImportModule ("doppler.interrupt.interrupt");
    if (!_own)
      {
        Py_DECREF (m);
        return NULL;
      }
    PyObject *_pg = PyObject_GetAttrString (_own, "_jm_pg_dp_interrupt_guard");
    Py_DECREF (_own);
    if (!_pg)
      {
        Py_DECREF (m);
        return NULL;
      }
    void *_p = PyCapsule_GetPointer (
        _pg, "doppler.dp_interrupt_guard._jm_procglobal");
    Py_DECREF (_pg);
    if (!_p)
      {
        Py_DECREF (m);
        return NULL;
      }
    dp_interrupt_guard_state_adopt (_p);
  }

  return m;
}
