/*
 * stream_ext.c — Python C extension wrapping the doppler streaming C library.
 *
 * Thin wrapper around libdoppler's dp_pub_t/dp_sub_t/dp_push_t/dp_pull_t/
 * dp_req_t/dp_rep_t API.  All socket creation, header construction,
 * send/recv logic, and protocol handling lives in the C library.
 *
 * Zero-copy recv: dp_msg_t owns the ZMQ buffer; we wrap it in dpMsgObject
 * which is set as the NumPy array's base object.  When the array is GC'd,
 * dpMsgObject.dealloc calls dp_msg_free().
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

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
  .tp_basicsize = sizeof (dpMsgObject),
  .tp_dealloc = (destructor)dpMsg_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Internal dp_msg_t wrapper for zero-copy recv",
};

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

  npy_intp dims[1];
  int typenum;
  dp_sample_type_t st = dp_msg_sample_type (msg);

  if (st == CI32)
    {
      dims[0] = (npy_intp)(dp_msg_num_samples (msg) * 2); /* interleaved I/Q */
      typenum = NPY_INT32;
    }
  else if (st == CF64)
    {
      dims[0] = (npy_intp)dp_msg_num_samples (msg);
      typenum = NPY_COMPLEX128;
    }
  else if (st == CF128)
    {
      dims[0] = (npy_intp)dp_msg_num_samples (msg);
      typenum = NPY_CLONGDOUBLE;
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
      PyErr_Format (PyExc_ValueError, "Unknown sample_type: %u", (unsigned)st);
      return NULL;
    }

  PyObject *arr
      = PyArray_SimpleNewFromData (1, dims, typenum, dp_msg_data (msg));
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
  PyDict_SetItemString (header, "sample_type",
                        PyLong_FromLong (hdr->sample_type));
  PyDict_SetItemString (header, "protocol", PyLong_FromLong (hdr->protocol));
  PyDict_SetItemString (header, "stream_id", PyLong_FromLong (hdr->stream_id));

  return Py_BuildValue ("(NN)", arr, header);
}

/* Generic signal-frame send (shared by all sender socket types). */
typedef int (*send_ci32_fn) (void *, const int32_t *, size_t, double, double);
typedef int (*send_cf64_fn) (void *, const double _Complex *, size_t, double,
                             double);
typedef int (*send_cf128_fn) (void *, const long double _Complex *, size_t,
                              double, double);
typedef int (*send_ci8_fn) (void *, const int8_t *, size_t, double, double);
typedef int (*send_ci16_fn) (void *, const int16_t *, size_t, double, double);
typedef int (*send_cf32_fn) (void *, const float _Complex *, size_t, double,
                             double);

