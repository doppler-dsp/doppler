/*
 * telemetry_ext_dp_event_log.c — EventLog type for the telemetry module.
 *
 * Included by telemetry_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only telemetry_ext.c is compiled.
 */
/* ======================================================== */
/* EventLogObject — wraps dp_event_log_state_t *       */
/* ======================================================== */

#include "dp_event_log/dp_event_log_core.h"

typedef struct
{
  PyObject_HEAD dp_event_log_state_t *handle;
} EventLogObject;

static void
EventLogObj_dealloc (EventLogObject *self)
{
  if (self->handle)
    {
      /* gh-541: tp_dealloc has no exception context — there
         is no caller to raise to, and an in-flight exception
         must not be clobbered. Discarding the status is the
         only correct choice here; the explicit teardown and
         __exit__ paths do report it. */
      (void)dp_event_log_destroy (self->handle);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
EventLogObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  EventLogObject *self = (EventLogObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
EventLogObj_init (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "path", "fc", NULL };
  PyObject    *path     = NULL; /* fspath -> bytes */
  double       fc       = 0.0;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&|d", kwlist,
                                    PyUnicode_FSConverter, &path, &fc))
    {
      Py_XDECREF (path);
      return -1;
    }
  self->handle = dp_event_log_open (PyBytes_AS_STRING (path), fc);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_OSError,
                       "event log could not be opened: the path must be "
                       "non-empty and writable (the file is truncated if it "
                       "exists)");
      return -1;
    }
  return 0;
}

/* gh-519: strcmp for the enum lookup below. Python.h already
 * pulls in <string.h>, but the include is explicit so the block
 * stands on its own wherever it is spliced. */
#include <string.h>

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index_EventLog (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_EventLog_stype[] = {
  "cf32", "cf64", "ci32", "ci16", "ci8", "f32",
  "f64",  "i32",  "i16",  "i8",   NULL,
};

static const char *const _enum_EventLog_endian[] = {
  "le",
  "be",
  NULL,
};

static PyObject *
EventLogObj_field (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "name", "value", NULL };
  const char  *name      = NULL;
  double       value     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sd", _kwlist, &name, &value))
    return NULL;
  int _rc = dp_event_log_field (self->handle, name, value);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "field: the name is empty or longer than 31 bytes, the "
                    "value is not finite (JSON has no NaN), or 16 fields are "
                    "already staged",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_field_str (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "name", "value", NULL };
  const char  *name      = NULL;
  const char  *value     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ss", _kwlist, &name, &value))
    return NULL;
  int _rc = dp_event_log_field_str (self->handle, name, value);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "field_str: the name is empty or longer than 31 bytes, "
                    "the value is longer than 63 bytes, or 16 fields are "
                    "already staged",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_append (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "sample_start", "label",        "sample_count",
                             "freq_hz",      "bandwidth_hz", NULL };
  unsigned long long sample_start_raw = 0ULL;
  const char        *label            = NULL;
  unsigned long long sample_count_raw = 0;
  double             freq_hz          = 0.0;
  double             bandwidth_hz     = 0.0;
  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "Ks|Kdd", _kwlist, &sample_start_raw, &label,
          &sample_count_raw, &freq_hz, &bandwidth_hz))
    return NULL;
  uint64_t sample_start = (uint64_t)sample_start_raw;
  uint64_t sample_count = (uint64_t)sample_count_raw;
  int      _rc = dp_event_log_append (self->handle, sample_start, label,
                                      sample_count, freq_hz, bandwidth_hz);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "append: the event could not be written (the log is "
                    "closed, or the write failed)",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_finalize (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[]
      = { "meta_path", "sample_type", "endian", "fs", "t0", NULL };
  const char *meta_path   = NULL;
  const char *sample_type = "cf32";
  const char *endian      = "le";
  double      fs          = 0.0;
  double      t0          = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|ssdd", _kwlist, &meta_path,
                                    &sample_type, &endian, &fs, &t0))
    return NULL;
  int _arg_sample_type
      = _enum_index_EventLog (_enum_EventLog_stype, sample_type);
  if (_arg_sample_type < 0)
    {
      PyErr_Format (PyExc_ValueError,
                    "invalid sample_type '%s' (choices: cf32, cf64, ci32, "
                    "ci16, ci8, f32, f64, i32, i16, i8)",
                    sample_type);
      return NULL;
    }
  int _arg_endian = _enum_index_EventLog (_enum_EventLog_endian, endian);
  if (_arg_endian < 0)
    {
      PyErr_Format (PyExc_ValueError, "invalid endian '%s' (choices: le, be)",
                    endian);
      return NULL;
    }
  int _rc = dp_event_log_finalize (self->handle, meta_path, _arg_sample_type,
                                   _arg_endian, fs, t0);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "finalize: the event file could not be read or the "
                    "sidecar could not be written",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_set_dataset (EventLogObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "name", NULL };
  const char  *name      = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", _kwlist, &name))
    return NULL;
  int _rc = dp_event_log_set_dataset (self->handle, name);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_dataset failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_set_telemetry (EventLogObject *self, PyObject *args,
                           PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "path", NULL };
  const char  *path      = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", _kwlist, &path))
    return NULL;
  int _rc = dp_event_log_set_telemetry (self->handle, path);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "set_telemetry failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_close (EventLogObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_event_log_close (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "close: an event failed to reach the disk during this "
                    "run (the error is sticky — an earlier append reports "
                    "here)",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
