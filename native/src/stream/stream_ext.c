/*
 * stream_ext.c — Python C extension wrapping the doppler streaming C library.
 *
 * Thin wrapper around libdoppler's dp_pub_t/dp_sub_t/dp_push_t/dp_pull_t/
 * dp_req_t/dp_rep_t API.  All socket creation, header construction,
 * send/recv logic, and protocol handling lives in the C library.
 *
 * Zero-copy recv: dp_msg_t owns the receive buffer; we wrap it in dpMsgObject
 * which is set as the NumPy array's base object.  When the array is GC'd,
 * dpMsgObject.dealloc calls dp_msg_free().
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <signal.h>

#include "dp_tlm/dp_tlm_core.h" /* dp_tlm_rec_t (TLM16 frames) */
#include "stream/stream.h"

/* =========================================================================
 * dpMsgObject — prevents premature dp_msg_free via NumPy base object
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_msg_t *msg;
} dpMsgObject;

static void
dpMsg_dealloc (dpMsgObject *self)
{
  if (self->msg)
    {
      dp_msg_free (self->msg);
      self->msg = NULL;
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyTypeObject dpMsgType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream._dpMsg",
  .tp_basicsize                           = sizeof (dpMsgObject),
  .tp_dealloc                             = (destructor)dpMsg_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Internal dp_msg_t wrapper for zero-copy recv",
};

/* Structured dtype for TLM16 telemetry frames — the exact dp_tlm_rec_t
 * layout, matching doppler.telemetry.Telemetry.read(). Built once at
 * module init. */
static PyArray_Descr *tlm16_descr = NULL;

/* =========================================================================
 * Shared helpers
 * ========================================================================= */

/* Build a (samples_array, header_dict) tuple from a dp_msg_t + dp_header_t.
 * Steals the msg reference (caller must not free it). */
static PyObject *
build_recv_result (dp_msg_t *msg, const dp_header_t *hdr)
{
  dpMsgObject *msg_obj = PyObject_New (dpMsgObject, &dpMsgType);
  if (!msg_obj)
    {
      dp_msg_free (msg);
      return NULL;
    }
  msg_obj->msg = msg;

  npy_intp         dims[1];
  int              typenum;
  dp_frame_kind_t  kind = dp_msg_kind (msg);
  dp_sample_type_t st   = dp_msg_sample_type (msg);
  const int        tlm  = (kind == DP_KIND_TLM);

  if (tlm)
    {
      /* telemetry records: structured rows (n u8 | value f4 | probe u2 |
       * flags u2), decoded below via the shared descr instead of a plain
       * typenum. */
      dims[0] = (npy_intp)dp_msg_num_samples (msg);
      typenum = -1;
    }
  else if (st == CI32)
    {
      dims[0] = (npy_intp)(dp_msg_num_samples (msg) * 2); /* interleaved I/Q */
      typenum = NPY_INT32;
    }
  else if (st == CF64)
    {
      dims[0] = (npy_intp)dp_msg_num_samples (msg);
      typenum = NPY_COMPLEX128;
    }
  else if (st == CI8)
    {
      dims[0] = (npy_intp)(dp_msg_num_samples (msg) * 2); /* interleaved I/Q */
      typenum = NPY_INT8;
    }
  else if (st == CI16)
    {
      dims[0] = (npy_intp)(dp_msg_num_samples (msg) * 2); /* interleaved I/Q */
      typenum = NPY_INT16;
    }
  else if (st == CF32)
    {
      dims[0] = (npy_intp)dp_msg_num_samples (msg);
      typenum = NPY_COMPLEX64;
    }
  else
    {
      Py_DECREF (msg_obj);
      PyErr_Format (PyExc_ValueError, "Unknown wire format: 0x%04X",
                    (unsigned)st);
      return NULL;
    }

  PyObject *arr;
  if (tlm)
    {
      Py_INCREF (tlm16_descr); /* NewFromDescr steals a reference */
      arr = PyArray_NewFromDescr (&PyArray_Type, tlm16_descr, 1, dims, NULL,
                                  dp_msg_data (msg), NPY_ARRAY_DEFAULT, NULL);
    }
  else
    arr = PyArray_SimpleNewFromData (1, dims, typenum, dp_msg_data (msg));
  if (!arr)
    {
      Py_DECREF (msg_obj);
      return NULL;
    }
  PyArray_SetBaseObject ((PyArrayObject *)arr, (PyObject *)msg_obj);

  PyObject *header = PyDict_New ();
  if (!header)
    {
      Py_DECREF (arr);
      return NULL;
    }

  PyDict_SetItemString (header, "sequence",
                        PyLong_FromUnsignedLongLong (hdr->sequence));
  PyDict_SetItemString (header, "timestamp_ns",
                        PyLong_FromUnsignedLongLong (hdr->timestamp_ns));
  PyDict_SetItemString (header, "sample_rate",
                        PyFloat_FromDouble (hdr->sample_rate));
  PyDict_SetItemString (header, "center_freq",
                        PyFloat_FromDouble (hdr->center_freq));
  PyDict_SetItemString (header, "num_samples",
                        PyLong_FromUnsignedLongLong (hdr->num_samples));
  PyDict_SetItemString (header, "format", PyLong_FromLong (hdr->format));
  PyDict_SetItemString (header, "kind", PyLong_FromLong (hdr->kind));
  PyDict_SetItemString (
      header, "data_rep",
      PyUnicode_FromStringAndSize (hdr->data_rep, sizeof hdr->data_rep));
  PyDict_SetItemString (header, "flags", PyLong_FromLong (hdr->flags));
  PyDict_SetItemString (header, "payload_bytes",
                        PyLong_FromUnsignedLong (hdr->payload_bytes));
  PyDict_SetItemString (header, "version", PyLong_FromLong (hdr->version));

  return Py_BuildValue ("(NN)", arr, header);
}

/* Generic signal-frame send (shared by all sender socket types). */
typedef int (*send_ci32_fn) (void *, const int32_t *, size_t, double, double);
typedef int (*send_cf64_fn) (void *, const double _Complex *, size_t, double,
                             double);
typedef int (*send_ci8_fn) (void *, const int8_t *, size_t, double, double);
typedef int (*send_ci16_fn) (void *, const int16_t *, size_t, double, double);
typedef int (*send_cf32_fn) (void *, const float _Complex *, size_t, double,
                             double);