static PyObject *
do_send (void *ctx, int sample_type, send_ci32_fn fn_ci32,
         send_cf64_fn fn_cf64, send_cf128_fn fn_cf128, send_ci8_fn fn_ci8,
         send_ci16_fn fn_ci16, send_cf32_fn fn_cf32, PyObject *args,
         PyObject *kwds)
{
  PyArrayObject *arr;
  double sample_rate = 0.0;
  double center_freq = 0.0;

  static char *kwlist[] = { "samples", "sample_rate", "center_freq", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O!|dd", kwlist, &PyArray_Type,
                                    &arr, &sample_rate, &center_freq))
    return NULL;

  if (!PyArray_IS_C_CONTIGUOUS (arr))
    {
      PyErr_SetString (PyExc_ValueError, "samples must be C-contiguous");
      return NULL;
    }

  int expected = (sample_type == CI32)   ? NPY_INT32
                 : (sample_type == CF64)  ? NPY_COMPLEX128
                 : (sample_type == CF128) ? NPY_CLONGDOUBLE
                 : (sample_type == CI8)   ? NPY_INT8
                 : (sample_type == CI16)  ? NPY_INT16
                                          : NPY_COMPLEX64; /* CF32 */
  if (PyArray_TYPE (arr) != expected)
    {
      PyErr_SetString (PyExc_TypeError, "samples dtype mismatch");
      return NULL;
    }

  npy_intp num_samples = PyArray_SIZE (arr);
  if (sample_type == CI32 || sample_type == CI8 || sample_type == CI16)
    num_samples /= 2; /* interleaved I/Q pairs */

  int rc;
  void *data = PyArray_DATA (arr);

  Py_BEGIN_ALLOW_THREADS;
  if (sample_type == CI32)
    rc = fn_ci32 (ctx, (const int32_t *)data, (size_t)num_samples, sample_rate,
                  center_freq);
  else if (sample_type == CF64)
    rc = fn_cf64 (ctx, (const double _Complex *)data, (size_t)num_samples,
                  sample_rate, center_freq);
  else if (sample_type == CI8)
    rc = fn_ci8 (ctx, (const int8_t *)data, (size_t)num_samples, sample_rate,
                 center_freq);
  else if (sample_type == CI16)
    rc = fn_ci16 (ctx, (const int16_t *)data, (size_t)num_samples, sample_rate,
                  center_freq);
  else if (sample_type == CF32)
    rc = fn_cf32 (ctx, (const float _Complex *)data, (size_t)num_samples,
                  sample_rate, center_freq);
  else
    rc = fn_cf128 (ctx, (const long double _Complex *)data,
                   (size_t)num_samples, sample_rate, center_freq);
  Py_END_ALLOW_THREADS;

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

  dp_msg_t *msg = NULL;
  dp_header_t hdr;
  int rc;

  Py_BEGIN_ALLOW_THREADS;
  rc = fn_recv (ctx, &msg, &hdr);
  Py_END_ALLOW_THREADS;

  if (rc == DP_ERR_TIMEOUT)
    {
      PyErr_SetString (PyExc_TimeoutError, "recv timeout");
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
 * Publisher (ZMQ PUB)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_pub_t *ctx;
  int sample_type;
  int closed;
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
  int sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (sample_type < CI32 || sample_type > CF32)
    {
      PyErr_SetString (PyExc_ValueError, "Invalid sample_type");
      return NULL;
    }

  PublisherObject *self = (PublisherObject *)type->tp_alloc (type, 0);
  if (!self)
    return NULL;

  self->ctx = dp_pub_create (endpoint, (dp_sample_type_t)sample_type);
  if (!self->ctx)
    {
      Py_DECREF (self);
      PyErr_Format (PyExc_RuntimeError, "dp_pub_create failed on %s",
                    endpoint);
      return NULL;
    }

  self->sample_type = sample_type;
  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
Publisher_send (PublisherObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (self->ctx, self->sample_type, (send_ci32_fn)dp_pub_send_ci32,
                  (send_cf64_fn)dp_pub_send_cf64,
                  (send_cf128_fn)dp_pub_send_cf128,
                  (send_ci8_fn)dp_pub_send_ci8,
                  (send_ci16_fn)dp_pub_send_ci16,
                  (send_cf32_fn)dp_pub_send_cf32, args, kwds);
}

static PyObject *
Publisher_close (PublisherObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_pub_destroy (self->ctx);
      self->ctx = NULL;
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

static PyMethodDef Publisher_methods[] = {
  { "send", (PyCFunction)Publisher_send, METH_VARARGS | METH_KEYWORDS,
    "send(samples, sample_rate=0, center_freq=0)" },
  { "close", (PyCFunction)Publisher_close, METH_NOARGS,
    "close() — destroy the socket" },
  { "__enter__", (PyCFunction)Publisher_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Publisher_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PublisherType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Publisher",
  .tp_basicsize = sizeof (PublisherObject),
  .tp_dealloc = (destructor)Publisher_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Publisher(endpoint, sample_type=CF64) — ZMQ PUB socket",
  .tp_methods = Publisher_methods,
  .tp_new = Publisher_new,
};

/* =========================================================================
 * Subscriber (ZMQ SUB)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_sub_t *ctx;
  int closed;
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
      self->ctx = NULL;
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
  .tp_basicsize = sizeof (SubscriberObject),
  .tp_dealloc = (destructor)Subscriber_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Subscriber(endpoint) — ZMQ SUB socket",
  .tp_methods = Subscriber_methods,
  .tp_new = Subscriber_new,
};

/* =========================================================================
 * Push (ZMQ PUSH)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_push_t *ctx;
  int sample_type;
  int closed;
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
  int sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (sample_type < CI32 || sample_type > CF32)
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
  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
Push_send (PushObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (self->ctx, self->sample_type,
                  (send_ci32_fn)dp_push_send_ci32,
                  (send_cf64_fn)dp_push_send_cf64,
                  (send_cf128_fn)dp_push_send_cf128,
                  (send_ci8_fn)dp_push_send_ci8,
                  (send_ci16_fn)dp_push_send_ci16,
                  (send_cf32_fn)dp_push_send_cf32, args, kwds);
}

static PyObject *
Push_close (PushObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_push_destroy (self->ctx);
      self->ctx = NULL;
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
    "send(samples, sample_rate=0, center_freq=0)" },
  { "close", (PyCFunction)Push_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Push_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Push_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PushType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Push",
  .tp_basicsize = sizeof (PushObject),
  .tp_dealloc = (destructor)Push_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Push(endpoint, sample_type=CF64) — ZMQ PUSH socket",
  .tp_methods = Push_methods,
  .tp_new = Push_new,
};

/* =========================================================================
 * Pull (ZMQ PULL)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_pull_t *ctx;
  int closed;
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
Pull_close (PullObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_pull_destroy (self->ctx);
      self->ctx = NULL;
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
  { "close", (PyCFunction)Pull_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Pull_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Pull_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject PullType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Pull",
  .tp_basicsize = sizeof (PullObject),
  .tp_dealloc = (destructor)Pull_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Pull(endpoint) — ZMQ PULL socket",
  .tp_methods = Pull_methods,
  .tp_new = Pull_new,
};

/* =========================================================================
 * Requester (ZMQ REQ)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_req_t *ctx;
  int sample_type;
  int closed;
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
  int sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (sample_type < CI32 || sample_type > CF32)
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
  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
Requester_send (RequesterObject *self, PyObject *args, PyObject *kwds)
{
  return do_send (self->ctx, self->sample_type, (send_ci32_fn)dp_req_send_ci32,
                  (send_cf64_fn)dp_req_send_cf64,
                  (send_cf128_fn)dp_req_send_cf128,
                  (send_ci8_fn)dp_req_send_ci8,
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
      self->ctx = NULL;
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
    "send(samples, sample_rate=0, center_freq=0)" },
  { "recv", (PyCFunction)Requester_recv, METH_VARARGS | METH_KEYWORDS,
    "recv(timeout_ms=-1) -> (samples, header)" },
  { "close", (PyCFunction)Requester_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Requester_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Requester_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject RequesterType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Requester",
  .tp_basicsize = sizeof (RequesterObject),
  .tp_dealloc = (destructor)Requester_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Requester(endpoint, sample_type=CF64) — ZMQ REQ socket",
  .tp_methods = Requester_methods,
  .tp_new = Requester_new,
};

/* =========================================================================
 * Replier (ZMQ REP)
 * ========================================================================= */

typedef struct
{
  PyObject_HEAD dp_rep_t *ctx;
  int sample_type;
  int closed;
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
  int sample_type = CF64;

  static char *kwlist[] = { "endpoint", "sample_type", NULL };
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|i", kwlist, &endpoint,
                                    &sample_type))
    return NULL;

  if (sample_type < CI32 || sample_type > CF32)
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
  self->closed = 0;
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
                  (send_cf64_fn)dp_rep_send_cf64,
                  (send_cf128_fn)dp_rep_send_cf128,
                  (send_ci8_fn)dp_rep_send_ci8,
                  (send_ci16_fn)dp_rep_send_ci16,
                  (send_cf32_fn)dp_rep_send_cf32, args, kwds);
}

