/*
 * telemetry_ext_dp_tlm_capture_extra.c — the MemoryCapture method jm cannot
 * render.
 *
 * read_dict() returns a dict keyed by probe name whose values are numpy
 * arrays of data-dependent length — a shape with no manifest spelling. jm
 * owns the rest of the type (telemetry_ext_dp_tlm_capture.c, fragment =
 * "generated"); this method is registered by the
 * `[[dp_tlm_capture.extra_methods]]` row in objects/dp_tlm_capture.toml,
 * which gives it its PyMethodDef row, prototype and .pyi member (gh-1997).
 * The module aggregator includes this file AFTER the generated types, so
 * `MemoryCaptureObject` is in scope; jm never writes it.
 *
 * It shares tlm_read_dict.h with Telemetry.read_dict(); only the record
 * SOURCE differs. Here it is the capture's own accumulator, borrowed and only
 * read — every array handed back is a fresh numpy allocation — and unlike the
 * ring drain this does NOT consume, exactly as records() does not.
 */

#include "tlm_read_dict.h"

static PyObject *
MemoryCaptureObj_read_dict (PyObject *obj, PyObject *args, PyObject *kwds)
{
  MemoryCaptureObject *self = (MemoryCaptureObject *)obj;
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", "index", NULL };
  PyObject          *n_obj     = NULL;
  unsigned long long n_raw     = 0;
  int                with_idx  = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Op", _kwlist, &n_obj,
                                    &with_idx))
    return NULL;
  /* The conversion jm renders for read()'s `n`, so both faces refuse a
   * count outside size_t identically: `K` masked it, so -1 meant "all" and
   * 2**64 + 5 meant 5. Refused before anything is drained. */
  if (n_obj != NULL)
    {
      PyObject *_ix = PyNumber_Index (n_obj);
      if (_ix == NULL)
        {
          return NULL;
        }
      n_raw = PyLong_AsUnsignedLongLong (_ix);
      if ((n_raw == (unsigned long long)-1 && PyErr_Occurred ())
          || n_raw > (unsigned long long)SIZE_MAX)
        {
          PyErr_Format (PyExc_OverflowError,
                        "n: %S is out of range for size_t [0, %llu]", _ix,
                        (unsigned long long)SIZE_MAX);
          Py_DECREF (_ix);
          return NULL;
        }
      Py_DECREF (_ix);
    }

  /* The context carries the registry the ids resolve against. */
  dp_tlm_t *tlm = dp_tlm_capture_context (self->handle);
  if (!tlm)
    {
      PyErr_SetString (PyExc_RuntimeError, "read_dict: no context");
      return NULL;
    }

  const dp_tlm_rec_t *recs  = dp_tlm_capture_records (self->handle);
  size_t              n_out = dp_tlm_capture_count (self->handle);
  if (n_raw != 0 && n_out > (size_t)n_raw)
    n_out = (size_t)n_raw;

  return tlm_build_read_dict (tlm, recs, n_out, with_idx);
}
