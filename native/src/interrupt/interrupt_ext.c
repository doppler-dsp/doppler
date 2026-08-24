/*
 * interrupt_ext.c — Python extension module interrupt
 *
 * Objects: Interrupt
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "interrupt_ext_dp_interrupt_guard.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef interrupt_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "interrupt",
  .m_doc     = "Stopping a run: the one flag every blocking wait in doppler "
               "consults, and the scoped guard that arms it.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> from doppler.interrupt import Interrupt\n"
               ">>> it = Interrupt([])\n"
               ">>> it.interrupted()\n"
               "0\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_interrupt (void)
{
  import_array ();
  if (PyType_Ready (&InterruptObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&interrupt_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&InterruptObjType);
  if (PyModule_AddObject (m, "Interrupt", (PyObject *)&InterruptObjType) < 0)
    {
      Py_DECREF (&InterruptObjType);
      Py_DECREF (m);
      return NULL;
    } /* gh-1117: this module OWNS dp_interrupt_guard's process-global state.
       */
  {
    void     *dp_interrupt_guard_state_ptr (void);
    void      dp_interrupt_guard_state_adopt (void *shared);
    PyObject *_pg
        = PyCapsule_New (dp_interrupt_guard_state_ptr (),
                         "doppler.dp_interrupt_guard._jm_procglobal", NULL);
    if (!_pg)
      {
        Py_DECREF (m);
        return NULL;
      }
    if (PyModule_AddObject (m, "_jm_pg_dp_interrupt_guard", _pg) < 0)
      {
        Py_DECREF (_pg);
        Py_DECREF (m);
        return NULL;
      }
  }

  return m;
}