static PyObject *
do_send (void *ctx, int sample_type, send_ci32_fn fn_ci32,
         send_cf64_fn fn_cf64, send_ci8_fn fn_ci8, send_ci16_fn fn_ci16,
         send_cf32_fn fn_cf32, PyObject *args, PyObject *kwds)
{
  PyArrayObject *arr;
  double         sample_rate      = 0.0;
  double         center_freq      = 0.0;
  PyObject      *timestamp_ns_obj = NULL;

  static char *kwlist[]
      = { "samples", "sample_rate", "center_freq", "timestamp_ns", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O!|ddO", kwlist,
                                    &PyArray_Type, &arr, &sample_rate,
                                    &center_freq, &timestamp_ns_obj))
    return NULL;

  if (timestamp_ns_obj && timestamp_ns_obj != Py_None)
    {
      unsigned long long ts = PyLong_AsUnsignedLongLong (timestamp_ns_obj);
      if (PyErr_Occurred ())
        return NULL;
      dp_ctx_set_timestamp_ns ((dp_pub_t *)ctx, (uint64_t)ts);
    }

  if (!PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError, "samples must be C-contiguous");
      return NULL;
    }

  int expected = (sample_type == CI32)   ? NPY_INT32
                 : (sample_type == CF64) ? NPY_COMPLEX128
                 : (sample_type == CI8)  ? NPY_INT8
                 : (sample_type == CI16) ? NPY_INT16
                                         : NPY_COMPLEX64; /* CF32 */
  if (PyArray_TYPE (arr) != expected)
    {
      PyErr_SetString (PyExc_TypeError, "samples dtype mismatch");
      return NULL;
    }

  npy_intp num_samples = PyArray_SIZE (arr);
  if (sample_type == CI32 || sample_type == CI8 || sample_type == CI16)
    num_samples /= 2; /* interleaved I/Q pairs */

  int   rc;
  void *data = PyArray_DATA (arr);

  Py_BEGIN_ALLOW_THREADS
    ;
    if (sample_type == CI32)
      rc = fn_ci32 (ctx, (const int32_t *)data, (size_t)num_samples,
                    sample_rate, center_freq);
    else if (sample_type == CF64)
      rc = fn_cf64 (ctx, (const double _Complex *)data, (size_t)num_samples,
                    sample_rate, center_freq);
    else if (sample_type == CI8)
      rc = fn_ci8 (ctx, (const int8_t *)data, (size_t)num_samples, sample_rate,
                   center_freq);
    else if (sample_type == CI16)
      rc = fn_ci16 (ctx, (const int16_t *)data, (size_t)num_samples,
                    sample_rate, center_freq);
    else /* CF32 */
      rc = fn_cf32 (ctx, (const float _Complex *)data, (size_t)num_samples,
                    sample_rate, center_freq);
  Py_END_ALLOW_THREADS;

  if (rc == DP_ERR_TOO_LARGE)
    {
      PyErr_Format (
          PyExc_ValueError,
          "%s: this frame does not fit in one message on the NATS "
          "work-queue (PUSH/PULL) tier; raise the broker max_payload "
          "or use PUB/SUB, which chunks",
          dp_strerror (rc));
      return NULL;
    }
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "send failed: %s", dp_strerror (rc));
      return NULL;
    }

  Py_RETURN_NONE;
}

/* Generic signal-frame recv (shared by all receiver socket types). */
typedef void (*set_timeout_fn) (void *, int);
typedef int (*recv_signal_fn) (void *, dp_msg_t **, dp_header_t *);

static PyObject *
do_recv (void *ctx, set_timeout_fn fn_timeout, recv_signal_fn fn_recv,
         PyObject *args, PyObject *kwds)
{
  int timeout_ms = -1;

  static char *kwlist[] = { "timeout_ms", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", kwlist, &timeout_ms))
    return NULL;

  if (timeout_ms >= 0)
    fn_timeout (ctx, timeout_ms);

  dp_msg_t   *msg = NULL;
  dp_header_t hdr;
  int         rc;

  Py_BEGIN_ALLOW_THREADS
    ;
    rc = fn_recv (ctx, &msg, &hdr);
  Py_END_ALLOW_THREADS;

  if (rc == DP_ERR_TIMEOUT)
    {
      PyErr_SetString (PyExc_TimeoutError, "recv timeout");
      return NULL;
    }
  if (rc == DP_ERR_INTERRUPTED)
    {
      /* KeyboardInterrupt rather than a doppler-specific exception: the
         interrupt exists so Ctrl+C works during a blocking recv, and a
         program's existing handling of Ctrl+C -- `try`, `finally`, a
         `with` block's cleanup -- applies unchanged. Anything else would
         make the caller learn a second word for the same event.

         PyErr_CheckSignals() FIRST, and once. Our C handler chains to
         CPython's, so a real Ctrl+C leaves a Python-level signal pending;
         raising our own exception here and letting that one fire too
         delivered KeyboardInterrupt TWICE -- the second landing inside
         the caller's `except KeyboardInterrupt:` cleanup. Measured, not
         reasoned. So: let CPython raise it if it has one to raise, and
         only invent one when nothing is pending (interrupt() called from
         another thread, where no signal was involved at all). */
      if (PyErr_CheckSignals () != 0)
        return NULL; /* CPython raised it; do not raise a second */
      PyErr_SetString (PyExc_KeyboardInterrupt, "interrupted");
      return NULL;
    }
  if (rc == DP_ERR_EOF)
    {
      /* EOFError, not RuntimeError: the sender finished, which is a state
         and the ordinary end of a loop. Python already has a word for it,
         and `except EOFError` is what a reader expects to write. */
      PyErr_SetString (PyExc_EOFError, "end of stream: the sender finished");
      return NULL;
    }
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "recv failed: %s", dp_strerror (rc));
      return NULL;
    }

  return build_recv_result (msg, &hdr);
}

/* =========================================================================
 * Publisher (NATS PUB)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_pub_t *ctx;
  int                     sample_type;
  int                     closed;
} PublisherObject;

static void
Publisher_dealloc (PublisherObject *self)
{
  if (!self->closed && self->ctx)
    dp_pub_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Publisher_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;
  int         sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  /* TLM16 is a frame KIND, not a sample format, and it arrives in the same
     argument slot because "what does this socket publish" is one question
     to a caller. The two vocabularies cannot collide: a BLUE code is two
     ASCII characters packed into 16 bits, so every one of them is >= 0x4200,
     while DP_KIND_TLM is 1. */
  const int tlm = (sample_type == DP_KIND_TLM);
  if (!tlm && !dp_sample_type_is_valid ((dp_sample_type_t)sample_type))
    {
      PyErr_SetString (PyExc_ValueError, "Invalid sample_type");
      return NULL;
    }

  PublisherObject *self = (PublisherObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = tlm ? dp_pub_create_tlm (endpoint)
                  : dp_pub_create (endpoint, (dp_sample_type_t)sample_type);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_pub_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->sample_type = sample_type;
  self->closed      = 0;
  return (PyObject *)self;
}

