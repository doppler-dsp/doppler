/*
 * dsss_ext.c — Python extension module dsss
 *
 * Objects: Despreader, BurstDespreader, Acquisition, BurstAcquisition,
 * PolynomialPhaseEstimator, BurstDemod, DsssReceiver, AsyncDsssReceiver
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "dsss_ext_acq.c"
#include "dsss_ext_async_dsss_receiver.c"
#include "dsss_ext_burst_acq.c"
#include "dsss_ext_burst_demod.c"
#include "dsss_ext_burst_despreader.c"
#include "dsss_ext_despreader.c"
#include "dsss_ext_dsss_receiver.c"
#include "dsss_ext_ppe.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef dsss_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "dsss",
  .m_doc  = "Direct-sequence spread-spectrum: the full chain from acquisition "
            "(Acquisition, BurstAcquisition) through despreading (Despreader, "
            "BurstDespreader) and polynomial-phase estimation to end-to-end "
            "receivers (DsssReceiver, AsyncDsssReceiver).\n"
            "\n"
            "Examples\n"
            "--------\n"
            ">>> import numpy as np\n"
            ">>> from doppler.dsss import Despreader\n"
            ">>> rng = np.random.default_rng(0)\n"
            ">>> code = rng.integers(0, 2, 31).astype(np.uint8)\n"
            ">>> csign = np.where(code & 1, -1.0, 1.0)\n"
            ">>> tx = np.repeat(np.tile(csign, 20), 2).astype(np.complex64)\n"
            ">>> bool(Despreader(code, sps=2).steps(tx).size >= 19)\n"
            "True\n",
  .m_size = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_dsss (void)
{
  import_array ();
  if (PyType_Ready (&DespreaderObjType) < 0)
    return NULL;
  if (PyType_Ready (&BurstDespreaderObjType) < 0)
    return NULL;
  if (PyType_Ready (&AcquisitionObjType) < 0)
    return NULL;
  if (PyType_Ready (&BurstAcquisitionObjType) < 0)
    return NULL;
  if (PyType_Ready (&PolynomialPhaseEstimatorObjType) < 0)
    return NULL;
  if (PyType_Ready (&BurstDemodObjType) < 0)
    return NULL;
  if (PyType_Ready (&DsssReceiverObjType) < 0)
    return NULL;
  if (PyType_Ready (&AsyncDsssReceiverObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&dsss_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&DespreaderObjType);
  if (PyModule_AddObject (m, "Despreader", (PyObject *)&DespreaderObjType) < 0)
    {
      Py_DECREF (&DespreaderObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&BurstDespreaderObjType);
  if (PyModule_AddObject (m, "BurstDespreader",
                          (PyObject *)&BurstDespreaderObjType)
      < 0)
    {
      Py_DECREF (&BurstDespreaderObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&AcquisitionObjType);
  if (PyModule_AddObject (m, "Acquisition", (PyObject *)&AcquisitionObjType)
      < 0)
    {
      Py_DECREF (&AcquisitionObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&BurstAcquisitionObjType);
  if (PyModule_AddObject (m, "BurstAcquisition",
                          (PyObject *)&BurstAcquisitionObjType)
      < 0)
    {
      Py_DECREF (&BurstAcquisitionObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&PolynomialPhaseEstimatorObjType);
  if (PyModule_AddObject (m, "PolynomialPhaseEstimator",
                          (PyObject *)&PolynomialPhaseEstimatorObjType)
      < 0)
    {
      Py_DECREF (&PolynomialPhaseEstimatorObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&BurstDemodObjType);
  if (PyModule_AddObject (m, "BurstDemod", (PyObject *)&BurstDemodObjType) < 0)
    {
      Py_DECREF (&BurstDemodObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&DsssReceiverObjType);
  if (PyModule_AddObject (m, "DsssReceiver", (PyObject *)&DsssReceiverObjType)
      < 0)
    {
      Py_DECREF (&DsssReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&AsyncDsssReceiverObjType);
  if (PyModule_AddObject (m, "AsyncDsssReceiver",
                          (PyObject *)&AsyncDsssReceiverObjType)
      < 0)
    {
      Py_DECREF (&AsyncDsssReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
