/*
 * telemetry_ext.c — Python C extension for telemetry/telemetry.h
 *
 * Exposes one type:
 *   doppler.telemetry.Telemetry — a dp_tlm_t context: probe registry +
 *   lock-free SPSC record ring.
 *
 * read() returns a NumPy structured array (one row per record, copied out
 * of the ring):
 *   dtype: [("n", "<u8"), ("value", "<f4"), ("probe", "<u2"),
 *           ("flags", "<u2")]  — 16 bytes/row, the exact dp_tlm_rec_t
 *   layout, so the drain is a single memcpy.
 *
 * Thread safety
 * -------------
 * The ring is single-producer / single-consumer.  Everything that emits
 * (attached C objects stepping, or emit()/set_now() from Python) is the
 * producer side and must stay on one thread; read()/dropped is the
 * consumer side and may run on a different thread.  probe() registration
 * must complete before the producer starts.
 *
 * The underlying dp_tlm_t* is exposed as a PyCapsule (name
 * "doppler.telemetry.dp_tlm") via the `_capsule` property, so instrumented
 * objects' set_telemetry bindings can attach to this context.
 */

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <numpy/arrayobject.h>

#include "telemetry/telemetry.h"

#define TLM_CAPSULE_NAME "doppler.telemetry.dp_tlm"

/* Shared structured dtype for read(); built once at module init. */
static PyArray_Descr *tlm_rec_descr = NULL;

typedef struct
{
  PyObject_HEAD dp_tlm_t *tlm; /* NULL after destroy() */
} TelemetryObject;

static PyObject *
Telemetry_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  TelemetryObject *self = (TelemetryObject *)type->tp_alloc (type, 0);
  if (self)
    self->tlm = NULL;
  return (PyObject *)self;
}

static int
Telemetry_init (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "ring_records", NULL };
  Py_ssize_t ring_records = 1 << 14;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|n", kwlist, &ring_records))
    return -1;
  if (ring_records <= 0)
    {
      PyErr_SetString (PyExc_ValueError, "ring_records must be positive");
      return -1;
    }
  self->tlm = dp_tlm_create ((size_t)ring_records);
  if (!self->tlm)
    {
      PyErr_SetString (PyExc_MemoryError,
                       "dp_tlm_create failed — ring_records must be a power "
                       "of 2 (sub-page sizes are rounded up to one page)");
      return -1;
    }
  return 0;
}

static void
Telemetry_dealloc (TelemetryObject *self)
{
  if (self->tlm)
    {
      dp_tlm_destroy (self->tlm);
      self->tlm = NULL;
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

/* Every method except destroy() opens with this: a destroyed context must
 * raise, not crash. */
static int
tlm_alive (TelemetryObject *self)
{
  if (!self->tlm)
    {
      PyErr_SetString (PyExc_RuntimeError, "Telemetry context destroyed");
      return 0;
    }
  return 1;
}

static PyObject *
Telemetry_probe (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "name", "decim", NULL };
  const char *name;
  unsigned int decim = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|I", kwlist, &name, &decim))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  int id = dp_tlm_probe (self->tlm, name, (uint32_t)decim);
  if (id < 0)
    {
      PyErr_Format (PyExc_ValueError,
                    "dp_tlm_probe(%s) failed — name too long (max %d), "
                    "decim == 0, or probe table full (max %d)",
                    name, DP_TLM_NAME_MAX - 1, DP_TLM_MAX_PROBES);
      return NULL;
    }
  return PyLong_FromLong (id);
}

static PyObject *
Telemetry_probe_id (TelemetryObject *self, PyObject *args)
{
  const char *name;
  if (!PyArg_ParseTuple (args, "s", &name))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  int id = dp_tlm_lookup (self->tlm, name);
  if (id < 0)
    {
      PyErr_SetString (PyExc_KeyError, name);
      return NULL;
    }
  return PyLong_FromLong (id);
}

static PyObject *
Telemetry_probe_names (TelemetryObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!tlm_alive (self))
    return NULL;
  PyObject *d = PyDict_New ();
  if (!d)
    return NULL;
  size_t n = dp_tlm_probe_count (self->tlm);
  for (size_t i = 0; i < n; i++)
    {
      PyObject *id = PyLong_FromSize_t (i);
      if (!id
          || PyDict_SetItemString (d, dp_tlm_probe_name (self->tlm, (int)i),
                                   id)
                 < 0)
        {
          Py_XDECREF (id);
          Py_DECREF (d);
          return NULL;
        }
      Py_DECREF (id);
    }
  return d;
}