static PyObject *
Replier_close (ReplierObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->ctx)
    {
      dp_rep_destroy (self->ctx);
      self->ctx = NULL;
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
    "send(samples, sample_rate=0, center_freq=0)" },
  { "close", (PyCFunction)Replier_close, METH_NOARGS, NULL },
  { "__enter__", (PyCFunction)Replier_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Replier_exit, METH_VARARGS, NULL },
  { NULL },
};

static PyTypeObject ReplierType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "stream.Replier",
  .tp_basicsize = sizeof (ReplierObject),
  .tp_dealloc = (destructor)Replier_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = "Replier(endpoint, sample_type=CF64) — ZMQ REP socket",
  .tp_methods = Replier_methods,
  .tp_new = Replier_new,
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

/* =========================================================================
 * Module definition
 * ========================================================================= */

static PyMethodDef module_methods[] = {
  { "get_timestamp_ns", py_get_timestamp_ns, METH_NOARGS,
    "get_timestamp_ns() -> int\n"
    "Current wall-clock time in nanoseconds (CLOCK_REALTIME)." },
  { NULL },
};

static PyModuleDef stream_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "stream",
  .m_doc = "Doppler streaming — ZMQ PUB/SUB, PUSH/PULL, REQ/REP.",
  .m_size = -1,
  .m_methods = module_methods,
};

PyMODINIT_FUNC
PyInit_stream (void)
{
  import_array ();

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
  PyModule_AddIntConstant (m, "CF128", CF128);
  PyModule_AddIntConstant (m, "CI8", CI8);
  PyModule_AddIntConstant (m, "CI16", CI16);
  PyModule_AddIntConstant (m, "CF32", CF32);

  return m;
}
