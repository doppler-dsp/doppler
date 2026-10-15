/*
 * track_ext.c — Python extension module track
 *
 * Objects: LoopFilter, Costas, Dll, SymbolSync, RateSync, CarrierMpsk,
 * CarrierNda, MpskReceiver, BpskReceiver, MpskReceiverR GENERATED — do not
 * hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

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

#ifndef JM_INFLIGHT_DEFINED
#define JM_INFLIGHT_DEFINED
/* gh-2272: the GIL-free calls running on one object, so its explicit
 * teardown can wait for them instead of freeing under them. `n` is read
 * and written only with the GIL held. `idle` exists only once a teardown
 * has had to wait: that teardown holds it, and the last call to leave
 * gives it back. */
typedef struct
{
  Py_ssize_t         n;
  int                waiting;
  PyThread_type_lock idle;
} jm_inflight_t;

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_enter (jm_inflight_t *g)
{
  g->n++;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_leave (jm_inflight_t *g)
{
  if (--g->n == 0 && g->waiting)
    {
      g->waiting = 0;
      PyThread_release_lock (g->idle);
    }
}

/* Called with the GIL held, after the object refuses new calls. Returns
 * with none running: 0, or -1 with MemoryError when no lock could be had,
 * in which case nothing has been waited for. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline int
jm_inflight_wait (jm_inflight_t *g)
{
  while (g->n > 0)
    {
      if (!g->idle)
        {
          g->idle = PyThread_allocate_lock ();
          if (!g->idle)
            {
              PyErr_NoMemory ();
              return -1;
            }
        }
      /* Take the lock, then block on it again with the GIL released:
       * the last jm_inflight_leave releases it. The GIL is dropped by
       * function here, not by the macro pair, so the macro still marks
       * exactly the calls a binding runs without it. */
      (void)PyThread_acquire_lock (g->idle, NOWAIT_LOCK);
      g->waiting        = 1;
      PyThreadState *ts = PyEval_SaveThread ();
      (void)PyThread_acquire_lock (g->idle, WAIT_LOCK);
      PyEval_RestoreThread (ts);
      PyThread_release_lock (g->idle);
    }
  return 0;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static inline void
jm_inflight_free (jm_inflight_t *g)
{
  if (g->idle)
    {
      PyThread_free_lock (g->idle);
      g->idle = NULL;
    }
}
#endif /* JM_INFLIGHT_DEFINED */

#include "track_ext_bpskreceiver.c"
#include "track_ext_carrier_mpsk.c"
#include "track_ext_carrier_nda.c"
#include "track_ext_costas.c"
#include "track_ext_dll.c"
#include "track_ext_loop_filter.c"
#include "track_ext_mpsk_receiver.c"
#include "track_ext_mpskreceiverr.c"
#include "track_ext_ratesync.c"
#include "track_ext_symsync.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef track_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "track",
  .m_doc     = "Carrier and timing tracking: loop filters, Costas / "
               "non-data-aided / MPSK carrier recovery, DLL code tracking, "
               "symbol-timing and rate sync, and full MPSK receivers.\n"
               "\n"
               "Examples\n"
               "--------\n"
               ">>> import numpy as np\n"
               ">>> from doppler.track import LoopFilter\n"
               ">>> y = LoopFilter(bn=0.05, zeta=0.707).steps(np.ones(50))\n"
               ">>> bool(y[-1] > y[0])\n"
               "True\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_track (void)
{
  import_array ();
  if (PyType_Ready (&LoopFilterObjType) < 0)
    return NULL;
  if (PyType_Ready (&CostasObjType) < 0)
    return NULL;
  if (PyType_Ready (&DllObjType) < 0)
    return NULL;
  if (PyType_Ready (&SymbolSyncObjType) < 0)
    return NULL;
  if (PyType_Ready (&RateSyncObjType) < 0)
    return NULL;
  if (PyType_Ready (&CarrierMpskObjType) < 0)
    return NULL;
  if (PyType_Ready (&CarrierNdaObjType) < 0)
    return NULL;
  if (PyType_Ready (&MpskReceiverObjType) < 0)
    return NULL;
  if (PyType_Ready (&BpskReceiverObjType) < 0)
    return NULL;
  if (PyType_Ready (&MpskReceiverRObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&track_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&LoopFilterObjType);
  if (PyModule_AddObject (m, "LoopFilter", (PyObject *)&LoopFilterObjType) < 0)
    {
      Py_DECREF (&LoopFilterObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CostasObjType);
  if (PyModule_AddObject (m, "Costas", (PyObject *)&CostasObjType) < 0)
    {
      Py_DECREF (&CostasObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&DllObjType);
  if (PyModule_AddObject (m, "Dll", (PyObject *)&DllObjType) < 0)
    {
      Py_DECREF (&DllObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&SymbolSyncObjType);
  if (PyModule_AddObject (m, "SymbolSync", (PyObject *)&SymbolSyncObjType) < 0)
    {
      Py_DECREF (&SymbolSyncObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&RateSyncObjType);
  if (PyModule_AddObject (m, "RateSync", (PyObject *)&RateSyncObjType) < 0)
    {
      Py_DECREF (&RateSyncObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CarrierMpskObjType);
  if (PyModule_AddObject (m, "CarrierMpsk", (PyObject *)&CarrierMpskObjType)
      < 0)
    {
      Py_DECREF (&CarrierMpskObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&CarrierNdaObjType);
  if (PyModule_AddObject (m, "CarrierNda", (PyObject *)&CarrierNdaObjType) < 0)
    {
      Py_DECREF (&CarrierNdaObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&MpskReceiverObjType);
  if (PyModule_AddObject (m, "MpskReceiver", (PyObject *)&MpskReceiverObjType)
      < 0)
    {
      Py_DECREF (&MpskReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&BpskReceiverObjType);
  if (PyModule_AddObject (m, "BpskReceiver", (PyObject *)&BpskReceiverObjType)
      < 0)
    {
      Py_DECREF (&BpskReceiverObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&MpskReceiverRObjType);
  if (PyModule_AddObject (m, "MpskReceiverR",
                          (PyObject *)&MpskReceiverRObjType)
      < 0)
    {
      Py_DECREF (&MpskReceiverRObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
