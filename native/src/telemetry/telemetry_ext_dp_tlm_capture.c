/*
 * telemetry_ext_dp_tlm_capture.c — MemoryCapture type for the telemetry
 * module.
 *
 * Included by telemetry_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only telemetry_ext.c is compiled.
 */
/* ======================================================== */
/* MemoryCaptureObject — wraps dp_tlm_capture_state_t *       */
/* ======================================================== */

#include "dp_tlm_capture/dp_tlm_capture_core.h"
#include "tlm_read_dict.h"

typedef struct
{
  PyObject_HEAD dp_tlm_capture_state_t *handle;
  PyObject                             *_tlm_owner;
  PyObject                             *_clock_owner;
} MemoryCaptureObject;

static void
MemoryCaptureObj_dealloc (MemoryCaptureObject *self)
{
  if (self->handle)
    {
      /* gh-541: tp_dealloc has no exception context — there
         is no caller to raise to, and an in-flight exception
         must not be clobbered. Discarding the status is the
         only correct choice here; the explicit teardown and
         __exit__ paths do report it. */
      (void)dp_tlm_capture_destroy (self->handle);
    }
  Py_XDECREF (self->_tlm_owner);
  Py_XDECREF (self->_clock_owner);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
MemoryCaptureObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  MemoryCaptureObject *self = (MemoryCaptureObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
MemoryCaptureObj_init (MemoryCaptureObject *self, PyObject *args,
                       PyObject *kwds)
{
  static char       *kwlist[] = { "tlm", "block_samples", "clock", NULL };
  PyObject          *tlm_obj  = NULL;
  unsigned long long block_samples_raw = 0ULL;
  PyObject          *clock_obj         = NULL;

  if (!PyArg_ParseTupleAndKeywords (args, kwds, "OKO", kwlist, &tlm_obj,
                                    &block_samples_raw, &clock_obj))
    return -1;
  dp_tlm_t *tlm = NULL;
  if (tlm_obj == Py_None || tlm_obj == NULL)
    {
      PyErr_SetString (
          PyExc_TypeError,
          "tlm is required and cannot be None;"
          " pass the doppler.telemetry.dp_tlm capsule or an object"
          " exposing it as ._capsule");
      return -1;
    }
  PyObject *tlm_cap = tlm_obj;
  Py_INCREF (tlm_cap);
  if (!PyCapsule_CheckExact (tlm_cap))
    {
      Py_DECREF (tlm_cap);
      tlm_cap = PyObject_GetAttrString (tlm_obj, "_capsule");
      if (!tlm_cap)
        {
          if (!PyErr_ExceptionMatches (PyExc_AttributeError))
            return -1;
          PyErr_Clear ();
          PyErr_Format (PyExc_TypeError,
                        "tlm must be the doppler.telemetry.dp_tlm capsule"
                        " or an object exposing it as ._capsule,"
                        " not %s",
                        Py_TYPE (tlm_obj)->tp_name);
          return -1;
        }
    }
  tlm = (dp_tlm_t *)PyCapsule_GetPointer (tlm_cap, "doppler.telemetry.dp_tlm");
  Py_DECREF (tlm_cap);
  if (!tlm)
    return -1;
  Py_INCREF (tlm_obj);
  Py_XSETREF (self->_tlm_owner, tlm_obj);
  size_t                   block_samples = (size_t)block_samples_raw;
  const dp_sample_clock_t *clock         = NULL;
  if (clock_obj != Py_None)
    {
      PyObject *clock_cap = clock_obj;
      Py_INCREF (clock_cap);
      if (!PyCapsule_CheckExact (clock_cap))
        {
          Py_DECREF (clock_cap);
          clock_cap = PyObject_GetAttrString (clock_obj, "_capsule");
          if (!clock_cap)
            {
              if (!PyErr_ExceptionMatches (PyExc_AttributeError))
                return -1;
              PyErr_Clear ();
              PyErr_Format (
                  PyExc_TypeError,
                  "clock must be the doppler.wfm.dp_sample_clock capsule"
                  " or an object exposing it as ._capsule,"
                  " not %s",
                  Py_TYPE (clock_obj)->tp_name);
              return -1;
            }
        }
      clock = (const dp_sample_clock_t *)PyCapsule_GetPointer (
          clock_cap, "doppler.wfm.dp_sample_clock");
      Py_DECREF (clock_cap);
      if (!clock)
        return -1;
    }
  Py_INCREF (clock_obj);
  Py_XSETREF (self->_clock_owner, clock_obj);
  self->handle = dp_tlm_capture_open_memory (tlm, block_samples, clock);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_ValueError,
                       "capture could not be opened: attach every probe "
                       "BEFORE opening (the ring is sized from the probe "
                       "table, so no probes means no bound), pass a non-zero "
                       "block_samples, use a context with no capture already "
                       "open, and — for the file flavour — a writable path");
      return -1;
    }
  return 0;
}

