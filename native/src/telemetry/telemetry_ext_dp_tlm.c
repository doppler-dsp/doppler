/*
 * telemetry_ext_dp_tlm.c — Telemetry type for the telemetry module.
 *
 * Included by telemetry_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only telemetry_ext.c is compiled.
 */
/* ======================================================== */
/* TelemetryObject — wraps dp_tlm_state_t *       */
/* ======================================================== */

#include "dp_tlm/dp_tlm_core.h"

typedef struct
{
  PyObject_HEAD dp_tlm_state_t *handle;
} TelemetryObject;

static void
TelemetryObj_dealloc (TelemetryObject *self)
{
  if (self->handle)
    dp_tlm_destroy (self->handle);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
TelemetryObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  TelemetryObject *self = (TelemetryObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
TelemetryObj_init (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char       *kwlist[]         = { "ring_records", NULL };
  unsigned long long ring_records_raw = 16384;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", kwlist,
                                    &ring_records_raw))
    return -1;
  size_t ring_records = (size_t)ring_records_raw;
  self->handle        = dp_tlm_create (ring_records);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "ring_records must be a power of two (and at least "
                       "the page minimum); read the granted size back from "
                       "Telemetry.capacity");
      return -1;
    }
  return 0;
}

static PyArray_Descr *TelemetryObj_read_dtype = NULL;

/* The record's numpy dtype, built from the compiler's own layout:
   offsetof/sizeof, never numpy's packing rules, so a padded
   struct cannot silently read every row after the first from the
   wrong bytes. */
static PyArray_Descr *
TelemetryObj_read_get_dtype (void)
{
  PyObject      *names = NULL, *formats = NULL;
  PyObject      *offsets = NULL, *spec = NULL;
  PyArray_Descr *out = NULL;
  if (TelemetryObj_read_dtype)
    {
      Py_INCREF (TelemetryObj_read_dtype);
      return TelemetryObj_read_dtype;
    }
  names = Py_BuildValue ("[ssss]", "n", "value", "probe", "flags");
  if (!names)
    goto done;
  formats = PyList_New (4);
  if (!formats)
    goto done;
  PyList_SET_ITEM (formats, 0, (PyObject *)PyArray_DescrFromType (NPY_UINT64));
  PyList_SET_ITEM (formats, 1, (PyObject *)PyArray_DescrFromType (NPY_FLOAT));
  PyList_SET_ITEM (formats, 2, (PyObject *)PyArray_DescrFromType (NPY_UINT16));
  PyList_SET_ITEM (formats, 3, (PyObject *)PyArray_DescrFromType (NPY_UINT16));
  offsets = Py_BuildValue ("[nnnn]", (Py_ssize_t)offsetof (dp_tlm_rec_t, n),
                           (Py_ssize_t)offsetof (dp_tlm_rec_t, value),
                           (Py_ssize_t)offsetof (dp_tlm_rec_t, probe),
                           (Py_ssize_t)offsetof (dp_tlm_rec_t, flags));
  if (!offsets)
    goto done;
  spec = Py_BuildValue ("{s:O,s:O,s:O,s:n}", "names", names, "formats",
                        formats, "offsets", offsets, "itemsize",
                        (Py_ssize_t)sizeof (dp_tlm_rec_t));
  if (!spec)
    goto done;
  if (!PyArray_DescrConverter (spec, &out))
    out = NULL;
done:
  Py_XDECREF (names);
  Py_XDECREF (formats);
  Py_XDECREF (offsets);
  Py_XDECREF (spec);
  if (out)
    {
      TelemetryObj_read_dtype = out;
      Py_INCREF (TelemetryObj_read_dtype);
    }
  return out;
}

static PyObject *
TelemetryObj_read (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|K", _kwlist, &n_raw))
    return NULL;
  size_t n     = (size_t)n_raw;
  size_t _need = dp_tlm_read_max_out (self->handle);
  size_t _cap  = dp_tlm_read_max_out (self->handle);
  (void)_need;
  npy_intp       _adim  = (npy_intp)_cap;
  PyArray_Descr *_descr = TelemetryObj_read_get_dtype ();
  if (!_descr)
    {
      return NULL;
    }
  PyObject *arr0 = PyArray_NewFromDescr (&PyArray_Type, _descr, 1, &_adim,
                                         NULL, NULL, 0, NULL);
  if (!arr0)
    {
      return NULL;
    }
  dp_tlm_rec_t *_d0   = (dp_tlm_rec_t *)PyArray_DATA ((PyArrayObject *)arr0);
  size_t        n_out = dp_tlm_read (self->handle, n, _d0, _cap);
  if ((size_t)n_out == _cap)
    {
      return arr0;
    }
  npy_intp     _odim = (npy_intp)n_out;
  PyArray_Dims _rs0  = { &_odim, 1 };
  PyObject *v0 = PyArray_Resize ((PyArrayObject *)arr0, &_rs0, 0, NPY_CORDER);
  if (!v0)
    {
      Py_DECREF (arr0);
      return NULL;
    }
  Py_DECREF (v0);
  return arr0;
}

