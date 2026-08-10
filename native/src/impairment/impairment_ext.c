/*
 * impairment_ext.c — Python extension module impairment
 *
 * Objects: DopplerChannel
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "impairment_ext_doppler_channel.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef impairment_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "impairment",
  .m_doc
  = "Channel impairments: a DopplerChannel applying carrier offset, delay, "
    "and additive white Gaussian noise to a signal for test and simulation.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.impairment import DopplerChannel\n"
    ">>> ch = DopplerChannel(fs=1e6, carrier_hz=1e5, doppler_ppm=10.0)\n"
    ">>> bool(ch.execute(np.ones(64, np.complex64)).size > 0)\n"
    "True\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_impairment (void)
{
  import_array ();
  if (PyType_Ready (&DopplerChannelObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&impairment_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&DopplerChannelObjType);
  if (PyModule_AddObject (m, "DopplerChannel",
                          (PyObject *)&DopplerChannelObjType)
      < 0)
    {
      Py_DECREF (&DopplerChannelObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
