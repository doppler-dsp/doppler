/*
 * telemetry_ext_dp_tlm_extra.c — the Telemetry method jm cannot render.
 *
 * read_dict() returns a dict whose KEYS come from the probe registry and
 * whose VALUES are numpy arrays of data-dependent length — a shape with no
 * manifest spelling. jm owns the rest of the type (telemetry_ext_dp_tlm.c,
 * fragment = "generated"); this method is registered by the
 * `[[dp_tlm.extra_methods]]` row in objects/dp_tlm.toml, which gives it its
 * PyMethodDef row, prototype and .pyi member (gh-1997). The module aggregator
 * includes this file AFTER the generated types, so `TelemetryObject` is in
 * scope; jm never writes it.
 *
 * The marshalling is shared with MemoryCapture.read_dict() in
 * tlm_read_dict.h; only the record SOURCE differs, and for this face that is
 * a drain of the ring.
 */

#include "tlm_read_dict.h"

static PyObject *
TelemetryObj_read_dict (PyObject *obj, PyObject *args, PyObject *kwds)
{
  TelemetryObject *self = (TelemetryObject *)obj;
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

  size_t        cap   = dp_tlm_read_max_out (self->handle);
  dp_tlm_rec_t *recs  = NULL;
  size_t        n_out = 0;
  if (cap > 0)
    {
      recs = (dp_tlm_rec_t *)PyMem_Malloc (cap * sizeof *recs);
      if (!recs)
        return PyErr_NoMemory ();
      n_out = dp_tlm_read (self->handle, (size_t)n_raw, recs, cap);
    }

  PyObject *out = tlm_build_read_dict (self->handle, recs, n_out, with_idx);
  PyMem_Free (recs);
  return out;
}
