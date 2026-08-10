/*
 * delay_ext.c — Python extension module delay
 *
 * Objects: DelayCf64
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "delay_ext_delay.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef delay_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "delay",
  .m_doc
  = "Fractional and integer sample delay: a delay line (DelayCf64) with a "
    "windowed tap view for building matched filters and correlators.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.delay import DelayCf64\n"
    ">>> d = DelayCf64(num_taps=3)\n"
    ">>> for v in (1, 2, 3):\n"
    "...     d.push(complex(v))\n"
    ">>> d.ptr(3).real.tolist()   # most-recent-first tap snapshot\n"
    "[3.0, 2.0, 1.0]\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_delay (void)
{
  import_array ();
  if (PyType_Ready (&DelayCf64ObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&delay_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&DelayCf64ObjType);
  if (PyModule_AddObject (m, "DelayCf64", (PyObject *)&DelayCf64ObjType) < 0)
    {
      Py_DECREF (&DelayCf64ObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
