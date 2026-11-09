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
  unsigned long long n_raw     = 0;
  int                with_idx  = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|Kp", _kwlist, &n_raw,
                                    &with_idx))
    return NULL;

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