EventLog_getprop_count (EventLogObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_event_log_count (self->handle)));
}

static PyGetSetDef EventLog_getset[] = {
  { "count", (getter)EventLog_getprop_count, NULL,
    "Events appended so far, counting only the ones that reached the disk.\n",
    NULL },
  { NULL }
};

static PyObject *
EventLogObj_destroy (EventLogObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc = dp_event_log_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_OSError,
                           "close: an event failed to reach the disk during "
                           "this run (the error is sticky — an earlier "
                           "append reports here)");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
EventLogObj_enter (EventLogObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
EventLogObj_exit (EventLogObject *self, PyObject *args)
{
  (void)args;
  if (!self->handle)
    Py_RETURN_NONE;
  /* gh-805 §H: the handle deliberately SURVIVES this call —
     finalize is not free, and the captured results only become
     valid once it has run. The free stays in tp_dealloc. */
  int _rc = dp_event_log_close (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "close: an event failed to reach the disk during this "
                    "run (the error is sticky — an earlier append reports "
                    "here)",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef EventLogObj_methods[] = {

  { "field", (PyCFunction)(void *)EventLogObj_field,
    METH_VARARGS | METH_KEYWORDS,
    "field(name, value) -> None\n"
    "\n"
    "Stages a numeric field for the next event.\n"
    "\n"
    "Rendered as `doppler:<name>` in the annotation, then cleared. Integral\n"
    "values print as integers (`3`, not `3.0`), which is what a reader\n"
    "expects of an emitter id.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Field name, without the namespace — `\"emitter\"`, not\n"
    "    `\"doppler:emitter\"`. Up to 31 bytes.\n"
    "value : float\n"
    "    The value. A non-finite value is refused rather than written: JSON\n"
    "    has no NaN, and a sidecar that cannot be parsed is worse than a\n"
    "    missing field.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``field: the name is empty or longer than 31 bytes, the value is\n"
    "    not finite (JSON has no NaN), or 16 fields are already staged``,\n"
    "    with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"))\n"
    ">>> log.field(\"cn0_db_hz\", 47.5)\n"
    ">>> log.field(\"emitter\", 3)          # integral: renders as 3\n"
    ">>> log.append(1024, \"tracking\")\n"
    ">>> log.count\n"
    "1\n"
    ">>> log.close()\n" },
  { "field_str", (PyCFunction)(void *)EventLogObj_field_str,
    METH_VARARGS | METH_KEYWORDS,
    "field_str(name, value) -> None\n"
    "\n"
    "Stages a string field for the next event.\n"
    "\n"
    "The string face of dp_event_log_field(), for the fields that are names\n"
    "rather than numbers — a state, a reason, a code.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Field name, without the namespace.\n"
    "value : str\n"
    "    The value; copied, up to 63 bytes.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``field_str: the name is empty or longer than 31 bytes, the value\n"
    "    is longer than 63 bytes, or 16 fields are already staged``, with\n"
    "    the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"))\n"
    ">>> log.field_str(\"state\", \"tracking\")\n"
    ">>> log.append(1024, \"seeded\")\n"
    ">>> log.close()\n"
    ">>> line = open(os.path.join(d, \"run.events\")).readline()\n"
    ">>> json.loads(line)[\"doppler:state\"]\n"
    "'tracking'\n" },
  { "append", (PyCFunction)(void *)EventLogObj_append,
    METH_VARARGS | METH_KEYWORDS,
    "append(sample_start, label, sample_count, freq_hz, bandwidth_hz) -> "
    "None\n"
    "\n"
    "Appends one event and consumes the staged fields.\n"
    "\n"
    "Writes one JSON object on one line and flushes it, so a reader tailing\n"
    "the file sees the event as it happens and a crash cannot cost an\n"
    "earlier one. The staged fields are cleared whether or not the write\n"
    "succeeded — an event that failed to reach the disk must not leak its\n"
    "fields into the next one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "sample_start : int\n"
    "    Stream position of the event → `core:sample_start`.\n"
    "label : str\n"
    "    `core:label` — `\"seeded\"`, `\"tracking\"`, `\"lost\"`, `\"gap\"`,\n"
    "    whatever the holder calls it. It comes before the span, and has no\n"
    "    default, because an unlabelled transition is a position with\n"
    "    nothing said about it: the label is the event.\n"
    "sample_count : int\n"
    "    Span in samples → `core:sample_count`. 0 means an INSTANT and the\n"
    "    key is omitted, which is the honest spelling: a transition happens\n"
    "    at a sample, and a written `0` would claim a measured span of\n"
    "    nothing.\n"
    "freq_hz : float\n"
    "    Offset from the channel centre (Hz), positive above.\n"
    "bandwidth_hz : float\n"
    "    Occupied width (Hz). <= 0.0 means \"no band stated\", and then\n"
    "    neither the edges nor `doppler:freq_hz` appear: an event like a\n"
    "    stream gap has no frequency, and a 0 Hz offset written for it would\n"
    "    read as an on-centre emitter.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``append: the event could not be written (the log is closed, or the\n"
    "    write failed)``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"))\n"
    ">>> log.append(48000, \"seeded\")            # an instant\n"
    ">>> log.append(96000, \"gap\", sample_count=1024)   # a span\n"
    ">>> log.close()\n"
    ">>> rows = [json.loads(x) for x in\n"
    "...         open(os.path.join(d, \"run.events\"))]\n"
    ">>> \"core:sample_count\" in rows[0], rows[1][\"core:sample_count\"]\n"
    "(False, 1024)\n" },
  { "finalize", (PyCFunction)(void *)EventLogObj_finalize,
    METH_VARARGS | METH_KEYWORDS,
    "finalize(meta_path, sample_type, endian, fs, t0) -> None\n"
    "\n"
    "Writes the `.sigmf-meta` sidecar for this log's events.\n"
    "\n"
    "Flushes the flat file and renders it through dp_event_log_write_meta(),\n"
    "with this log's own path and fc. The log stays open and usable\n"
    "afterwards: a long run can emit a sidecar per hour and keep going.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "meta_path : str\n"
    "    Sidecar to write, conventionally `<base>.sigmf-meta`.\n"
    "sample_type : str\n"
    "    Dataset wire type (wavegen order) → `core:datatype`.\n"
    "endian : str\n"
    "    0 little, 1 big.\n"
    "fs : float\n"
    "    Sample rate (Hz), or 0.0 to leave `core:sample_rate` unstated. The\n"
    "    dataset and the telemetry file come from dp_event_log_set_dataset()\n"
    "    / _set_telemetry().\n"
    "t0 : float\n"
    "    Input.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``finalize: the event file could not be read or the sidecar could\n"
    "    not be written``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"), fc=2.4e9)\n"
    ">>> log.append(48000, \"seeded\", bandwidth_hz=4.0e6)\n"
    ">>> meta = os.path.join(d, \"run.sigmf-meta\")\n"
    ">>> log.finalize(meta, fs=1.0e7)\n"
    ">>> log.close()\n"
    ">>> json.load(open(meta))[\"captures\"][0][\"core:frequency\"]\n"
    "2400000000\n" },
  { "set_dataset", (PyCFunction)(void *)EventLogObj_set_dataset,
    METH_VARARGS | METH_KEYWORDS,
    "set_dataset(name) -> None\n"
    "\n"
    "Names the sample file these events index.\n"
    "\n"
    "A property of the RUN, not of a sidecar, which is why it is set once\n"
    "here rather than passed to every dp_event_log_finalize() — a long run\n"
    "writes a sidecar an hour and the dataset does not change between them.\n"
    "\n"
    "Unset (the default) means there is nothing on disk to point at — a live\n"
    "NATS stream that nobody recorded — and the sidecar then says\n"
    "`core:metadata_only`, which is SigMF's own word for it rather than an\n"
    "absent key a reader has to interpret.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Dataset basename, copied. NULL or empty restores \"none\".\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_dataset failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"))\n"
    ">>> log.set_dataset(\"capture.sigmf-data\")\n"
    ">>> log.append(0, \"seeded\")\n"
    ">>> meta = os.path.join(d, \"run.sigmf-meta\")\n"
    ">>> log.finalize(meta)\n"
    ">>> log.close()\n"
    ">>> json.load(open(meta))[\"global\"][\"core:dataset\"]\n"
    "'capture.sigmf-data'\n" },
  { "set_telemetry", (PyCFunction)(void *)EventLogObj_set_telemetry,
    METH_VARARGS | METH_KEYWORDS,
    "set_telemetry(path) -> None\n"
    "\n"
    "Names the `dp_tlm` record file written for the same run.\n"
    "\n"
    "Carried with its record dtype under a `doppler:telemetry` global, so\n"
    "one sidecar indexes all three products of a run — the dataset, the\n"
    "events, the telemetry — each in the format that suits its rate:\n"
    "annotations for transitions at a handful a minute, a flat record file\n"
    "for a time series at thousands a second (§8.1).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str\n"
    "    Telemetry record file, copied. NULL or empty restores \"none\".\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``set_telemetry failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"))\n"
    ">>> log.set_telemetry(\"run.tlm\")\n"
    ">>> log.append(0, \"seeded\")\n"
    ">>> meta = os.path.join(d, \"run.sigmf-meta\")\n"
    ">>> log.finalize(meta)\n"
    ">>> log.close()\n"
    ">>> json.load(open(meta))[\"global\"][\"doppler:telemetry\"][\"path\"]\n"
    "'run.tlm'\n" },
  { "close", (PyCFunction)EventLogObj_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Closes the flat file, keeping the object readable.\n"
    "\n"
    "Idempotent — a second call is ::DP_OK and does nothing. Separate from\n"
    "the destructor because a close can FAIL (the last buffered bytes\n"
    "meeting a full disk) and a caller is entitled to hear about it; the\n"
    "destructor cannot return.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``close: an event failed to reach the disk during this run (the\n"
    "    error is sticky — an earlier append reports here)``, with the\n"
    "    return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> from doppler.telemetry import EventLog\n"
    "    >>> obj = EventLog(path=..., fc=0.0)\n"
    "    >>> obj.close()\n" },
  { "destroy", (PyCFunction)EventLogObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C destructor reports failure. Raised from an explicit call\n"
    "    and from ``__exit__`` alike, so a failing teardown propagates out\n"
    "    of a ``with`` block (gh-541).\n" },
  { "__enter__", (PyCFunction)EventLogObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a EventLog be used in a `with` statement so its C resources are\n"
    "finalized deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "EventLog\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)EventLogObj_exit, METH_VARARGS,
    "Exit a context manager, finalizing the EventLog.\n"
    "\n"
    "Equivalent to calling `close()`. The EventLog is **not** released here:\n"
    "it stays usable, which is what makes results gathered during the `with`\n"
    "body readable after it. The memory is freed when the object is\n"
    "collected.\n"
    "\n"
    "Returns ``None``, so an exception raised inside the `with` body\n"
    "propagates normally; this never suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If ``close()`` reports failure. ``__exit__`` calls it and raises\n"
    "    what it raises, so a failed finalize propagates out of the ``with``\n"
    "    block (gh-805 §H).\n" },
  { NULL }
};

