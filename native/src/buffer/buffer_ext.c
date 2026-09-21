/*
 * buffer_ext.c — Python extension module buffer
 *
 * Objects: F32Buffer, F64Buffer, I16Buffer
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "clib_common.h"
#include <numpy/arrayobject.h>

#include "buffer_ext_f32_buffer.c"
#include "buffer_ext_f64_buffer.c"
#include "buffer_ext_i16_buffer.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef buffer_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "buffer",
  .m_doc  = "Sample buffering: lock-free ring buffers for handing IQ blocks "
            "between producer and consumer stages.\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> import numpy as np\n"
            ">>> from doppler.buffer import F32Buffer\n"
            ">>> b = F32Buffer(16)\n"
            ">>> b.write(np.ones(4, np.complex64))\n"
            "True\n"
            ">>> b.wait(4).shape\n"
            "(4,)\n",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_buffer (void)
{
  import_array ();
  if (PyType_Ready (&F32BufferObjType) < 0)
    return NULL;
  if (PyType_Ready (&F64BufferObjType) < 0)
    return NULL;
  if (PyType_Ready (&I16BufferObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&buffer_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&F32BufferObjType);
  if (PyModule_AddObject (m, "F32Buffer", (PyObject *)&F32BufferObjType) < 0)
    {
      Py_DECREF (&F32BufferObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&F64BufferObjType);
  if (PyModule_AddObject (m, "F64Buffer", (PyObject *)&F64BufferObjType) < 0)
    {
      Py_DECREF (&F64BufferObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&I16BufferObjType);
  if (PyModule_AddObject (m, "I16Buffer", (PyObject *)&I16BufferObjType) < 0)
    {
      Py_DECREF (&I16BufferObjType);
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