static PyObject *
MemoryCaptureObj_records_max_out (MemoryCaptureObject *self,
                                  PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromSize_t (dp_tlm_capture_read_max_out (self->handle));
}

static PyArray_Descr *MemoryCaptureObj_records_dtype = NULL;

/* The record's numpy dtype, built from the compiler's own layout:
   offsetof/sizeof, never numpy's packing rules, so a padded
   struct cannot silently read every row after the first from the
   wrong bytes. */
static PyArray_Descr *
MemoryCaptureObj_records_get_dtype (void)
{
  PyObject      *names = NULL, *formats = NULL;
  PyObject      *offsets = NULL, *spec = NULL;
  PyArray_Descr *out = NULL;
  if (MemoryCaptureObj_records_dtype)
    {
      Py_INCREF (MemoryCaptureObj_records_dtype);
      return MemoryCaptureObj_records_dtype;
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
      MemoryCaptureObj_records_dtype = out;
      Py_INCREF (MemoryCaptureObj_records_dtype);
    }
  return out;
}

static PyObject *
MemoryCaptureObj_records (MemoryCaptureObject *self, PyObject *args,
                          PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char       *_kwlist[] = { "n", "out", NULL };
  unsigned long long n_raw     = 0;
  PyObject          *out_obj   = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|KO", _kwlist, &n_raw,
                                    &out_obj))
    return NULL;
  size_t n = (size_t)n_raw;
  if (out_obj && out_obj != Py_None)
    {
      /* Require the exact record dtype AND C-contiguity. Compared with
       * EquivTypes against the generated descr: a structured array's type
       * num is NPY_VOID, so a scalar enum cannot say what layout is
       * wanted, and coercing to one would silently reinterpret the
       * caller's buffer. */
      {
        PyArray_Descr *_want = MemoryCaptureObj_records_get_dtype ();
        if (!_want)
          {
            return NULL;
          }
        if (!PyArray_Check (out_obj)
            || !PyArray_EquivTypes (PyArray_DESCR ((PyArrayObject *)out_obj),
                                    _want)
            || !PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)
            || !PyArray_ISWRITEABLE ((PyArrayObject *)out_obj))
          {
            PyErr_Format (
                PyExc_TypeError,
                "out must be a writable, C-contiguous ndarray of"
                " dtype %R, not %R",
                _want,
                PyArray_Check (out_obj)
                    ? (PyObject *)PyArray_DESCR ((PyArrayObject *)out_obj)
                    : Py_None);
            Py_DECREF (_want);
            return NULL;
          }
        Py_DECREF (_want);
      }
      PyArrayObject *out_arr = (PyArrayObject *)out_obj;
      Py_INCREF (out_arr);
      size_t _cap     = (size_t)PyArray_SIZE (out_arr);
      size_t _omax    = dp_tlm_capture_read_max_out (self->handle);
      size_t _min_cap = _omax > dp_tlm_capture_read_max_out (self->handle)
                            ? _omax
                            : (dp_tlm_capture_read_max_out (self->handle));
      if (_cap < _min_cap)
        {
          PyErr_Format (PyExc_ValueError, "out has %zu elements, need >= %zu",
                        _cap, _min_cap);
          Py_DECREF (out_arr);
          return NULL;
        }
      size_t n_out = dp_tlm_capture_read (
          self->handle, n, (dp_tlm_rec_t *)PyArray_DATA (out_arr), _cap);
      npy_intp       _odim   = (npy_intp)n_out;
      PyArray_Descr *_vdescr = MemoryCaptureObj_records_get_dtype ();
      if (!_vdescr)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyObject *_oview
          = PyArray_NewFromDescr (&PyArray_Type, _vdescr, 1, &_odim, NULL,
                                  PyArray_DATA (out_arr), 0, NULL);
      if (!_oview)
        {
          Py_DECREF (out_arr);
          return NULL;
        }
      PyArray_SetBaseObject ((PyArrayObject *)_oview, (PyObject *)out_arr);
      return _oview;
    }
  size_t _need = dp_tlm_capture_read_max_out (self->handle);
  size_t _cap  = dp_tlm_capture_read_max_out (self->handle);
  if (!_cap || _cap < _need)
    _cap = _need;
  npy_intp       _adim  = (npy_intp)_cap;
  PyArray_Descr *_descr = MemoryCaptureObj_records_get_dtype ();
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
  size_t        n_out = dp_tlm_capture_read (self->handle, n, _d0, _cap);
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
MemoryCaptureObj_block (MemoryCaptureObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_tlm_capture_block (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)", "block failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
MemoryCaptureObj_close (MemoryCaptureObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = dp_tlm_capture_close (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "the capture has a hole: records were dropped, which the "
                    "block bound makes impossible unless a step ran longer "
                    "than block_samples or no boundary was reached at all — "
                    "see Capture.dropped",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}
static PyObject *
MemoryCapture_getprop_count (MemoryCaptureObject *self,
                             void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_capture_count (self->handle)));
}
static PyObject *
MemoryCapture_getprop_dropped (MemoryCaptureObject *self,
                               void                *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  return PyLong_FromUnsignedLongLong (
      (unsigned long long)(dp_tlm_capture_dropped (self->handle)));
}