static PyTypeObject EventLogObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "telemetry.EventLog",
  .tp_basicsize                           = sizeof (EventLogObject),
  .tp_dealloc                             = (destructor)EventLogObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Opens (truncating) the flat event file for a run.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str | os.PathLike\n"
    "    Flat event file, truncated if it exists. One JSON object per line,\n"
    "    flushed per event, so it can be tailed while the run is live and a\n"
    "    crash costs at most the event being written.\n"
    "fc : float, default 0.0\n"
    "    Channel centre frequency (Hz), or 0.0 when the input does not state "
    "one\n"
    "    — a NATS stream, typically. It decides two things together, which "
    "is\n"
    "    why it is one argument: whether an event can carry absolute\n"
    "    core:freq_lower_edge/upper_edge keys, and what captures[0] reports "
    "as\n"
    "    core:frequency. Unknown means OMITTED, never guessed.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If construction fails. The exception message is ``event log could "
    "not\n"
    "    be opened: the path must be non-empty and writable (the file is\n"
    "    truncated if it exists)``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import json, os, tempfile\n"
    ">>> from doppler.telemetry import EventLog\n"
    ">>> d = tempfile.mkdtemp()\n"
    ">>> log = EventLog(os.path.join(d, \"run.events\"), fc=2.4e9)\n"
    ">>> log.field(\"emitter\", 3)\n"
    ">>> log.field_str(\"state\", \"tracking\")\n"
    ">>> log.append(48000, \"seeded\", bandwidth_hz=4.0e6)\n"
    ">>> log.finalize(os.path.join(d, \"run.sigmf-meta\"), fs=10e6)\n"
    ">>> m = json.load(open(os.path.join(d, \"run.sigmf-meta\")))\n"
    ">>> m[\"annotations\"][0][\"core:sample_start\"]\n"
    "48000\n"
    ">>> m[\"annotations\"][0][\"doppler:state\"]\n"
    "'tracking'\n"
    ">>> log.close()\n",
  .tp_methods = EventLogObj_methods,
  .tp_getset  = EventLog_getset,
  .tp_new     = EventLogObj_new,
  .tp_init    = (initproc)EventLogObj_init,
};
