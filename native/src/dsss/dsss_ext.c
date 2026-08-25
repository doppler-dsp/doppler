/*
 * dsss_ext.c — Python extension module dsss
 *
 * Objects: Despreader, BurstDespreader, Acquisition, BurstAcquisition,
 * PolynomialPhaseEstimator, BurstDemod, DsssReceiver, AsyncDsssReceiver,
 * DsssBurstReceiver GENERATED — do not hand-edit. Patches belong in the
 * _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "dsss/dsss_core.h"

#include "dsss_ext_acq.c"
#include "dsss_ext_async_dsss_receiver.c"
#include "dsss_ext_burst_acq.c"
#include "dsss_ext_burst_demod.c"
#include "dsss_ext_burst_despreader.c"
#include "dsss_ext_despreader.c"
#include "dsss_ext_dsss_burst_receiver.c"
#include "dsss_ext_dsss_receiver.c"
#include "dsss_ext_ppe.c"

static PyObject *
_bind_bin_to_signed (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  static char       *_kwlist[]  = { "bin", "n_bins", NULL };
  unsigned long long bin_raw    = 0ULL;
  unsigned long long n_bins_raw = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "KK", _kwlist, &bin_raw,
                                    &n_bins_raw))
    return NULL;
  size_t bin    = (size_t)bin_raw;
  size_t n_bins = (size_t)n_bins_raw;
  return PyLong_FromLong ((long)bin_to_signed (bin, n_bins));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef dsss_module_methods[] = {
  { "bin_to_signed", (PyCFunction)(void *)_bind_bin_to_signed,
    METH_VARARGS | METH_KEYWORDS,
    "Map an FFT bin index to its SIGNED frequency index --\n"
    "numpy.fft.fftfreq(n) * n, exactly: 0 = DC, ascending positive to\n"
    "(n-1)/2, then wrapping negative, so an even grid's Nyquist bin is -n/2.\n"
    "Multiply by doppler_res_hz for Hz. Call this rather than writing the\n"
    "fold out: the search and its hand-off must agree on the convention, and\n"
    "a consumer seeded on the wrong side of it is off by the full search\n"
    "span -- a failure that once surfaced here as a receiver reporting\n"
    "tracking while decoding noise. A thin wrapper over dp_fftfreq_index()\n"
    "in clib_common.h, so C callers inline the same code.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "bin : int\n"
    "    Bin index in `[0, n_bins)`.\n"
    "n_bins : int\n"
    "    Grid size.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Signed index in `[-(n_bins/2), +((n_bins-1)/2)]`.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.dsss import bin_to_signed\n"
    ">>> [bin_to_signed(b, 8) for b in range(8)]\n"
    "[0, 1, 2, 3, -4, -3, -2, -1]\n"
    ">>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention\n"
    "[0, 1, 2, 3, -4, -3, -2, -1]\n"
    ">>> bin_to_signed(4, 7)                         # odd grid: no "
    "ambiguity\n"
    "-3\n" },
  { NULL, NULL, 0, NULL }
};

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
  .m_methods = dsss_module_methods,
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
  if (PyType_Ready (&DsssBurstReceiverObjType) < 0)
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
  Py_INCREF (&DsssBurstReceiverObjType);
  if (PyModule_AddObject (m, "DsssBurstReceiver",
                          (PyObject *)&DsssBurstReceiverObjType)
      < 0)
    {
      Py_DECREF (&DsssBurstReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
