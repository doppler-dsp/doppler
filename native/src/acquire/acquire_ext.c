/*
 * acquire_ext.c — Python extension module acquire
 *
 * Objects: CarrierAcquisition, Acquisition, BurstAcquisition, BurstCapture,
 * PersistentBurstCapture GENERATED — do not hand-edit. Patches belong in the
 * _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "clib_common.h"
#include <numpy/arrayobject.h>

#include "acquire/acquire_core.h"

#include "acquire_ext_acq.c"
#include "acquire_ext_burst_acq.c"
#include "acquire_ext_burst_capture.c"
#include "acquire_ext_carrier_acq.c"
#include "acquire_ext_persistentburstcapture.c"

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

static PyMethodDef acquire_module_methods[] = {
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
    ">>> from doppler.acquire import bin_to_signed\n"
    ">>> [bin_to_signed(b, 8) for b in range(8)]\n"
    "[0, 1, 2, 3, -4, -3, -2, -1]\n"
    ">>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention\n"
    "[0, 1, 2, 3, -4, -3, -2, -1]\n"
    ">>> bin_to_signed(4, 7)                         # odd grid: no "
    "ambiguity\n"
    "-3\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef acquire_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "acquire",
  .m_doc     = "Acquisition: the searches that find a signal before anything "
               "tracks it. Acquisition is the code-phase x Doppler engine; "
               "BurstAcquisition and BurstCapture find a burst of any repeated "
               "complex preamble (a PN code, Zadoff-Chu, a chirp or a QPSK "
               "sequence), and PersistentBurstCapture keeps one armed across "
               "calls. CarrierAcquisition is a coarse frequency/phase search that "
               "seeds a carrier tracking loop. bin_to_signed is the Doppler-bin "
               "fold convention the searches and their hand-offs share.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> import numpy as np\n"
               ">>> from doppler.acquire import CarrierAcquisition\n"
               ">>> ca = CarrierAcquisition(sample_rate_hz=8000.0, "
               "symbol_rate_hz=1000.0,\n"
               "...                          resolution_hz=5.0)\n"
               ">>> ca.ready()\n"
               "False\n",
  .m_size    = -1,
  .m_methods = acquire_module_methods,
};

PyMODINIT_FUNC
PyInit_acquire (void)
{
  import_array ();
  if (PyType_Ready (&CarrierAcquisitionObjType) < 0)
    return NULL;
  if (PyType_Ready (&AcquisitionObjType) < 0)
    return NULL;
  if (PyType_Ready (&BurstAcquisitionObjType) < 0)
    return NULL;
  if (PyType_Ready (&BurstCaptureObjType) < 0)
    return NULL;
  if (PyType_Ready (&PersistentBurstCaptureObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&acquire_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&CarrierAcquisitionObjType);
  if (PyModule_AddObject (m, "CarrierAcquisition",
                          (PyObject *)&CarrierAcquisitionObjType)
      < 0)
    {
      Py_DECREF (&CarrierAcquisitionObjType);
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
  Py_INCREF (&BurstCaptureObjType);
  if (PyModule_AddObject (m, "BurstCapture", (PyObject *)&BurstCaptureObjType)
      < 0)
    {
      Py_DECREF (&BurstCaptureObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&PersistentBurstCaptureObjType);
  if (PyModule_AddObject (m, "PersistentBurstCapture",
                          (PyObject *)&PersistentBurstCaptureObjType)
      < 0)
    {
      Py_DECREF (&PersistentBurstCaptureObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