static PyObject *
Telemetry_emit (TelemetryObject *self, PyObject *args)
{
  int    id;
  double value;
  if (!PyArg_ParseTuple (args, "id", &id, &value))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  if (id < 0 || (size_t)id >= dp_tlm_probe_count (self->tlm))
    {
      PyErr_Format (PyExc_ValueError, "unknown probe id %d", id);
      return NULL;
    }
  dp_tlm_emit (self->tlm, id, value);
  Py_RETURN_NONE;
}

static PyObject *
Telemetry_set_now (TelemetryObject *self, PyObject *args)
{
  unsigned long long n;
  if (!PyArg_ParseTuple (args, "K", &n))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  dp_tlm_set_now (self->tlm, (uint64_t)n);
  Py_RETURN_NONE;
}

static PyObject *
Telemetry_read (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "max_records", NULL };
  Py_ssize_t max_records = -1; /* -1: everything available */
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|n", kwlist, &max_records))
    return NULL;
  if (!tlm_alive (self))
    return NULL;

  /* Snapshot the available count, then drain exactly that many: records
   * only ever grow on the producer side, so `avail` stays available and
   * the array is allocated exactly once. */
  dp_tlmr_t *ring  = self->tlm->ring;
  size_t     avail = DP_LOAD_ACQ (&ring->head) - DP_LOAD_RLX (&ring->tail);
  if (max_records >= 0 && avail > (size_t)max_records)
    avail = (size_t)max_records;

  npy_intp dims[1] = { (npy_intp)avail };
  Py_INCREF (tlm_rec_descr); /* SimpleNewFromDescr steals a reference */
  PyObject *arr = PyArray_SimpleNewFromDescr (1, dims, tlm_rec_descr);
  if (!arr)
    return NULL;
  if (avail)
    {
      size_t got = dp_tlm_read (self->tlm,
                                (dp_tlm_rec_t *)PyArray_DATA (
                                    (PyArrayObject *)arr),
                                avail);
      (void)got; /* == avail by the SPSC contract */
    }
  return arr;
}

/* Group one drained batch by probe: {name: values} — or {name: (n, values)}
   with index=True, which is what gives a plot a real x-axis.

   Every consumer of read() was rebuilding this by hand
   (`recs[recs["probe"] == tlm.probe_id(n)]["value"]` appeared in all three
   telemetry examples, plus the id->name inversion beside it), so it belongs
   here once. It reads the ring exactly like read() does — same single drain,
   same SPSC contract — and only the shaping differs.

   EVERY registered probe gets a key, including probes with nothing in this
   batch (an empty array). A caller draining block by block would otherwise
   get a dict whose shape changes every call, which is precisely what makes
   plotting loops fragile. */
