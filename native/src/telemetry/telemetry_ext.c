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