static PyObject *
TelemetryObj_probe (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "name", "decim", NULL };
  const char   *name      = NULL;
  unsigned long decim_raw = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|k", _kwlist, &name,
                                    &decim_raw))
    return NULL;
  uint32_t decim = (uint32_t)decim_raw;
  int      _rc   = dp_tlm_probe (self->handle, name, decim);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "probe: name is NULL or too long, decim is 0, or the "
                    "table is full",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
TelemetryObj_probe_id (TelemetryObject *self, PyObject *args, PyObject *kwds)
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
  int _rc = dp_tlm_probe_id (self->handle, name);
  if (_rc < 0)
    {
      PyErr_Format (PyExc_KeyError, "%s (rc=%lld)", "no probe by that name",
                    (long long)_rc);
      return NULL;
    }
  return PyLong_FromLong ((long)_rc);
}

static PyObject *
TelemetryObj_set_decim (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char  *_kwlist[] = { "name", "decim", NULL };
  const char   *name      = NULL;
  unsigned long decim_raw = 0UL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sk", _kwlist, &name,
                                    &decim_raw))
    return NULL;
  uint32_t decim = (uint32_t)decim_raw;
  int      _rc   = dp_tlm_set_decim (self->handle, name, decim);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "set_decim failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
TelemetryObj_emit (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "id", "v", NULL };
  long         id_raw    = 0L;
  double       v         = 0.0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "ld", _kwlist, &id_raw, &v))
    return NULL;
  int32_t id  = (int32_t)id_raw;
  int     _rc = dp_tlm_emit_checked (self->handle, id, v);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "emit failed (rc=%d)", _rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
TelemetryObj_set_now (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", NULL };
  unsigned long long n_raw     = 0ULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "K", _kwlist, &n_raw))
    return NULL;
  uint64_t n = (uint64_t)n_raw;
  dp_tlm_set_now (self->handle, n);
  Py_RETURN_NONE;
}

static PyObject *
TelemetryObj_emitted (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "id", NULL };
  int          id        = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "i", _kwlist, &id))
    return NULL;
  uint64_t y = dp_tlm_emitted (self->handle, id);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyStructSequence_Field TelemetryObj_stats_fields[] = {
  { "dropped",
    "Records lost to ring overrun, monotonic over the context's lifetime." },
  { "emitted", "Records written, summed over every probe." },
  { "capacity", "Ring capacity in records." },
  { "probes", "Registered probes." },
  { NULL, NULL },
};
static PyStructSequence_Desc TelemetryObj_stats_desc
    = { "doppler.telemetry.TelemetryStats",
        "Context-wide telemetry counters, snapshotted together. Per-probe "
        "detail is probe_names + emitted(), which stay the SSOT for it.",
        TelemetryObj_stats_fields, 4 };
static PyTypeObject *TelemetryObj_stats_type = NULL;

