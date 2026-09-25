/*
 * dsss_ext.c — Python extension module dsss
 *
 * Objects: Despreader, BurstDespreader, PolynomialPhaseEstimator, BurstDemod,
 * DsssReceiver, AsyncDsssReceiver, AsyncDsssPool, DsssBurstReceiver,
 * CellAsyncDsssReceiver GENERATED — do not hand-edit. Patches belong in the
 * _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "dsss_ext_async_dsss_pool.c"
#include "dsss_ext_async_dsss_receiver.c"
#include "dsss_ext_burst_demod.c"
#include "dsss_ext_burst_despreader.c"
#include "dsss_ext_cellasyncdsssreceiver.c"
#include "dsss_ext_despreader.c"
#include "dsss_ext_dsss_burst_receiver.c"
#include "dsss_ext_dsss_receiver.c"
#include "dsss_ext_ppe.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef dsss_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "dsss",
  .m_doc
  = "Direct-sequence spread-spectrum: despreading (Despreader, "
    "BurstDespreader), polynomial-phase estimation and end-to-end receivers "
    "(DsssReceiver, AsyncDsssReceiver, DsssBurstReceiver). The receivers "
    "compose the acquisition engines of doppler.acquire.\n"
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
  .m_size    = -1,
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
  if (PyType_Ready (&PolynomialPhaseEstimatorObjType) < 0)
    return NULL;
  if (!PolynomialPhaseEstimatorObj_estimate_type)
    {
      PolynomialPhaseEstimatorObj_estimate_type = PyStructSequence_NewType (
          &PolynomialPhaseEstimatorObj_estimate_desc);
      if (!PolynomialPhaseEstimatorObj_estimate_type)
        return NULL;
    }
  if (PyType_Ready (&BurstDemodObjType) < 0)
    return NULL;
  if (PyType_Ready (&DsssReceiverObjType) < 0)
    return NULL;
  if (PyType_Ready (&AsyncDsssReceiverObjType) < 0)
    return NULL;
  if (!AsyncDsssReceiverObj_status_type)
    {
      AsyncDsssReceiverObj_status_type
          = PyStructSequence_NewType (&AsyncDsssReceiverObj_status_desc);
      if (!AsyncDsssReceiverObj_status_type)
        return NULL;
    }
  if (PyType_Ready (&AsyncDsssPoolObjType) < 0)
    return NULL;
  if (!AsyncDsssPoolObj_status_type)
    {
      AsyncDsssPoolObj_status_type
          = PyStructSequence_NewType (&AsyncDsssPoolObj_status_desc);
      if (!AsyncDsssPoolObj_status_type)
        return NULL;
    }
  if (PyType_Ready (&DsssBurstReceiverObjType) < 0)
    return NULL;
  if (PyType_Ready (&CellAsyncDsssReceiverObjType) < 0)
    return NULL;
  CellAsyncDsssReceiverObj_status_type
      = AsyncDsssReceiverObj_status_type; /* ReceiverStatus: one public name,
                                             one type */
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
  Py_INCREF (&PolynomialPhaseEstimatorObjType);
  if (PyModule_AddObject (m, "PolynomialPhaseEstimator",
                          (PyObject *)&PolynomialPhaseEstimatorObjType)
      < 0)
    {
      Py_DECREF (&PolynomialPhaseEstimatorObjType);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (
          m, "PolynomialPhaseEstimate",
          (PyObject *)PolynomialPhaseEstimatorObj_estimate_type)
      < 0)
    {
      Py_DECREF (PolynomialPhaseEstimatorObj_estimate_type);
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
  if (PyModule_AddObject (m, "ReceiverStatus",
                          (PyObject *)AsyncDsssReceiverObj_status_type)
      < 0)
    {
      Py_DECREF (AsyncDsssReceiverObj_status_type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&AsyncDsssPoolObjType);
  if (PyModule_AddObject (m, "AsyncDsssPool",
                          (PyObject *)&AsyncDsssPoolObjType)
      < 0)
    {
      Py_DECREF (&AsyncDsssPoolObjType);
      Py_DECREF (m);
      return NULL;
    }
  if (PyModule_AddObject (m, "PoolSlot",
                          (PyObject *)AsyncDsssPoolObj_status_type)
      < 0)
    {
      Py_DECREF (AsyncDsssPoolObj_status_type);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&DsssBurstReceiverObjType);
  if (PyModule_AddObject (m, "DsssBurstReceiver",
                          (PyObject *)&DsssBurstReceiverObjType)
      < 0)
    {
      Py_DECREF (&DsssBurstReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CellAsyncDsssReceiverObjType);
  if (PyModule_AddObject (m, "CellAsyncDsssReceiver",
                          (PyObject *)&CellAsyncDsssReceiverObjType)
      < 0)
    {
      Py_DECREF (&CellAsyncDsssReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