static PyObject *
Publisher_send (PublisherObject *self, PyObject *args, PyObject *kwds)
{
  if (self->sample_type == DP_KIND_TLM)
    {
      /* Telemetry frames: a structured array of 16-byte records (the
       * dtype Telemetry.read() returns) published verbatim. PUB-only —
       * do_send's typed sample dispatch doesn't apply. */
      PyArrayObject *arr;
      double         sample_rate      = 0.0;
      double         center_freq      = 0.0;
      PyObject      *timestamp_ns_obj = NULL;
      static char   *kwlist[]
          = { "samples", "sample_rate", "center_freq", "timestamp_ns", NULL };
      if (!PyArg_ParseTupleAndKeywords (args, kwds, "O!|ddO", kwlist,
                                        &PyArray_Type, &arr, &sample_rate,
                                        &center_freq, &timestamp_ns_obj))
        return NULL;
      if (timestamp_ns_obj && timestamp_ns_obj != Py_None)
        {
          unsigned long long ts = PyLong_AsUnsignedLongLong (timestamp_ns_obj);
          if (PyErr_Occurred ())
            return NULL;
          dp_ctx_set_timestamp_ns (self->ctx, (uint64_t)ts);
        }
      if (!PyArray_IS_C_CONTIGUOUS (arr)
          || PyArray_ITEMSIZE (arr) != (npy_intp)sizeof (dp_tlm_rec_t))
        {
          PyErr_SetString (PyExc_TypeError,
                           "samples must be a C-contiguous array of 16-byte "
                           "telemetry records (Telemetry.read() output)");
          return NULL;
        }
      size_t      n    = (size_t)PyArray_SIZE (arr);
      const void *data = PyArray_DATA (arr);
      int         rc;
      Py_BEGIN_ALLOW_THREADS
        ;
        rc = dp_pub_send_tlm16 (self->ctx, data, n, sample_rate, center_freq);
      Py_END_ALLOW_THREADS;
      if (rc != DP_OK)
        {
          PyErr_Format (PyExc_RuntimeError, "send failed: %s",
                        dp_strerror (rc));
          return NULL;
        }
      Py_RETURN_NONE;
    }
  return do_send (self->ctx, self->sample_type, (send_ci32_fn)dp_pub_send_ci32,
                  (send_cf64_fn)dp_pub_send_cf64, (send_ci8_fn)dp_pub_send_ci8,
                  (send_ci16_fn)dp_pub_send_ci16,
                  (send_cf32_fn)dp_pub_send_cf32, args, kwds);
}