static PyObject *
TelemetryObj_stats (TelemetryObject *self, PyObject *args)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  if (!TelemetryObj_stats_type)
    {
      TelemetryObj_stats_type
          = PyStructSequence_NewType (&TelemetryObj_stats_desc);
      if (!TelemetryObj_stats_type)
        return NULL;
    }
  dp_tlm_stats_t _r = dp_tlm_stats (self->handle);
  PyObject      *_o = PyStructSequence_New (TelemetryObj_stats_type);
  if (!_o)
    return NULL;
  PyStructSequence_SET_ITEM (
      _o, 0, PyLong_FromUnsignedLongLong ((unsigned long long)_r.dropped));
  PyStructSequence_SET_ITEM (
      _o, 1, PyLong_FromUnsignedLongLong ((unsigned long long)_r.emitted));
  PyStructSequence_SET_ITEM (
      _o, 2, PyLong_FromUnsignedLongLong ((unsigned long long)_r.capacity));
  PyStructSequence_SET_ITEM (
      _o, 3, PyLong_FromUnsignedLongLong ((unsigned long long)_r.probes));
  return _o;
}
static PyObject *
Telemetry_getprop_probe_names (TelemetryObject *self,
                               void            *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  size_t    _n = dp_tlm_probe_count (self->handle);
  PyObject *_c = PyDict_New ();
  if (!_c)
    return NULL;
  for (size_t _i = 0; _i < _n; _i++)
    {
      const char *_k = dp_tlm_probe_name (self->handle, _i);
      if (!_k)
        {
          PyErr_Format (
              PyExc_RuntimeError,
              "probe_names: dp_tlm_probe_name returned NULL at index %zu", _i);
          Py_DECREF (_c);
          return NULL;
        }
      PyObject *_v
          = PyLong_FromLong ((long)dp_tlm_probe_id_at (self->handle, _i));
      if (!_v)
        {
          Py_DECREF (_c);
          return NULL;
        }
      if (PyDict_SetItemString (_c, _k, _v) != 0)
        {
          Py_DECREF (_v);
          Py_DECREF (_c);
          return NULL;
        }
      Py_DECREF (_v);
    }
  return _c;
}
static PyObject *
Telemetry_getprop_capacity (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_capacity (self->handle)));
}
static PyObject *
Telemetry_getprop_dropped (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_dropped (self->handle)));
}
static PyObject *
Telemetry_getprop_probe_count (TelemetryObject *self,
                               void            *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_probe_count (self->handle)));
}
static PyObject *
Telemetry_getprop_avail (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_avail (self->handle)));
}
static PyObject *
Telemetry_getprop__capsule (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* Borrowed: NULL destructor, so the capsule never
     frees a pointer Telemetry still owns. */
  return PyCapsule_New ((void *)(self->handle), "doppler.telemetry.dp_tlm",
                        NULL);
}

static PyGetSetDef Telemetry_getset[]
    = { { "probe_names", (getter)Telemetry_getprop_probe_names, NULL,
          "Registered probes as `{name: id}`, in registration order. The "
          "inverse of `probe_id()`, and what a consumer needs to resolve a "
          "record's `probe` field back to a name.\n",
          NULL },
        { "capacity", (getter)Telemetry_getprop_capacity, NULL,
          "Ring capacity in records, after buffer.h rounds the requested size "
          "up to a power of two and the page minimum. Read this rather than "
          "assuming the constructor's argument was granted verbatim.\n",
          NULL },
        { "dropped", (getter)Telemetry_getprop_dropped, NULL,
          "Records lost to ring overrun over this context's lifetime, "
          "monotonic. Non-zero means a hole; prefer a `Capture`, which makes "
          "overrun arithmetically impossible rather than merely countable.\n",
          NULL },
        { "probe_count", (getter)Telemetry_getprop_probe_count, NULL,
          "Number of registered probes.\n", NULL },
        { "avail", (getter)Telemetry_getprop_avail, NULL,
          "Records currently readable, without consuming them. A lower bound "
          "while a producer is running -- the true count can only grow after "
          "the snapshot.\n",
          NULL },
        { "_capsule", (getter)Telemetry_getprop__capsule, NULL, " capsule.\n",
          NULL },
        { NULL } };