static PyObject *
Telemetry_read_dict (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "index", "max_records", NULL };
  int          index     = 0;
  Py_ssize_t max_records = -1; /* -1: everything available */
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|pn", kwlist, &index,
                                    &max_records))
    return NULL;
  if (!tlm_alive (self))
    return NULL;

  dp_tlmr_t *ring  = self->tlm->ring;
  size_t     avail = DP_LOAD_ACQ (&ring->head) - DP_LOAD_RLX (&ring->tail);
  if (max_records >= 0 && avail > (size_t)max_records)
    avail = (size_t)max_records;

  size_t        n_probes = dp_tlm_probe_count (self->tlm);
  dp_tlm_rec_t *buf      = NULL;
  if (avail)
    {
      buf = (dp_tlm_rec_t *)malloc (avail * sizeof *buf);
      if (!buf)
        return PyErr_NoMemory ();
      avail = dp_tlm_read (self->tlm, buf, avail);
    }

  /* One counting pass, so each probe's arrays are allocated exactly once —
     no per-probe reallocation, and no second drain (the records are gone
     from the ring after the read above). */
  size_t *count = (size_t *)calloc (n_probes ? n_probes : 1, sizeof *count);
  if (!count)
    {
      free (buf);
      return PyErr_NoMemory ();
    }
  for (size_t i = 0; i < avail; i++)
    if (buf[i].probe < n_probes)
      count[buf[i].probe]++;

  PyObject *d = PyDict_New ();
  if (!d)
    goto fail;

  for (size_t p = 0; p < n_probes; p++)
    {
      npy_intp  dims[1] = { (npy_intp)count[p] };
      PyObject *vals    = PyArray_SimpleNew (1, dims, NPY_FLOAT32);
      PyObject *idxs    = NULL;
      if (!vals)
        goto fail;
      if (index)
        {
          idxs = PyArray_SimpleNew (1, dims, NPY_UINT64);
          if (!idxs)
            {
              Py_DECREF (vals);
              goto fail;
            }
        }
      float    *vp = (float *)PyArray_DATA ((PyArrayObject *)vals);
      uint64_t *np_
          = index ? (uint64_t *)PyArray_DATA ((PyArrayObject *)idxs) : NULL;
      size_t k = 0;
      for (size_t i = 0; i < avail; i++)
        if (buf[i].probe == p)
          {
            if (np_)
              np_[k] = buf[i].n;
            vp[k++] = buf[i].value;
          }

      PyObject *item = vals;
      if (index)
        {
          item = PyTuple_Pack (2, idxs, vals);
          Py_DECREF (idxs);
          Py_DECREF (vals);
          if (!item)
            goto fail;
        }
      int rc = PyDict_SetItemString (
          d, dp_tlm_probe_name (self->tlm, (int)p), item);
      Py_DECREF (item);
      if (rc < 0)
        goto fail;
    }
  free (count);
  free (buf);
  return d;

fail:
  Py_XDECREF (d);
  free (count);
  free (buf);
  return NULL;
}

/* Per-probe decimation, by name. The capability already existed -- probe()
   is idempotent by name and updates decim -- but only as a side effect of
   "registering" a probe that is already registered, which reads like a
   mistake at a call site. This is a name for it, so thinning one noisy
   series while its siblings stay at full rate is something you can say. */
static PyObject *
Telemetry_set_decim (TelemetryObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "name", "decim", NULL };
  const char  *name;
  unsigned int decim;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sI", kwlist, &name, &decim))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  if (dp_tlm_lookup (self->tlm, name) < 0)
    {
      PyErr_SetString (PyExc_KeyError, name);
      return NULL;
    }
  if (dp_tlm_probe (self->tlm, name, (uint32_t)decim) < 0)
    {
      PyErr_Format (PyExc_ValueError, "decim must be >= 1 (got %u)", decim);
      return NULL;
    }
  Py_RETURN_NONE;
}

/* Reconcile what the probes emitted against what the ring dropped -- the
   cross-check every capture needs and everyone was doing by hand. `dropped`
   is ring-wide (a dropped record no longer knows which probe it came from),
   so it is reported once, not per probe. */
static PyObject *
Telemetry_stats (TelemetryObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!tlm_alive (self))
    return NULL;
  PyObject *per = PyDict_New ();
  if (!per)
    return NULL;
  size_t n = dp_tlm_probe_count (self->tlm);
  for (size_t i = 0; i < n; i++)
    {
      PyObject *v
          = PyLong_FromUnsignedLongLong (dp_tlm_emitted (self->tlm, (int)i));
      if (!v
          || PyDict_SetItemString (
                 per, dp_tlm_probe_name (self->tlm, (int)i), v)
                 < 0)
        {
          Py_XDECREF (v);
          Py_DECREF (per);
          return NULL;
        }
      Py_DECREF (v);
    }
  PyObject *d = Py_BuildValue (
      "{s:N,s:K,s:n,s:n}", "emitted", per, "dropped",
      (unsigned long long)dp_tlm_dropped (self->tlm), "capacity",
      (Py_ssize_t)dp_tlm_capacity (self->tlm), "probes", (Py_ssize_t)n);
  if (!d)
    Py_DECREF (per);
  return d;
}

