/*
 * telemetry_ext_dp_tlm_capture_extra.c — MemoryCapture.read_dict(),
 * hand-written.
 *
 * Registered by objects/dp_tlm_capture.toml's
 * [[dp_tlm_capture.extra_methods]] row: jm owns its PyMethodDef entry,
 * forward prototype, stub and the #include of this file (after the
 * generated MemoryCaptureObject), and never touches this file
 * (doppler#1886). CPython hands every method self as PyObject *, so the body
 * casts it. tlm_read_dict.h is include-guarded, so including it here as well
 * as beside Telemetry.read_dict() compiles its helper once.
 */

#include "tlm_read_dict.h"

/* Hand-written, sharing tlm_read_dict.h with Telemetry.read_dict(); only the
   record SOURCE differs. Here it is the capture's own accumulator, borrowed
   and only read — every array handed back is a fresh numpy allocation — and
   unlike the ring drain this does NOT consume, exactly as records() does not.
 */
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