static PyObject *
TelemetryObj_destroy (TelemetryObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      dp_tlm_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
TelemetryObj_enter (TelemetryObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
TelemetryObj_exit (TelemetryObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      dp_tlm_destroy (self->handle);
      self->handle = NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef TelemetryObj_methods[] = {

  { "read", (PyCFunction)(void *)TelemetryObj_read,
    METH_VARARGS | METH_KEYWORDS,
    "read(n) -> ndarray\n"
    "\n"
    "Drains records into out. Non-blocking.\n"
    "\n"
    "Consumer side of the SPSC ring: safe to call from a different thread\n"
    "than the producer. Returns immediately with whatever is available\n"
    "(possibly 0) — never spins.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Records wanted; 0 means \"everything available\".\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Number of records copied out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> eid = tlm.probe(\"sync.e\")\n"
    ">>> for i in range(5):\n"
    "...     tlm.emit(eid, i / 10)\n"
    ">>> recs = tlm.read(2)          # take two\n"
    ">>> recs.shape, recs.dtype.names\n"
    "((2,), ('n', 'value', 'probe', 'flags'))\n"
    ">>> tlm.read().shape            # 0 means \"everything left\"\n"
    "(3,)\n"
    ">>> tlm.read().shape            # drained\n"
    "(0,)\n"
    "\n"
    "Fields\n"
    "------\n"
    "n : int\n"
    "    Caller-stamped sample index (dp_tlm_set_now).\n"
    "value : float\n"
    "    The scalar, narrowed to float.\n"
    "probe : int\n"
    "    Probe id; index into the registry.\n"
    "flags : int\n"
    "    Reserved; always 0.\n" },
  { "probe", (PyCFunction)(void *)TelemetryObj_probe,
    METH_VARARGS | METH_KEYWORDS,
    "probe(name, decim) -> int\n"
    "\n"
    "Registers (or re-registers) a named probe. Setup path, not hot.\n"
    "\n"
    "Idempotent by name: registering an existing name returns its id and\n"
    "updates decim (re-attach after a reset keeps ids stable). The\n"
    "decimation phase is primed so the FIRST event after registration emits.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Probe name, e.g. \"agc.gain_db\". Must be shorter than\n"
    "    DP_TLM_NAME_MAX.\n"
    "decim : int\n"
    "    Emit every decim-th event; >= 1.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Probe id (>= 0), or DP_ERR_INVALID on NULL/overlong name, decim ==\n"
    "    0, or a full table.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> tlm.probe(\"sync.e\", decim=4)\n"
    "0\n"
    ">>> tlm.probe(\"sync.e\")     # same name: same id, decim retuned\n"
    "0\n"
    ">>> tlm.probe_count\n"
    "1\n" },
  { "probe_id", (PyCFunction)(void *)TelemetryObj_probe_id,
    METH_VARARGS | METH_KEYWORDS,
    "probe_id(name) -> int\n"
    "\n"
    "Looks up a probe id by name; ::DP_ERR_INVALID if unknown.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Probe name as passed to dp_tlm_probe().\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Probe id (>= 0), or ::DP_ERR_INVALID if no such probe.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> _ = tlm.probe(\"agc.gain_db\")\n"
    ">>> tlm.probe_id(\"agc.gain_db\")\n"
    "0\n"
    ">>> tlm.probe_id(\"never.registered\")\n"
    "Traceback (most recent call last):\n"
    "KeyError: 'no probe by that name (rc=-4)'\n" },
  { "set_decim", (PyCFunction)(void *)TelemetryObj_set_decim,
    METH_VARARGS | METH_KEYWORDS,
    "set_decim(name, decim) -> int\n"
    "\n"
    "Retunes an EXISTING probe's decimation, by name.\n"
    "\n"
    "Distinct from dp_tlm_probe(), which registers on a miss: this refuses\n"
    "an unknown name rather than quietly creating a probe nothing emits to,\n"
    "which is what a typo in a retune call deserves.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "name : str\n"
    "    Name of an ALREADY registered probe.\n"
    "decim : int\n"
    "    Emit every decim-th event; >= 1.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> _ = tlm.probe(\"sync.e\", decim=1)\n"
    ">>> tlm.set_decim(\"sync.e\", 8)      # retune the existing probe\n"
    ">>> tlm.set_decim(\"typo.e\", 8)      # refused, not silently created\n"
    "Traceback (most recent call last):\n"
    "ValueError: set_decim failed (rc=-4)\n" },
  { "emit", (PyCFunction)(void *)TelemetryObj_emit,
    METH_VARARGS | METH_KEYWORDS,
    "emit(id, v) -> int\n"
    "\n"
    "Validating dp_tlm_emit(): refuses an id the registry never issued.\n"
    "\n"
    "The out-of-line twin of the inline hot-path emit, for callers whose id\n"
    "did not come from dp_tlm_probe() on this context — in practice, a\n"
    "language binding, where the id is whatever the caller passed.\n"
    "dp_tlm_emit() checks only the ARRAY bound (see its docs: checking\n"
    "n_probes there costs ~16% of the decimated path), so an in-range but\n"
    "unregistered id reaches it and emits a record against a probe nobody\n"
    "registered. Here that is an error.\n"
    "\n"
    "C hot loops keep calling dp_tlm_emit() directly and pay nothing for\n"
    "this.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "id : int\n"
    "    Probe id from dp_tlm_probe() on THIS context.\n"
    "v : float\n"
    "    The scalar, narrowed to float by the ring record.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"rx.snr_db\")\n"
    ">>> tlm.emit(pid, 12.5)\n"
    ">>> float(tlm.read()[0][\"value\"])\n"
    "12.5\n"
    "\n"
    "An id the registry never issued is refused, not written:\n"
    "\n"
    ">>> tlm.emit(pid + 1, 1.0)\n"
    "Traceback (most recent call last):\n"
    "ValueError: emit failed (rc=-4)\n" },
  { "set_now", (PyCFunction)(void *)TelemetryObj_set_now,
    METH_VARARGS | METH_KEYWORDS,
    "set_now(n) -> None\n"
    "\n"
    "Stamps the sample index carried by subsequent records, and — when a\n"
    "capture is open — closes out the block just finished.\n"
    "\n"
    "Call once per block from whoever owns the pipeline's sample clock\n"
    "(`dp_tlm_set_now (tlm, clk->n)`). NULL-safe so pipeline glue can call\n"
    "it unconditionally.\n"
    "\n"
    "Callers already place this at the top of the block loop, *before*\n"
    "stepping, which makes it exactly the boundary a lossless capture needs:\n"
    "delegating here drains the PREVIOUS block, leaving the ring empty as\n"
    "the next one starts. That is the invariant dp_tlm_block_bound() is\n"
    "sized against, so an existing `set_now / steps / read` loop becomes\n"
    "lossless by opening a capture and changing nothing else.\n"
    "\n"
    "With no capture open the behaviour is byte-identical to a bare\n"
    "assignment. The delegation is a cold branch on a per-block call, never\n"
    "a per-sample one, so it is nowhere near the hot loops dp_tlm_emit()\n"
    "cares about.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Sample index stamped into every subsequent record.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> tlm.set_now(1000)           # top of the block, before stepping\n"
    ">>> tlm.emit(pid, -3.5)\n"
    ">>> rec = tlm.read()[0]\n"
    ">>> int(rec[\"n\"]), float(rec[\"value\"])\n"
    "(1000, -3.5)\n" },
  { "emitted", (PyCFunction)(void *)TelemetryObj_emitted,
    METH_VARARGS | METH_KEYWORDS,
    "emitted(id) -> int\n"
    "\n"
    "Records written for probe id (post-decimation, post-drop).\n"
    "\n"
    "Reconcile against dp_tlm_dropped() to account for losses: what a probe\n"
    "emitted is what reached the ring, not what the call sites offered it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "id : int\n"
    "    Probe id from dp_tlm_probe().\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Records written for that probe, 0 for an unknown id.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> eid = tlm.probe(\"sync.e\", decim=2)\n"
    ">>> for i in range(4):\n"
    "...     tlm.emit(eid, i / 10)\n"
    ">>> tlm.emitted(eid)            # decim=2: half the events\n"
    "2\n"
    ">>> tlm.dropped\n"
    "0\n" },
  { "stats", (PyCFunction)TelemetryObj_stats, METH_VARARGS,
    "stats() -> TelemetryStats record (dropped, emitted, capacity, probes)." },
  { "destroy", (PyCFunction)TelemetryObj_destroy, METH_NOARGS,
    "Release the underlying C resources immediately.\n"
    "\n"
    "Ordinarily unnecessary: the resources are freed when the object is\n"
    "garbage-collected. Call this to release them at a definite point\n"
    "instead, or use the object as a context manager, which calls it on\n"
    "exit.\n"
    "\n"
    "Idempotent: calling it again on an already-released object does\n"
    "nothing. Every other method raises ``RuntimeError`` once it has run.\n" },
  { "__enter__", (PyCFunction)TelemetryObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a DpTlm be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "DpTlm\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)TelemetryObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the DpTlm.\n"
    "\n"
    "Equivalent to calling `destroy()`. Returns ``None``, so an exception\n"
    "raised inside the `with` body propagates normally; this never\n"
    "suppresses one.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "exc_type : object | None\n"
    "    Exception class, or None. Ignored.\n"
    "exc : object | None\n"
    "    Exception instance, or None. Ignored.\n"
    "tb : object | None\n"
    "    Traceback object, or None. Ignored.\n" },
  { NULL }
};

static PyTypeObject TelemetryObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "telemetry.Telemetry",
  .tp_basicsize                           = sizeof (TelemetryObject),
  .tp_dealloc                             = (destructor)TelemetryObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Creates a telemetry context with a ring of ring_records slots.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ring_records : int, default 16384\n"
    "    Requested ring capacity in records. MUST be a power of 2. Sub-page\n"
    "    requests are rounded up to the page minimum (buffer.h semantics) — "
    "read\n"
    "    the authoritative value back with dp_tlm_capacity().\n"
    "\n"
    "Examples\n"
    "--------\n"
    "Create with defaults:\n"
    "\n"
    ">>> from doppler import Telemetry\n"
    ">>> obj = Telemetry(ring_records=16384)\n",
  .tp_methods = TelemetryObj_methods,
  .tp_getset  = Telemetry_getset,
  .tp_new     = TelemetryObj_new,
  .tp_init    = (initproc)TelemetryObj_init,
};