static PyObject *
Telemetry_emitted (TelemetryObject *self, PyObject *args)
{
  int id;
  if (!PyArg_ParseTuple (args, "i", &id))
    return NULL;
  if (!tlm_alive (self))
    return NULL;
  return PyLong_FromUnsignedLongLong (dp_tlm_emitted (self->tlm, id));
}

static PyObject *
Telemetry_destroy (TelemetryObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->tlm)
    {
      dp_tlm_destroy (self->tlm);
      self->tlm = NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Telemetry_capacity (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!tlm_alive (self))
    return NULL;
  return PyLong_FromSize_t (dp_tlm_capacity (self->tlm));
}

static PyObject *
Telemetry_dropped (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!tlm_alive (self))
    return NULL;
  return PyLong_FromUnsignedLongLong (dp_tlm_dropped (self->tlm));
}

static PyObject *
Telemetry_probe_count (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!tlm_alive (self))
    return NULL;
  return PyLong_FromSize_t (dp_tlm_probe_count (self->tlm));
}

/* Non-owning capsule: the Telemetry object keeps ownership, attach glue
 * only borrows the pointer.  Attached objects must not outlive `self`. */
static PyObject *
Telemetry_capsule (TelemetryObject *self, void *Py_UNUSED (closure))
{
  if (!tlm_alive (self))
    return NULL;
  return PyCapsule_New (self->tlm, TLM_CAPSULE_NAME, NULL);
}

static PyGetSetDef Telemetry_getset[] = {
  { "capacity", (getter)Telemetry_capacity, NULL,
    "Authoritative ring capacity in records (post page rounding).", NULL },
  { "dropped", (getter)Telemetry_dropped, NULL,
    "Total records dropped on ring overrun (monotonic).", NULL },
  { "probe_count", (getter)Telemetry_probe_count, NULL,
    "Number of registered probes.", NULL },
  { "_capsule", (getter)Telemetry_capsule, NULL,
    "PyCapsule('" TLM_CAPSULE_NAME "') borrowing the dp_tlm_t* — the "
    "attach point for instrumented objects' set_telemetry().",
    NULL },
  { NULL },
};