static PyGetSetDef MemoryCapture_getset[]
    = { { "count", (getter)MemoryCapture_getprop_count, NULL,
          "Records captured so far, across memory and file alike.\n", NULL },
        { "dropped", (getter)MemoryCapture_getprop_dropped, NULL,
          "Records the ring dropped during THIS capture (latched at open "
          "against the context's monotonic counter). Non-zero means a hole.\n",
          NULL },
        { NULL } };

static PyObject *
MemoryCaptureObj_destroy (MemoryCaptureObject *self,
                          PyObject            *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc = dp_tlm_capture_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_ValueError,
                           "the capture has a hole: records were dropped, "
                           "which the block bound makes impossible unless a "
                           "step ran longer than block_samples or no "
                           "boundary was reached at all — see "
                           "Capture.dropped");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
MemoryCaptureObj_enter (MemoryCaptureObject *self,
                        PyObject            *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
MemoryCaptureObj_exit (MemoryCaptureObject *self, PyObject *args)
{
  (void)args;
  if (!self->handle)
    Py_RETURN_NONE;
  /* gh-805 §H: the handle deliberately SURVIVES this call —
     finalize is not free, and the captured results only become
     valid once it has run. The free stays in tp_dealloc. */
  int _rc = dp_tlm_capture_close (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "the capture has a hole: records were dropped, which the "
                    "block bound makes impossible unless a step ran longer "
                    "than block_samples or no boundary was reached at all — "
                    "see Capture.dropped",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

/* Hand-written, sharing tlm_read_dict.h with Telemetry.read_dict(); only the
   record SOURCE differs. Here it is the capture's own accumulator, borrowed
   and only read — every array handed back is a fresh numpy allocation — and
   unlike the ring drain this does NOT consume, exactly as records() does not.
 */
static PyObject *
MemoryCaptureObj_read_dict (MemoryCaptureObject *self, PyObject *args,
                            PyObject *kwds)
{
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

static PyMethodDef MemoryCaptureObj_methods[] = {

  { "read_dict", (PyCFunction)(void *)MemoryCaptureObj_read_dict,
    METH_VARARGS | METH_KEYWORDS,
    "read_dict(n=0, index=False) -> dict\n"
    "\n"
    "The captured records, grouped by probe name.\n"
    "\n"
    "records() hands back one structured array carrying every probe\n"
    "interleaved; this splits it, so a consumer never writes the\n"
    "`recs[recs[\"probe\"] == id][\"value\"]` filter or the id-to-name\n"
    "inversion by hand. Every REGISTERED probe gets a key, including one\n"
    "that captured nothing, so the key set is stable.\n"
    "\n"
    "Like records(), this does NOT consume — call it as often as you like.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int, optional\n"
    "    Records to take from the front; 0 (the default) means all.\n"
    "index : bool, optional\n"
    "    When True each value is ``(n, values)`` — the sample indices\n"
    "    alongside the values, so a real time axis is ``n / fs``.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "dict\n"
    "    ``{probe_name: values}``, or ``{probe_name: (n, values)}`` when\n"
    "    `index` is True.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import MemoryCapture, Telemetry\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> eid = tlm.probe(\"sync.e\")\n"
    ">>> with MemoryCapture(tlm, 64, None) as cap:\n"
    "...     tlm.set_now(0)\n"
    "...     tlm.emit(eid, 0.25)\n"
    "...     tlm.set_now(64)\n"
    ">>> n, v = cap.read_dict(index=True)[\"sync.e\"]\n"
    ">>> v\n"
    "array([0.25], dtype=float32)\n"
    ">>> n\n"
    "array([0], dtype=uint64)\n" },

  { "records", (PyCFunction)(void *)MemoryCaptureObj_records,
    METH_VARARGS | METH_KEYWORDS,
    "records(n, out) -> ndarray\n"
    "\n"
    "Copies accumulated records out. Memory mode only.\n"
    "\n"
    "The copying twin of dp_tlm_capture_records(): same records, same order,\n"
    "but into caller memory rather than a borrowed pointer. Both exist\n"
    "because they serve opposite callers — a C consumer wants the zero-copy\n"
    "view, and a binding must not hand out a pointer the capture can free\n"
    "underneath it.\n"
    "\n"
    "Deliberately the same shape as dp_tlm_read(), so the two drains bind\n"
    "identically and neither needs a second convention invented for it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "n : int\n"
    "    Records wanted; 0 means \"everything accumulated\".\n"
    "out : NDArray[Any] | None\n"
    "    Destination.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Number of records copied out.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> for blk in range(4):\n"
    "...     tlm.set_now(blk * 256)\n"
    "...     tlm.emit(pid, float(blk))\n"
    ">>> cap.close()\n"
    ">>> [float(v) for v in cap.records()[\"value\"]]\n"
    "[0.0, 1.0, 2.0, 3.0]\n"
    ">>> cap.records(2).shape             # 0 (the default) means \"all\"\n"
    "(2,)\n"
    "\n"
    "Fields\n"
    "------\n"
    "n : int\n"
    "    Caller-stamped sample index (Telemetry.set_now).\n"
    "value : float\n"
    "    The scalar, narrowed to float.\n"
    "probe : int\n"
    "    Probe id; index into the registry.\n"
    "flags : int\n"
    "    Reserved; always 0.\n" },
  { "records_max_out", (PyCFunction)MemoryCaptureObj_records_max_out,
    METH_NOARGS,
    "records_max_out() -> int\n"
    "\n"
    "Upper bound on what dp_tlm_capture_read() can return right now.\n"
    "\n"
    "The accumulated count: a caller sizing a destination cannot know its\n"
    "own request will be smaller, and the generated binding allocates this\n"
    "much, reads, then resizes to what actually came back.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "block", (PyCFunction)MemoryCaptureObj_block, METH_NOARGS,
    "block() -> None\n"
    "\n"
    "Block boundary: drains the ring to empty.\n"
    "\n"
    "Grows the ring first if probes appeared since the last boundary, which\n"
    "is safe precisely here — the ring is about to be emptied and the\n"
    "producer is between blocks. Then copies everything available into the\n"
    "active staging buffer, handing it to the sink and swapping when it can\n"
    "no longer hold another block.\n"
    "\n"
    "**May block** in file mode, if the writer still holds the other buffer.\n"
    "That wait is the backpressure that keeps the capture lossless; it\n"
    "happens at the boundary, never inside the DSP loop.\n"
    "\n"
    "Usually reached through dp_tlm_set_now() rather than called directly.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``block failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> tlm.emit(pid, 1.5)\n"
    "\n"
    "An explicit boundary; set_now() reaches this for you:\n"
    "\n"
    ">>> cap.block()\n"
    ">>> cap.count\n"
    "1\n" },
  { "close", (PyCFunction)MemoryCaptureObj_close, METH_NOARGS,
    "close() -> None\n"
    "\n"
    "Final boundary, then flush, join, and write the sidecar.\n"
    "\n"
    "Sweeps the tail the last block left behind, drains the staging buffers,\n"
    "joins the writer thread, closes the file and writes `<path>-meta`.\n"
    "Idempotent: a second call is a no-op returning the first call's\n"
    "verdict.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``the capture has a hole: records were dropped, which the block\n"
    "    bound makes impossible unless a step ran longer than block_samples\n"
    "    or no boundary was reached at all — see Capture.dropped``, with the\n"
    "    return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> for blk in range(4):\n"
    "...     tlm.set_now(blk * 256)\n"
    "...     tlm.emit(pid, float(blk))\n"
    ">>> cap.close()          # silent: the block contract was honoured\n"
    ">>> cap.close()          # idempotent, same verdict\n"
    "\n"
    "Breaking the contract -- here, never reaching a boundary at all -- is "
    "the\n"
    "one way to lose a record, and it is reported rather than absorbed:\n"
    "\n"
    ">>> tlm2 = Telemetry(1 << 12)\n"
    ">>> p2 = tlm2.probe(\"x\")\n"
    ">>> bad = MemoryCapture(tlm2, 8, SampleClock(1e6))\n"
    ">>> for i in range(20000):\n"
    "...     tlm2.emit(p2, float(i))\n"
    ">>> bad.close()  # doctest: +ELLIPSIS\n"
    "Traceback (most recent call last):\n"
    "ValueError: the capture has a hole: ...\n" },
  { "destroy", (PyCFunction)MemoryCaptureObj_destroy, METH_NOARGS,
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
    "ValueError\n"
    "    If the C destructor reports failure. Raised from an explicit call\n"
    "    and from ``__exit__`` alike, so a failing teardown propagates out\n"
    "    of a ``with`` block (gh-541).\n" },
  { "__enter__", (PyCFunction)MemoryCaptureObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a MemoryCapture be used in a `with` statement so its C resources\n"
    "are finalized deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "MemoryCapture\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)MemoryCaptureObj_exit, METH_VARARGS,
    "Exit a context manager, finalizing the MemoryCapture.\n"
    "\n"
    "Equivalent to calling `close()`. The MemoryCapture is **not** released\n"
    "here: it stays usable, which is what makes results gathered during the\n"
    "`with` body readable after it. The memory is freed when the object is\n"
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
    "ValueError\n"
    "    If ``close()`` reports failure. ``__exit__`` calls it and raises\n"
    "    what it raises, so a failed finalize propagates out of the ``with``\n"
    "    block (gh-805 §H).\n" },
  { NULL }
};

static PyTypeObject MemoryCaptureObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "telemetry.MemoryCapture",
  .tp_basicsize                           = sizeof (MemoryCaptureObject),
  .tp_dealloc = (destructor)MemoryCaptureObj_dealloc,
  .tp_flags   = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Opens a capture that accumulates in memory instead of a file.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "tlm : Any\n"
    "    Telemetry context to capture. Must outlive the capture.\n"
    "block_samples : int\n"
    "    The LARGEST number of input samples processed between two boundaries "
    "—\n"
    "    the step of your own block loop, not a buffer size to tune.\n"
    "    Over-stating it costs only memory; under-stating it is the one way "
    "to\n"
    "    lose a record, and close() reports it.\n"
    "clock : Any\n"
    "    The pipeline's sample clock, borrowed for the sidecar's time base. "
    "Read\n"
    "    at close(), so later track() corrections are picked up. Must "
    "outlive\n"
    "    the capture. Pass None to state that there is no time base: the "
    "sidecar\n"
    "    then omits the rate and epoch keys rather than fabricating them into "
    "a\n"
    "    file that outlives the process. The argument itself is not omittable "
    "—\n"
    "    say None deliberately.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If construction fails. The exception message is ``capture could not "
    "be\n"
    "    opened: attach every probe BEFORE opening (the ring is sized from "
    "the\n"
    "    probe table, so no probes means no bound), pass a non-zero\n"
    "    block_samples, use a context with no capture already open, and — "
    "for\n"
    "    the file flavour — a writable path``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.telemetry import Telemetry, MemoryCapture\n"
    ">>> from doppler.wfm import SampleClock\n"
    ">>> tlm = Telemetry(1 << 12)\n"
    ">>> pid = tlm.probe(\"agc.gain_db\")   # probes FIRST: they set the "
    "bound\n"
    ">>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))\n"
    ">>> for blk in range(4):\n"
    "...     tlm.set_now(blk * 256)       # drains the block just finished\n"
    "...     tlm.emit(pid, float(blk))\n"
    ">>> cap.close()                      # raises if anything was lost\n"
    ">>> [float(v) for v in cap.records()[\"value\"]]\n"
    "[0.0, 1.0, 2.0, 3.0]\n"
    ">>> cap.dropped\n"
    "0\n",
  .tp_methods = MemoryCaptureObj_methods,
  .tp_getset  = MemoryCapture_getset,
  .tp_new     = MemoryCaptureObj_new,
  .tp_init    = (initproc)MemoryCaptureObj_init,
};
