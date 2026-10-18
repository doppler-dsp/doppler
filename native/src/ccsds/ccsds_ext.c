/*
 * ccsds_ext.c — Python extension module ccsds
 *
 * Objects:
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <numpy/arrayobject.h>

#include "doppler/ccsds/ccsds_core.h"

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

#ifndef JM_CAPSULE_LEND_DEFINED
#define JM_CAPSULE_LEND_DEFINED
/* gh-2328: a lent capsule's context is the object that owns its pointer.
 * The destructor releases that reference and nothing else -- the pointer is
 * the owner's to free -- so whoever keeps the capsule keeps the owner. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static void
jm_capsule_lend_release (PyObject *cap)
{
  Py_XDECREF ((PyObject *)PyCapsule_GetContext (cap));
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((unused))
#endif
static PyObject *
jm_capsule_lend (void *ptr, const char *name, PyObject *owner)
{
  PyObject *cap = PyCapsule_New (ptr, name, jm_capsule_lend_release);
  if (!cap)
    return NULL;
  Py_INCREF (owner);
  if (PyCapsule_SetContext (cap, owner) < 0)
    {
      Py_DECREF (owner);
      Py_DECREF (cap);
      return NULL;
    }
  return cap;
}
#endif /* JM_CAPSULE_LEND_DEFINED */

static PyObject *
_bind_asm_bits (PyObject *self, PyObject *Py_UNUSED (args))
{
  (void)self;
  size_t _dim_need = (size_t)(32);
  if (_dim_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "asm_bits: output of %zu elements is too large",
                    _dim_need);
      return NULL;
    }
  npy_intp  _dim = (npy_intp)_dim_need;
  PyObject *_out = PyArray_EMPTY (1, &_dim, NPY_UINT8, 0);
  if (!_out)
    {
      return NULL;
    }
  dp_asm_bits ((uint8_t *)PyArray_DATA ((PyArrayObject *)_out));
  return _out;
}

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyMethodDef ccsds_module_methods[] = {
  { "asm_bits", _bind_asm_bits, METH_NOARGS,
    "The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits — "
    "`out[0]` is the first bit on the wire (the top of 0x1A). Pass it to "
    "`doppler.detection.SyncFinder` to acquire a CADU in a bit stream; it is "
    "NOT randomised, so it reads the same in every frame and in exactly one "
    "polarity, which is what makes it the thing that reports a 180-degree "
    "carrier ambiguity.\n"
    "\n"
    "`out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers\n"
    "the marker's bit 0 as the most significant bit of 0x1A. One bit per\n"
    "byte, the convention every frame path here passes around.\n"
    "\n"
    "The thing a Python receiver ACQUIRES on: pair it with\n"
    "`doppler.detection.SyncFinder` to find where a CADU starts in a bit\n"
    "stream, then slice and `Frame.check()` it. The marker is deliberately\n"
    "NOT randomised (10.4's NOTE: \"The ASM was not randomized and is not\n"
    "derandomized\"), so it reads the same in every frame and in exactly one\n"
    "polarity — which is what makes it the only thing in a CADU that can\n"
    "report a 180-degree carrier ambiguity.\n"
    "\n"
    "A function rather than a constant a caller expands, because an\n"
    "MSB-first expansion written out twice is a transcription that can\n"
    "disagree with itself. This tree's own doctests were the second copy\n"
    "until doppler#900, and this alias exists so the third copy is not a\n"
    "Python one: it delegates to `dp_ccsds_tm_asm_bits`, which is where the\n"
    "expansion is written and where `test_ccsds_tm_asm` holds it to the\n"
    "published pattern.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[np.uint8]\n"
    "    Output.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> b = asm_bits()\n"
    ">>> b.size, b[:8].tolist()          # 0x1A, first bit at the top\n"
    "(32, [0, 0, 0, 1, 1, 0, 1, 0])\n"
    ">>> int(\"\".join(map(str, b.tolist())), 2) == 0x1ACFFC1D\n"
    "True\n" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef ccsds_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "ccsds",
  .m_doc
  = "CCSDS 131.0-B TM Synchronization and Channel Coding — the literals the "
    "standard picked, beside the general layer rather than inside it.\n"
    "\n"
    "`asm_bits()` is the Attached Sync Marker as bits, which is what a "
    "receiver acquires on: pair it with `doppler.detection.SyncFinder` to "
    "find where a CADU starts in a bit stream. The standard's *transforms* "
    "are not here and are not coming here — describe a CADU with "
    "`doppler.wfm.FrameDesc` and the outer code, the randomiser and the inner "
    "code run through the general assembler.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.ccsds import asm_bits\n"
    ">>> b = asm_bits()\n"
    ">>> int(\"\".join(map(str, b.tolist())), 2) == 0x1ACFFC1D\n"
    "True\n",
  .m_size    = -1,
  .m_methods = ccsds_module_methods,
};

PyMODINIT_FUNC
PyInit_ccsds (void)
{
  import_array ();

  PyObject *m = PyModule_Create (&ccsds_moduledef);
  if (!m)
    return NULL;

  return m;
}