static PyMethodDef Telemetry_methods[] = {
  { "probe", (PyCFunction)Telemetry_probe, METH_VARARGS | METH_KEYWORDS,
    "probe(name, decim=1) -> int\n\n"
    "Register (or re-register) a named probe; returns its id.\n"
    "Idempotent by name; decim=N emits every N-th event.  Setup path\n"
    "only — complete registration before the producer starts." },
  { "probe_id", (PyCFunction)Telemetry_probe_id, METH_VARARGS,
    "probe_id(name) -> int\n\n"
    "Look up a probe id by name.  Raises KeyError if unknown." },
  { "probe_names", (PyCFunction)Telemetry_probe_names, METH_NOARGS,
    "probe_names() -> dict[str, int]\n\n"
    "The full name -> id map for every registered probe." },
  { "emit", (PyCFunction)Telemetry_emit, METH_VARARGS,
    "emit(probe_id, value) -> None\n\n"
    "Record one scalar (producer side).  For Python-side events and\n"
    "tests; C objects emit directly from their hot loops." },
  { "set_now", (PyCFunction)Telemetry_set_now, METH_VARARGS,
    "set_now(n) -> None\n\n"
    "Stamp the sample index carried by subsequent records (producer\n"
    "side; once per block)." },
  { "read", (PyCFunction)Telemetry_read, METH_VARARGS | METH_KEYWORDS,
    "read(max_records=-1) -> np.ndarray\n\n"
    "Drain up to max_records (default: all available) into a structured\n"
    "array [('n','<u8'),('value','<f4'),('probe','<u2'),('flags','<u2')].\n"
    "Non-blocking; consumer side (may run on another thread)." },
  { "read_dict", (PyCFunction)Telemetry_read_dict,
    METH_VARARGS | METH_KEYWORDS,
    "read_dict(index=False, max_records=-1) -> dict\n\n"
    "Drain like read(), grouped by probe NAME instead of returning one\n"
    "structured array: {name: values} as float32, or {name: (n, values)}\n"
    "with index=True, where n is the stamped sample index — that is what\n"
    "gives a plot a real time axis (n / fs) instead of an ordinal.\n\n"
    "Every registered probe gets a key, including ones with nothing in\n"
    "this batch (an empty array), so the dict's shape does not change\n"
    "from call to call while draining block by block." },
  { "set_decim", (PyCFunction)Telemetry_set_decim,
    METH_VARARGS | METH_KEYWORDS,
    "set_decim(name, decim) -> None\n\n"
    "Emit every decim-th event for one probe, leaving its siblings at\n"
    "full rate.  Raises KeyError if the probe is not registered." },
  { "stats", (PyCFunction)Telemetry_stats, METH_NOARGS,
    "stats() -> dict\n\n"
    "{'emitted': {name: count}, 'dropped': int, 'capacity': int,\n"
    "'probes': int} — reconciles what each probe emitted against what\n"
    "the ring dropped.  `dropped` is ring-wide: a dropped record no\n"
    "longer knows which probe it came from." },
  { "emitted", (PyCFunction)Telemetry_emitted, METH_VARARGS,
    "emitted(probe_id) -> int\n\n"
    "Records written for this probe (post-decimation, post-drop)." },
  { "destroy", (PyCFunction)Telemetry_destroy, METH_NOARGS,
    "destroy() -> None\n\n"
    "Free the context now.  Detach any attached objects first; further\n"
    "method calls raise RuntimeError." },
  { NULL },
};

static PyTypeObject TelemetryType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "telemetry.Telemetry",
  .tp_basicsize = sizeof (TelemetryObject),
  .tp_dealloc = (destructor)Telemetry_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Telemetry(ring_records=16384)\n\n"
            "Scalar telemetry context: named probe registry + lock-free\n"
            "SPSC record ring (see docs/design/telemetry.md).\n"
            "ring_records must be a power of 2; a sub-page request is\n"
            "rounded up to one page, so read the real size from\n"
            "`.capacity`.",
  .tp_methods = Telemetry_methods,
  .tp_getset = Telemetry_getset,
  .tp_init = (initproc)Telemetry_init,
  .tp_new = Telemetry_new,
};

/* =====================================================================
 * Module
 * ===================================================================== */

static PyModuleDef telemetry_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "telemetry",
  .m_doc = "Doppler scalar telemetry bindings.\n\nType: Telemetry.",
  .m_size = -1,
};

PyMODINIT_FUNC
PyInit_telemetry (void)
{
  import_array ();

  /* Build the shared record dtype: 16 bytes packed, the exact
   * dp_tlm_rec_t layout, so read() drains with one memcpy. */
  PyObject *spec
      = Py_BuildValue ("[(ss)(ss)(ss)(ss)]", "n", "<u8", "value", "<f4",
                       "probe", "<u2", "flags", "<u2");
  if (!spec)
    return NULL;
  int ok = PyArray_DescrConverter (spec, &tlm_rec_descr);
  Py_DECREF (spec);
  if (!ok)
    return NULL;
  if ((size_t)PyDataType_ELSIZE (tlm_rec_descr) != sizeof (dp_tlm_rec_t))
    {
      PyErr_SetString (PyExc_SystemError,
                       "telemetry record dtype does not match "
                       "sizeof(dp_tlm_rec_t)");
      return NULL;
    }

  if (PyType_Ready (&TelemetryType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&telemetry_module);
  if (!m)
    return NULL;

  Py_INCREF (&TelemetryType);
  if (PyModule_AddObject (m, "Telemetry", (PyObject *)&TelemetryType) < 0)
    {
      Py_DECREF (&TelemetryType);
      Py_DECREF (m);
      return NULL;
    }

  return m;
}