static PyObject *
Publisher_close (PublisherObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_pub_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Publisher_enter (PublisherObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Publisher_exit (PublisherObject *self, PyObject *Py_UNUSED (args))
{
  return Publisher_close (self, NULL);
}

static PyObject *
Publisher_flush (PublisherObject *self, PyObject *args, PyObject *kwds)
{
  int          timeout_ms = 2000;
  static char *kwlist[]   = { "timeout_ms", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", kwlist, &timeout_ms))
    return NULL;
  if (self->closed || !self->ctx)
    {
      PyErr_SetString (PyExc_RuntimeError, "publisher is closed");
      return NULL;
    }

  int rc;
  Py_BEGIN_ALLOW_THREADS
    ;
    rc = dp_pub_flush (self->ctx, timeout_ms);
  Py_END_ALLOW_THREADS;

  if (rc == DP_ERR_TIMEOUT)
    {
      PyErr_Format (PyExc_TimeoutError,
                    "the server did not acknowledge everything published "
                    "within %d ms; data is still buffered",
                    timeout_ms);
      return NULL;
    }
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "flush failed: %s", dp_strerror (rc));
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Publisher_drain (PublisherObject *self, PyObject *args, PyObject *kwds)
{
  int          timeout_ms = 5000;
  static char *kwlist[]   = { "timeout_ms", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", kwlist, &timeout_ms))
    return NULL;
  if (self->closed || !self->ctx)
    {
      PyErr_SetString (PyExc_RuntimeError, "publisher is closed");
      return NULL;
    }

  int rc;
  Py_BEGIN_ALLOW_THREADS
    ;
    rc = dp_stream_drain (self->ctx, timeout_ms);
  Py_END_ALLOW_THREADS;

  if (rc == DP_ERR_TIMEOUT)
    {
      PyErr_Format (PyExc_TimeoutError,
                    "the drain did not complete within %d ms", timeout_ms);
      return NULL;
    }
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "drain failed: %s", dp_strerror (rc));
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Publisher_send_eos (PublisherObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->ctx)
    {
      PyErr_SetString (PyExc_RuntimeError, "publisher is closed");
      return NULL;
    }
  int rc;
  Py_BEGIN_ALLOW_THREADS
    ;
    rc = dp_pub_send_eos (self->ctx);
  Py_END_ALLOW_THREADS;
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "send_eos failed: %s",
                    dp_strerror (rc));
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyMethodDef Publisher_methods[] = {
  { "send", (PyCFunction)Publisher_send, METH_VARARGS | METH_KEYWORDS,
    "send(samples, sample_rate=0, center_freq=0, timestamp_ns=None) -- "
    "timestamp_ns overrides the auto-stamped send time, propagating an "
    "upstream origin timestamp instead of stamping now" },
  { "send_eos", (PyCFunction)Publisher_send_eos, METH_NOARGS,
    "send_eos() -> None\n"
    "\n"
    "Tell subscribers the stream has ended.\n"
    "\n"
    "A subscriber's recv() raises EOFError instead of waiting out a\n"
    "timeout -- which means only \"nothing yet\" and cannot be told from\n"
    "\"nothing ever\" without this.\n"
    "\n"
    "Send it BEFORE drain(), not after: a drain cannot be reversed and\n"
    "refuses sends once it reaches its publish-flushing phase. The order\n"
    "is: stop producing, send_eos(), drain(), close().\n"
    "\n"
    "PUB/SUB is at-most-once, so this frame can be dropped like any\n"
    "other: it makes the common case end promptly, not reliably, and a\n"
    "subscriber that must not hang still needs a timeout. PUSH/PULL\n"
    "delivers it at-least-once, so handling must be idempotent.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import Publisher, CF32\n"
    ">>> pub = Publisher(\"nats://127.0.0.1:4222/demo\", CF32)  # doctest: "
    "+SKIP\n"
    ">>> pub.send_eos()                                        # doctest: "
    "+SKIP\n"
    ">>> pub.drain()                                           # doctest: "
    "+SKIP\n" },
  { "flush", (PyCFunction)Publisher_flush, METH_VARARGS | METH_KEYWORDS,
    "flush(timeout_ms=2000) -> None\n"
    "\n"
    "Wait until the server has everything published so far.\n"
    "\n"
    "send() hands the frame to the client and returns; the client writes\n"
    "it in the background, so \"the send returned\" is not \"the server\n"
    "has it\". This waits for a round trip.\n"
    "\n"
    "You do NOT need it before close(): the NATS client flushes what is\n"
    "buffered when the connection closes. But it does so best-effort\n"
    "with a 500 ms cap and no way to report failure, so a backlog that\n"
    "cannot drain in half a second is dropped silently -- and on a link\n"
    "slower than loopback that is not a large backlog. Call this when\n"
    "losing the tail would matter, and you get a budget you chose and an\n"
    "answer you can act on. It is also the only way to ask the question\n"
    "without closing.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "timeout_ms : int, optional\n"
    "    How long to wait (default 2000).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Raises\n"
    "------\n"
    "TimeoutError\n"
    "    If the budget ran out with data still buffered.\n"
    "RuntimeError\n"
    "    If the publisher is closed, or the flush failed outright.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.stream import Publisher, CF64  # doctest: +SKIP\n"
    ">>> with Publisher(\"nats://127.0.0.1:4222/iq\", CF64) as pub:\n"
    "...     pub.send(np.zeros(8, dtype=np.complex128))  # doctest: +SKIP\n"
    "...     pub.flush()                                 # doctest: +SKIP\n" },
  { "drain", (PyCFunction)Publisher_drain, METH_VARARGS | METH_KEYWORDS,
    "drain(timeout_ms=5000) -> None\n"
    "\n"
    "Shut down gracefully: stop accepting new work, let what is in\n"
    "flight finish, flush everything pending, close.\n"
    "\n"
    "This WAITS for the connection to close, which is the part worth\n"
    "having: the underlying drain returns immediately and finishes in\n"
    "the background, so a process that exits when it returns abandons\n"
    "exactly the work the drain was for.\n"
    "\n"
    "Drain LAST, after you have stopped producing. It cannot be\n"
    "reversed, and a send issued while one is in progress races its\n"
    "phases -- it may slip through, or be refused. Because this waits,\n"
    "a single-threaded caller need not reason about that: afterwards a\n"
    "send raises RuntimeError deterministically.\n"
    "\n"
    "Against flush(): flush asks whether the server has what you\n"
    "published and leaves the publisher usable; drain ends it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "timeout_ms : int, optional\n"
    "    How long to wait for the close (default 5000).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Raises\n"
    "------\n"
    "TimeoutError\n"
    "    If the drain did not complete in the budget.\n"
    "RuntimeError\n"
    "    If the publisher is already closed, or the drain failed.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import Publisher, CF64   # doctest: +SKIP\n"
    ">>> pub = Publisher(\"nats://127.0.0.1:4222/iq\", CF64)  # doctest: "
    "+SKIP\n"
    ">>> pub.drain()   # stopped producing, so shut down  # doctest: +SKIP\n"
    ">>> pub.close()                                      # doctest: "
    "+SKIP\n" },
  { "close", (PyCFunction)Publisher_close, METH_NOARGS,
    "close() — destroy the socket" },
  { "__enter__", (PyCFunction)Publisher_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Publisher_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PublisherType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Publisher",
  .tp_basicsize                           = sizeof (PublisherObject),
  .tp_dealloc                             = (destructor)Publisher_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Publisher(endpoint, sample_type=CF64) — NATS PUB",
  .tp_methods = Publisher_methods,
  .tp_new     = Publisher_new,
};

/* =========================================================================
 * Subscriber (NATS SUB)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_sub_t *ctx;
  int                     closed;
} SubscriberObject;

static void
Subscriber_dealloc (SubscriberObject *self)
{
  if (!self->closed && self->ctx)
    dp_sub_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Subscriber_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;

  static char *kwlist[] = { "endpoint", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", kwlist, &endpoint))
    return NULL;

  SubscriberObject *self = (SubscriberObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_sub_create (endpoint);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_sub_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
Subscriber_recv (SubscriberObject *self, PyObject *args, PyObject *kwds)
{
  return do_recv (self->ctx, (set_timeout_fn)dp_sub_set_timeout,
                  (recv_signal_fn)dp_sub_recv, args, kwds);
}

static PyObject *
Subscriber_close (SubscriberObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_sub_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Subscriber_enter (SubscriberObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Subscriber_exit (SubscriberObject *self, PyObject *Py_UNUSED (args))
{
  return Subscriber_close (self, NULL);
}

static PyMethodDef Subscriber_methods[] = {
  { "recv", (PyCFunction)Subscriber_recv, METH_VARARGS | METH_KEYWORDS,
    "recv(timeout_ms=-1) -> (samples, header) — zero-copy recv" },
  { "close", (PyCFunction)Subscriber_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Subscriber_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Subscriber_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject SubscriberType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Subscriber",
  .tp_basicsize                           = sizeof (SubscriberObject),
  .tp_dealloc                             = (destructor)Subscriber_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc                                 = "Subscriber(endpoint) — NATS SUB",
  .tp_methods                             = Subscriber_methods,
  .tp_new                                 = Subscriber_new,
};

/* =========================================================================
 * Push (NATS JetStream work-queue)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_push_t *ctx;
  int                      sample_type;
  int                      closed;
} PushObject;

static void
Push_dealloc (PushObject *self)
{
  if (!self->closed && self->ctx)
    dp_push_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Push_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;
  int         sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (!dp_sample_type_is_valid ((dp_sample_type_t)sample_type))
    {
      PyErr_SetString (PyExc_ValueError, "Invalid sample_type");
      return NULL;
    }

  PushObject *self = (PushObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_push_create (endpoint, (dp_sample_type_t)sample_type);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_push_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->sample_type = sample_type;
  self->closed      = 0;
  return (PyObject *)self;
}

static PyObject *
Push_send (PushObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (
      self->ctx, self->sample_type, (send_ci32_fn)dp_push_send_ci32,
      (send_cf64_fn)dp_push_send_cf64, (send_ci8_fn)dp_push_send_ci8,
      (send_ci16_fn)dp_push_send_ci16, (send_cf32_fn)dp_push_send_cf32, args,
      kwds);
}

static PyObject *
Push_close (PushObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_push_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Push_enter (PushObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Push_exit (PushObject *self, PyObject *Py_UNUSED (args))
{
  return Push_close (self, NULL);
}

static PyMethodDef Push_methods[] = {
  { "send", (PyCFunction)Push_send, METH_VARARGS | METH_KEYWORDS,
    "send(samples, sample_rate=0, center_freq=0, timestamp_ns=None) -- "
    "timestamp_ns overrides the auto-stamped send time, propagating an "
    "upstream origin timestamp instead of stamping now" },
  { "close", (PyCFunction)Push_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Push_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Push_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PushType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Push",
  .tp_basicsize                           = sizeof (PushObject),
  .tp_dealloc                             = (destructor)Push_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Push(endpoint, sample_type=CF64) — NATS JetStream work-queue producer",
  .tp_methods = Push_methods,
  .tp_new     = Push_new,
};

/* =========================================================================
 * Pull (NATS JetStream work-queue)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_pull_t *ctx;
  int                      closed;
} PullObject;

static void
Pull_dealloc (PullObject *self)
{
  if (!self->closed && self->ctx)
    dp_pull_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Pull_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;

  static char *kwlist[] = { "endpoint", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", kwlist, &endpoint))
    return NULL;

  PullObject *self = (PullObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_pull_create (endpoint);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_pull_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
Pull_recv (PullObject *self, PyObject *args, PyObject *kwds)
{
  return do_recv (self->ctx, (set_timeout_fn)dp_pull_set_timeout,
                  (recv_signal_fn)dp_pull_recv, args, kwds);
}

static PyObject *
Pull_ack (PullObject *Py_UNUSED (self), PyObject *arr)
{
  /* `arr` is the samples array from recv(); its NumPy base is the dpMsgObject
   * holding the dp_msg.  Acknowledges a JetStream work-queue message (so it is
   * not redelivered); a no-op for core-NATS PUB/SUB.  Call after the
   * frame has been processed, before dropping the array. */
  if (!PyArray_Check (arr))
    {
      PyErr_SetString (PyExc_TypeError,
                       "ack() expects the samples array returned by recv()");
      return NULL;
    }
  PyObject *base = PyArray_BASE ((PyArrayObject *)arr);
  if (!base || Py_TYPE (base) != &dpMsgType)
    {
      PyErr_SetString (PyExc_ValueError,
                       "array is not an un-freed recv() result");
      return NULL;
    }
  dp_msg_t *msg = ((dpMsgObject *)base)->msg;
  int       rc;
  Py_BEGIN_ALLOW_THREADS
    rc = dp_msg_ack (msg);
  Py_END_ALLOW_THREADS
  if (rc != DP_OK)
    {
      PyErr_Format (PyExc_RuntimeError, "ack failed: %s", dp_strerror (rc));
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Pull_close (PullObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_pull_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Pull_enter (PullObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Pull_exit (PullObject *self, PyObject *Py_UNUSED (args))
{
  return Pull_close (self, NULL);
}

static PyMethodDef Pull_methods[] = {
  { "recv", (PyCFunction)Pull_recv, METH_VARARGS | METH_KEYWORDS,
    "recv(timeout_ms=-1) -> (samples, header) — zero-copy recv" },
  { "ack", (PyCFunction)Pull_ack, METH_O,
    "ack(samples) — acknowledge a JetStream work-queue frame (no-op on "
    "PUB/SUB)" },
  { "close", (PyCFunction)Pull_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Pull_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Pull_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PullType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Pull",
  .tp_basicsize                           = sizeof (PullObject),
  .tp_dealloc                             = (destructor)Pull_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Pull(endpoint) — NATS JetStream work-queue consumer",
  .tp_methods = Pull_methods,
  .tp_new     = Pull_new,
};

/* =========================================================================
 * Requester (NATS REQ)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_req_t *ctx;
  int                     sample_type;
  int                     closed;
} RequesterObject;

static void
Requester_dealloc (RequesterObject *self)
{
  if (!self->closed && self->ctx)
    dp_req_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Requester_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;
  int         sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (!dp_sample_type_is_valid ((dp_sample_type_t)sample_type))
    {
      PyErr_SetString (PyExc_ValueError, "Invalid sample_type");
      return NULL;
    }

  RequesterObject *self = (RequesterObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_req_create (endpoint);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_req_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->sample_type = sample_type;
  self->closed      = 0;
  return (PyObject *)self;
}

static PyObject *
Requester_send (RequesterObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (self->ctx, self->sample_type, (send_ci32_fn)dp_req_send_ci32,
                  (send_cf64_fn)dp_req_send_cf64, (send_ci8_fn)dp_req_send_ci8,
                  (send_ci16_fn)dp_req_send_ci16,
                  (send_cf32_fn)dp_req_send_cf32, args, kwds);
}

static PyObject *
Requester_recv (RequesterObject *self, PyObject *args, PyObject *kwds)
{
  return do_recv (self->ctx, (set_timeout_fn)dp_req_set_timeout,
                  (recv_signal_fn)dp_req_recv_signal, args, kwds);
}

static PyObject *
Requester_close (RequesterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_req_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Requester_enter (RequesterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Requester_exit (RequesterObject *self, PyObject *Py_UNUSED (args))
{
  return Requester_close (self, NULL);
}

static PyMethodDef Requester_methods[] = {
  { "send", (PyCFunction)Requester_send, METH_VARARGS | METH_KEYWORDS,
    "send(samples, sample_rate=0, center_freq=0, timestamp_ns=None) -- "
    "timestamp_ns overrides the auto-stamped send time, propagating an "
    "upstream origin timestamp instead of stamping now" },
  { "recv", (PyCFunction)Requester_recv, METH_VARARGS | METH_KEYWORDS,
    "recv(timeout_ms=-1) -> (samples, header)" },
  { "close", (PyCFunction)Requester_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Requester_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Requester_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject RequesterType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Requester",
  .tp_basicsize                           = sizeof (RequesterObject),
  .tp_dealloc                             = (destructor)Requester_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Requester(endpoint, sample_type=CF64) — NATS request",
  .tp_methods = Requester_methods,
  .tp_new     = Requester_new,
};

/* =========================================================================
 * Replier (NATS REP)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_rep_t *ctx;
  int                     sample_type;
  int                     closed;
} ReplierObject;

static void
Replier_dealloc (ReplierObject *self)
{
  if (!self->closed && self->ctx)
    dp_rep_destroy (self->ctx);
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
Replier_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  const char *endpoint;
  int         sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (!dp_sample_type_is_valid ((dp_sample_type_t)sample_type))
    {
      PyErr_SetString (PyExc_ValueError, "Invalid sample_type");
      return NULL;
    }

  ReplierObject *self = (ReplierObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_rep_create (endpoint);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_rep_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->sample_type = sample_type;
  self->closed      = 0;
  return (PyObject *)self;
}

static PyObject *
Replier_recv (ReplierObject *self, PyObject *args, PyObject *kwds)
{
  return do_recv (self->ctx, (set_timeout_fn)dp_rep_set_timeout,
                  (recv_signal_fn)dp_rep_recv_signal, args, kwds);
}

static PyObject *
Replier_send (ReplierObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (self->ctx, self->sample_type, (send_ci32_fn)dp_rep_send_ci32,
                  (send_cf64_fn)dp_rep_send_cf64, (send_ci8_fn)dp_rep_send_ci8,
                  (send_ci16_fn)dp_rep_send_ci16,
                  (send_cf32_fn)dp_rep_send_cf32, args, kwds);
}

static PyObject *
Replier_close (ReplierObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_rep_destroy (self->ctx);
      self->ctx    = NULL;
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Replier_enter (ReplierObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Replier_exit (ReplierObject *self, PyObject *Py_UNUSED (args))
{
  return Replier_close (self, NULL);
}

static PyMethodDef Replier_methods[] = {
  { "recv", (PyCFunction)Replier_recv, METH_VARARGS | METH_KEYWORDS,
    "recv(timeout_ms=-1) -> (samples, header)" },
  { "send", (PyCFunction)Replier_send, METH_VARARGS | METH_KEYWORDS,
    "send(samples, sample_rate=0, center_freq=0, timestamp_ns=None) -- "
    "timestamp_ns overrides the auto-stamped send time, propagating an "
    "upstream origin timestamp instead of stamping now" },
  { "close", (PyCFunction)Replier_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Replier_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Replier_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject ReplierType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Replier",
  .tp_basicsize                           = sizeof (ReplierObject),
  .tp_dealloc                             = (destructor)Replier_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Replier(endpoint, sample_type=CF64) — NATS reply",
  .tp_methods = Replier_methods,
  .tp_new     = Replier_new,
};

/* =========================================================================
 * Module-level functions
 * ========================================================================= */

static PyObject *
py_get_timestamp_ns (PyObject *self, PyObject *args)
{
  (void)self;
  (void)args;
  return PyLong_FromUnsignedLongLong (dp_get_timestamp_ns ());
}

/* mean_power(samples) -> float
 *
 * The same dp_mean_power() the C examples call, so the Python and C
 * receivers report one number computed one way. The format comes from the
 * array's dtype, which is what recv() set from the frame's own header. */
static PyObject *
py_mean_power (PyObject *self, PyObject *args)
{
  (void)self;
  PyArrayObject *arr;
  if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &arr))
    return NULL;
  if (!PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError, "samples must be C-contiguous");
      return NULL;
    }

  dp_sample_type_t fmt;
  switch (PyArray_TYPE (arr))
    {
    case NPY_COMPLEX64:
      fmt = CF32;
      break;
    case NPY_COMPLEX128:
      fmt = CF64;
      break;
    case NPY_INT8:
      fmt = CI8;
      break;
    case NPY_INT16:
      fmt = CI16;
      break;
    case NPY_INT32:
      fmt = CI32;
      break;
    default:
      PyErr_SetString (PyExc_TypeError,
                       "samples dtype is not a doppler wire format "
                       "(complex64/complex128, or int8/int16/int32 "
                       "interleaved I/Q)");
      return NULL;
    }

  npy_intp n = PyArray_SIZE (arr);
  if (fmt == CI8 || fmt == CI16 || fmt == CI32)
    n /= 2; /* interleaved I/Q pairs */

  double p;
  void  *data = PyArray_DATA (arr);
  Py_BEGIN_ALLOW_THREADS
    ;
    p = dp_mean_power (fmt, data, (size_t)n);
  Py_END_ALLOW_THREADS;
  return PyFloat_FromDouble (p);
}

/* format_name(code) -> str — the same dp_sample_type_str() the C face
 * prints, so the two receivers name a format identically instead of each
 * carrying a private code-to-name table. */
static PyObject *
py_format_name (PyObject *self, PyObject *args)
{
  (void)self;
  int code;
  if (!PyArg_ParseTuple (args, "i", &code))
    return NULL;
  return PyUnicode_FromString (dp_sample_type_str ((dp_sample_type_t)code));
}

static PyObject *
py_interrupt (PyObject *self, PyObject *args)
{
  (void)self;
  (void)args;
  dp_stream_interrupt ();
  Py_RETURN_NONE;
}

static PyObject *
py_resume (PyObject *self, PyObject *args)
{
  (void)self;
  (void)args;
  dp_stream_resume ();
  Py_RETURN_NONE;
}

static PyObject *
py_set_interrupt_latency_ms (PyObject *self, PyObject *args)
{
  (void)self;
  unsigned ms;
  if (!PyArg_ParseTuple (args, "I", &ms))
    return NULL;
  dp_stream_set_interrupt_latency_ms (ms);
  Py_RETURN_NONE;
}

static PyObject *
py_interrupt_latency_ms (PyObject *self, PyObject *args)
{
  (void)self;
  (void)args;
  return PyLong_FromUnsignedLong (dp_stream_interrupt_latency_ms ());
}

static PyObject *
py_interrupted (PyObject *self, PyObject *args)
{
  (void)self;
  (void)args;
  return PyBool_FromLong (dp_stream_interrupted ());
}

/* =========================================================================
 * interrupt_on_sigint — the context manager
 *
 * In C with the rest of the binding, not a .py beside it: this package is
 * a thin face over the C ABI, and the mechanism it drives (a sigaction
 * chained to CPython's own) is C anyway. Same __enter__/__exit__ shape as
 * the socket types above, so a reader meets one pattern.
 * ========================================================================= */

#define DP_GUARD_MAX_SIGS 8

typedef struct
{
  PyObject_HEAD int sigs[DP_GUARD_MAX_SIGS];
  Py_ssize_t        nsigs;
  int               armed;
  unsigned          latency_ms; /* 0 = leave the process setting alone */
  unsigned          saved_latency_ms;
} InterruptGuardObject;

static PyTypeObject InterruptGuardType;

static PyObject *
InterruptGuard_enter (InterruptGuardObject *self, PyObject *Py_UNUSED (a))
{
  /* A stale flag would refuse the first receive inside the block. */
  dp_stream_resume ();
  if (self->latency_ms)
    {
      self->saved_latency_ms = dp_stream_interrupt_latency_ms ();
      dp_stream_set_interrupt_latency_ms (self->latency_ms);
    }
  for (Py_ssize_t i = 0; i < self->nsigs; i++)
    {
      if (dp_stream_interrupt_on_signal (self->sigs[i]) != DP_OK)
        {
          for (Py_ssize_t j = 0; j < i; j++)
            dp_stream_restore_signal (self->sigs[j]);
          PyErr_Format (PyExc_OSError,
                        "cannot install a handler for signal %d",
                        self->sigs[i]);
          return NULL;
        }
    }
  self->armed = 1;
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
InterruptGuard_exit (InterruptGuardObject *self, PyObject *Py_UNUSED (a))
{
  if (self->armed)
    {
      for (Py_ssize_t i = 0; i < self->nsigs; i++)
        dp_stream_restore_signal (self->sigs[i]);
      self->armed = 0;
    }
  if (self->latency_ms)
    dp_stream_set_interrupt_latency_ms (self->saved_latency_ms);
  /* Cleared on the way out so a later receive is not refused by an
     interrupt that has already been handled. */
  dp_stream_resume ();
  Py_RETURN_FALSE;
}

static void
InterruptGuard_dealloc (InterruptGuardObject *self)
{
  if (self->armed)
    {
      for (Py_ssize_t i = 0; i < self->nsigs; i++)
        dp_stream_restore_signal (self->sigs[i]);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyMethodDef InterruptGuard_methods[] = {
  { "__enter__", (PyCFunction)InterruptGuard_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)InterruptGuard_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject InterruptGuardType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.stream.InterruptGuard",
  .tp_basicsize                           = sizeof (InterruptGuardObject),
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc     = "Context manager returned by interrupt_on_sigint().",
  .tp_methods = InterruptGuard_methods,
  .tp_dealloc = (destructor)InterruptGuard_dealloc,
};

static PyObject *
py_interrupt_on_sigint (PyObject *self, PyObject *args, PyObject *kwds)
{
  (void)self;
  unsigned  latency_ms = 0;
  PyObject *kw_latency
      = kwds ? PyDict_GetItemString (kwds, "latency_ms") : NULL;
  if (kwds && PyDict_Size (kwds) > (kw_latency ? 1 : 0))
    {
      PyErr_SetString (PyExc_TypeError,
                       "interrupt_on_sigint() takes signals positionally "
                       "and only latency_ms by keyword");
      return NULL;
    }
  if (kw_latency)
    {
      long v = PyLong_AsLong (kw_latency);
      if (v == -1 && PyErr_Occurred ())
        return NULL;
      if (v < 0)
        {
          PyErr_SetString (PyExc_ValueError, "latency_ms must not be < 0");
          return NULL;
        }
      latency_ms = (unsigned)v;
    }

  Py_ssize_t extra = PyTuple_GET_SIZE (args);
  if (extra + 1 > DP_GUARD_MAX_SIGS)
    {
      PyErr_SetString (PyExc_ValueError, "too many signals");
      return NULL;
    }

  InterruptGuardObject *g
      = PyObject_New (InterruptGuardObject, &InterruptGuardType);
  if (!g)
    return NULL;
  g->armed            = 0;
  g->latency_ms       = latency_ms;
  g->saved_latency_ms = 0;
  g->sigs[0]          = SIGINT;
  g->nsigs            = 1;
  for (Py_ssize_t i = 0; i < extra; i++)
    {
      long v = PyLong_AsLong (PyTuple_GET_ITEM (args, i));
      if (v == -1 && PyErr_Occurred ())
        {
          Py_DECREF (g);
          return NULL;
        }
      g->sigs[g->nsigs++] = (int)v;
    }
  return (PyObject *)g;
}

/* =========================================================================
 * Module definition
 * ========================================================================= */

static PyMethodDef module_methods[] = {
  { "get_timestamp_ns", py_get_timestamp_ns, METH_NOARGS,
    "get_timestamp_ns() -> int\n"
    "Current wall-clock time in nanoseconds (CLOCK_REALTIME)." },
  { "interrupt", py_interrupt, METH_NOARGS,
    "interrupt() -> None\n"
    "\n"
    "Ask every blocking receive in this process to return now.\n"
    "\n"
    "A blocking ``recv()`` waits inside the NATS client with the GIL\n"
    "released, so Python's own signal handling does not run until it\n"
    "returns -- which, with no sender, is never. This is the flag the\n"
    "library checks inside that wait; a receive it unblocks raises\n"
    "``KeyboardInterrupt``, so the rest of the program behaves exactly\n"
    "as it would for any other Ctrl+C.\n"
    "\n"
    "``interrupt_on_sigint()`` is the context manager that wires this to\n"
    "SIGINT for you; call this directly to stop a receive from another\n"
    "thread.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import interrupt, interrupted, resume\n"
    ">>> interrupt()\n"
    ">>> interrupted()\n"
    "True\n"
    ">>> resume()\n"
    ">>> interrupted()\n"
    "False\n" },
  { "resume", py_resume, METH_NOARGS,
    "resume() -> None\n"
    "\n"
    "Clear the interrupt, so blocking receives block again.\n"
    "\n"
    "The flag is sticky on purpose: a handler fires once and the loops\n"
    "it unblocks may be several, so an auto-clearing flag would release\n"
    "one caller and leave the rest parked.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import interrupt, interrupted, resume\n"
    ">>> interrupt(); resume()\n"
    ">>> interrupted()\n"
    "False\n" },
  { "interrupted", py_interrupted, METH_NOARGS,
    "interrupted() -> bool\n"
    "\n"
    "True when an interrupt is pending.\n"
    "\n"
    "For a loop that wants to notice without calling ``recv()`` again.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bool\n"
    "    Whether a blocking receive would return immediately.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import interrupted, resume\n"
    ">>> resume()\n"
    ">>> interrupted()\n"
    "False\n" },
  { "interrupt_on_sigint", (PyCFunction)py_interrupt_on_sigint,
    METH_VARARGS | METH_KEYWORDS,
    "interrupt_on_sigint(*extra_signals) -> context manager\n"
    "\n"
    "Make Ctrl+C stop a blocking receive, for the duration of the block.\n"
    "\n"
    "Installs a C handler that calls interrupt(), so a recv() already\n"
    "blocked returns promptly and raises KeyboardInterrupt -- the\n"
    "exception Ctrl+C raises anywhere else, so existing try/finally and\n"
    "`with` cleanup applies unchanged. The handler is installed in C\n"
    "because a Python one runs only when the interpreter next regains\n"
    "control, which is exactly what the blocking wait prevents.\n"
    "\n"
    "Whatever handler was there is CHAINED, not replaced, so a Ctrl+C\n"
    "arriving outside a receive behaves as it always did. On exit the\n"
    "previous handlers are restored and the flag is cleared.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "*extra_signals : int\n"
    "    Further signals to treat the same way, e.g. signal.SIGTERM for a\n"
    "    container that is stopped rather than interrupted. SIGINT is\n"
    "    always included.\n"
    "latency_ms : int, optional\n"
    "    Set the interrupt latency for the duration of the block and\n"
    "    restore it after. See set_interrupt_latency_ms().\n"
    "\n"
    "Returns\n"
    "-------\n"
    "context manager\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If a signal cannot be caught (SIGKILL, SIGSTOP).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import interrupt_on_sigint, interrupted\n"
    ">>> with interrupt_on_sigint():\n"
    "...     interrupted()\n"
    "False\n" },
  { "set_interrupt_latency_ms", py_set_interrupt_latency_ms, METH_VARARGS,
    "set_interrupt_latency_ms(ms) -> None\n"
    "\n"
    "How soon a blocking receive must notice interrupt().\n"
    "\n"
    "The library cannot be woken from the NATS client's wait, so it\n"
    "waits in slices and checks between them; this is the worst-case\n"
    "delay between interrupt() and the receive returning, and the cost\n"
    "is one wakeup per slice on an idle receiver. A human pressing\n"
    "Ctrl+C cannot perceive the 100 ms default; a control loop can, and\n"
    "a battery-powered sensor would rather wake once a second.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "ms : int\n"
    "    Milliseconds; 0 restores the default.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import (\n"
    "...     interrupt_latency_ms, set_interrupt_latency_ms)\n"
    ">>> set_interrupt_latency_ms(10)\n"
    ">>> interrupt_latency_ms()\n"
    "10\n"
    ">>> set_interrupt_latency_ms(0)   # back to the default\n"
    ">>> interrupt_latency_ms()\n"
    "100\n" },
  { "interrupt_latency_ms", py_interrupt_latency_ms, METH_NOARGS,
    "interrupt_latency_ms() -> int\n"
    "\n"
    "The interrupt latency in force, in milliseconds.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import interrupt_latency_ms\n"
    ">>> interrupt_latency_ms() > 0\n"
    "True\n" },
  { "format_name", py_format_name, METH_VARARGS,
    "format_name(code) -> str\n"
    "\n"
    "The name of a wire format code.\n"
    "\n"
    "The same ``dp_sample_type_str()`` the C face prints, so a Python\n"
    "receiver names a format exactly as the C one does rather than\n"
    "carrying a private code-to-name table.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "code : int\n"
    "    A wire format, e.g. ``CF64``.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "str\n"
    "    The format's name, or ``\"UNKNOWN\"`` for a code this build does\n"
    "    not know.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.stream import format_name, CF64, CI8\n"
    ">>> format_name(CF64), format_name(CI8)\n"
    "('CF64', 'CI8')\n" },
  { "mean_power", py_mean_power, METH_VARARGS,
    "mean_power(samples) -> float\n"
    "\n"
    "Mean power of a complex sample block, normalised to full scale.\n"
    "\n"
    "``mean(|x|**2)``, with the integer formats divided by their full\n"
    "scale first, so the answer means the same thing whatever the wire\n"
    "carried and ``10*log10()`` of it is dBFS in every case. This is the\n"
    "same ``dp_mean_power()`` the C examples call -- one implementation,\n"
    "so the Python and C receivers cannot report different numbers for\n"
    "one frame.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "samples : ndarray\n"
    "    C-contiguous block: ``complex64``/``complex128``, or\n"
    "    ``int8``/``int16``/``int32`` interleaved I/Q (length ``2*n``).\n"
    "\n"
    "Returns\n"
    "-------\n"
    "float\n"
    "    Mean power, 0.0 for an empty block.\n"
    "\n"
    "Raises\n"
    "------\n"
    "TypeError\n"
    "    If the dtype is not one of the wire formats.\n"
    "ValueError\n"
    "    If ``samples`` is not C-contiguous.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.stream import mean_power\n"
    ">>> mean_power(np.ones(4, dtype=np.complex64))\n"
    "1.0\n" },
  { NULL },
};

static PyModuleDef stream_module = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "stream",
  .m_doc     = "Doppler streaming — NATS PUB/SUB, PUSH/PULL, REQ/REP.",
  .m_size    = -1,
  .m_methods = module_methods,
};

PyMODINIT_FUNC
PyInit_stream (void)
{
  import_array ();

  /* TLM16 record dtype: 16 bytes packed, the exact dp_tlm_rec_t layout
   * (mirrors doppler.telemetry's read() dtype). */
  PyObject *spec = Py_BuildValue ("[(ss)(ss)(ss)(ss)]", "n", "<u8", "value",
                                  "<f4", "probe", "<u2", "flags", "<u2");
  if (!spec)
    return NULL;
  int descr_ok = PyArray_DescrConverter (spec, &tlm16_descr);
  Py_DECREF (spec);
  if (!descr_ok)
    return NULL;

  if (PyType_Ready (&dpMsgType) < 0)
    return NULL;
  if (PyType_Ready (&PublisherType) < 0)
    return NULL;
  if (PyType_Ready (&SubscriberType) < 0)
    return NULL;
  if (PyType_Ready (&PushType) < 0)
    return NULL;
  if (PyType_Ready (&PullType) < 0)
    return NULL;
  if (PyType_Ready (&RequesterType) < 0)
    return NULL;
  if (PyType_Ready (&ReplierType) < 0)
    return NULL;

  PyObject *m = PyModule_Create (&stream_module);
  if (!m)
    return NULL;

  Py_INCREF (&PublisherType);
  PyModule_AddObject (m, "Publisher", (PyObject *)&PublisherType);
  Py_INCREF (&SubscriberType);
  PyModule_AddObject (m, "Subscriber", (PyObject *)&SubscriberType);
  Py_INCREF (&PushType);
  PyModule_AddObject (m, "Push", (PyObject *)&PushType);
  Py_INCREF (&PullType);
  PyModule_AddObject (m, "Pull", (PyObject *)&PullType);
  Py_INCREF (&RequesterType);
  PyModule_AddObject (m, "Requester", (PyObject *)&RequesterType);
  Py_INCREF (&ReplierType);
  PyModule_AddObject (m, "Replier", (PyObject *)&ReplierType);

  PyModule_AddIntConstant (m, "CI32", CI32);
  PyModule_AddIntConstant (m, "CF64", CF64);
  PyModule_AddIntConstant (m, "CI8", CI8);
  PyModule_AddIntConstant (m, "CI16", CI16);
  PyModule_AddIntConstant (m, "CF32", CF32);
  if (PyType_Ready (&InterruptGuardType) < 0)
    return NULL;

  PyModule_AddIntConstant (m, "TLM16", DP_KIND_TLM);

  return m;
}
