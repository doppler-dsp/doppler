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
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/acquire/acquire_core.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, except that for a
 * one-byte element type a byte buffer (bytes, bytearray, memoryview) is its
 * bytes, one element per byte (gh-1700). `name` is the parameter, for the
 * message. `hint` is its declared str_hint, or NULL. Declaring one is the
 * opt-in to refusing text (gh-1824): a str, or a bytes numpy would parse as
 * a number, and a str's refusal ends with the hint (gh-1756). With NULL,
 * numpy converts a str as it converts anything else. Returns a new
 * reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  int text     = PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj));
  if (hint && text)
    {
      /* The hint says where text goes instead: a str's refusal only. */
      int say = PyUnicode_Check (obj);
      PyErr_Format (PyExc_TypeError,
                    "%s must be an array of numbers, not %.200s%s%s", name,
                    Py_TYPE (obj)->tp_name, say ? ": " : "", say ? hint : "");
      return NULL;
    }
  if (one_byte && !PyArray_Check (obj) && PyObject_CheckBuffer (obj))
    {
      PyObject *view = PyMemoryView_FromObject (obj);
      if (!view)
        return NULL;
      if (PyMemoryView_GET_BUFFER (view)->itemsize == 1)
        {
          PyObject *raw = PyArray_FromBuffer (
              view, PyArray_DescrFromType (typenum), -1, 0);
          Py_DECREF (view);
          if (!raw)
            return NULL;
          PyObject *arr = PyArray_FROM_OTF (raw, typenum, requirements);
          Py_DECREF (raw);
          return (PyArrayObject *)arr;
        }
      Py_DECREF (view);
    }
  return (PyArrayObject *)PyArray_FROM_OTF (obj, typenum, requirements);
}
/* `unused` (gh-1747): emitted into every extension translation unit,
 * including one that takes no array, or calls only jm_array_arg_hint above
 * -- which needs no mark, since this wrapper always calls it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

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
  return PyLong_FromLong ((long)dp_bin_to_signed (bin, n_bins));
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef acquire_module_methods[] = {
  { "bin_to_signed", (PyCFunction)(void *)_bind_bin_to_signed,
    METH_VARARGS | METH_KEYWORDS,
    "Map an FFT bin index to its SIGNED frequency index -- "
    "numpy.fft.fftfreq(n) * n, exactly: 0 = DC, ascending positive to "
    "(n-1)/2, then wrapping negative, so an even grid's Nyquist bin is -n/2. "
    "Multiply by doppler_res_hz for Hz. Call this rather than writing the "
    "fold out: the search and its hand-off must agree on the convention, and "
    "a consumer seeded on the wrong side of it is off by the full search span "
    "-- a failure that once surfaced here as a receiver reporting tracking "
    "while decoding noise. A thin wrapper over dp_fftfreq_index() in "
    "clib_common.h, so C callers inline the same code.\n"
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
  .m_name = "acquire",
  .m_doc
  = "Acquisition: the searches that find a signal before anything tracks it. "
    "Acquisition is the code-phase x Doppler engine; BurstAcquisition and "
    "BurstCapture find a burst of any repeated complex preamble (a PN code, "
    "Zadoff-Chu, a chirp or a QPSK sequence), and PersistentBurstCapture "
    "keeps the capture's history in a file. CarrierAcquisition is a coarse "
    "frequency/phase search that seeds a carrier tracking loop. bin_to_signed "
    "is the Doppler-bin fold convention the searches and their hand-offs "
    "share.\n"
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
