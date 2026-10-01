/*
 * wfm_sink_ext.c — handle extension: typed `StreamSink` over `wfm_stream_sink`
 * (jm; gh-306).
 *
 * `StreamSink` wraps an opaque wfm_stream_sink_t *; the resource logic
 * lives hand-written in the backing _core.c. This file is pure generated glue
 * — lifecycle, arg coercion, numpy marshaling, decoded-getter properties,
 * RAII.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "doppler/clib_common.h"
#include <math.h>
#include <numpy/arrayobject.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "doppler/wfm/wfm_sink.h"

#ifndef JM_ARRAY_ARG_DEFINED
#define JM_ARRAY_ARG_DEFINED
/* Convert a Python argument for an array parameter to an ndarray of
 * `typenum` meeting `requirements` -- PyArray_FROM_OTF, less the two inputs
 * it reads as text (gh-1700): a str is refused, never parsed as a number,
 * and for a one-byte element type a byte buffer (bytes, bytearray,
 * memoryview) is its bytes, one element per byte. `name` is the parameter,
 * for the message, and `hint` (NULL for none) is appended to a str's
 * refusal. Returns a new reference, or NULL with an exception. */
static inline PyArrayObject *
jm_array_arg_hint (PyObject *obj, int typenum, int requirements,
                   const char *name, const char *hint)
{
  int one_byte = typenum == NPY_UINT8 || typenum == NPY_INT8;
  if (PyUnicode_Check (obj) || (!one_byte && PyBytes_Check (obj)))
    {
      /* `hint` (gh-1756) says where text goes instead: a str only. */
      int say = hint && PyUnicode_Check (obj);
      PyErr_Format (PyExc_TypeError,
                    "%s must be an array of numbers, not %.200s%s%s", name,
                    Py_TYPE (obj)->tp_name, say ? ": " : "", say ? hint : "");
      return NULL;
    }
  if (one_byte && !PyArray_Check (obj) && PyObject_CheckBuffer (obj))
    {
      PyObject *view = PyMemoryView_FromObject (obj);
      if (!view)
        return NULL;
      if (PyMemoryView_GET_BUFFER (view)->itemsize == 1)
        {
          PyObject *raw = PyArray_FromBuffer (
              view, PyArray_DescrFromType (typenum), -1, 0);
          Py_DECREF (view);
          if (!raw)
            return NULL;
          PyObject *arr = PyArray_FROM_OTF (raw, typenum, requirements);
          Py_DECREF (raw);
          return (PyArrayObject *)arr;
        }
      Py_DECREF (view);
    }
  return (PyArrayObject *)PyArray_FROM_OTF (obj, typenum, requirements);
}
static inline PyArrayObject *
jm_array_arg (PyObject *obj, int typenum, int requirements, const char *name)
{
  return jm_array_arg_hint (obj, typenum, requirements, name, NULL);
}
#endif /* JM_ARRAY_ARG_DEFINED */

/* String-enum tables — order is the C int (the [[enum]] SSOT). */
static int
_enum_index (const char *const *tab, const char *s)
{
  for (int i = 0; tab[i]; i++)
    if (strcmp (tab[i], s) == 0)
      return i;
  return -1;
}

static const char *const _enum_stype[] = {
  "cf32", "cf64", "ci32", "ci16", "ci8", "f32",
  "f64",  "i32",  "i16",  "i8",   NULL,
};

typedef struct
{
  PyObject_HEAD wfm_stream_sink_t *h;
  int                              closed;
  int                              sample_type;
} StreamSinkObject;

static int
StreamSink_init (StreamSinkObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]    = { "endpoint", "sample_type", NULL };
  const char  *endpoint    = 0;
  const char  *sample_type = "cf32";
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s|s", kwlist, &endpoint,
                                    &sample_type))
    {
      return -1;
    }
  int _arg_sample_type = _enum_index (_enum_stype, sample_type);
  if (_arg_sample_type < 0)
    {
      PyErr_Format (PyExc_ValueError,
                    "invalid sample_type '%s' (choices: cf32, cf64, ci32, "
                    "ci16, ci8, f32, f64, i32, i16, i8)",
                    sample_type);
      return -1;
    }
  if (!self->closed && self->h)
    {
      dp_wfm_stream_sink_close (self->h);
      self->h      = NULL;
      self->closed = 1;
    }
  self->h = dp_wfm_stream_sink_open (endpoint, _arg_sample_type);
  if (!self->h)
    {
      PyErr_SetString (PyExc_RuntimeError, "dp_wfm_stream_sink_open failed");
      return -1;
    }
  self->closed      = 0;
  self->sample_type = _arg_sample_type;

  return 0;
}

static PyObject *
StreamSink_send (StreamSinkObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "iq", "fs", "fc", NULL };
  PyObject    *x_obj;
  double       fs;
  double       fc;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "Odd", kwlist, &x_obj, &fs,
                                    &fc))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  PyArrayObject *x_arr
      = jm_array_arg (x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS, "iq");
  if (!x_arr)
    return NULL;
  size_t                n_in    = (size_t)PyArray_SIZE (x_arr);
  const float _Complex *in_data = (const float _Complex *)PyArray_DATA (x_arr);
  int                   r;
  Py_BEGIN_ALLOW_THREADS
    r = dp_wfm_stream_sink_send (self->h, in_data, n_in, fs, fc);
  Py_END_ALLOW_THREADS
  Py_DECREF (x_arr);
  return PyLong_FromLong ((long)r);
}

static PyObject *
StreamSink_send_eos (StreamSinkObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  int _rc;
  _rc = dp_wfm_stream_sink_send_eos (self->h);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "dp_wfm_stream_sink_send_eos failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
StreamSink_drain (StreamSinkObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]   = { "timeout_ms", NULL };
  int          timeout_ms = 0;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", kwlist, &timeout_ms))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  int _rc;
  _rc = dp_wfm_stream_sink_drain (self->h, timeout_ms);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)",
                    "dp_wfm_stream_sink_drain failed", (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
StreamSink_track_clipping (StreamSinkObject *self, PyObject *args,
                           PyObject *kwds)
{
  static char *kwlist[] = { "on", NULL };
  int          on       = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", kwlist, &on))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  dp_wfm_stream_sink_track_clipping (self->h, on);
  Py_RETURN_NONE;
}

static PyObject *
StreamSink_get_clip_fraction (StreamSinkObject *self, void *closure)
{
  (void)closure;
  double tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  tmp = dp_wfm_stream_sink_clip_fraction (self->h);
  return PyFloat_FromDouble (tmp);
}

static PyObject *
StreamSink_get_peak_dbfs (StreamSinkObject *self, void *closure)
{
  (void)closure;
  double tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  tmp = dp_wfm_stream_sink_peak (self->h);
  return PyFloat_FromDouble (tmp > 0 ? 20 * log10 (tmp) : -INFINITY);
}

static PyObject *
StreamSink_get_clipped (StreamSinkObject *self, void *closure)
{
  (void)closure;
  double tmp;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "StreamSink is closed");
      return NULL;
    }
  tmp = dp_wfm_stream_sink_peak (self->h);
  return PyBool_FromLong ((long)(tmp > 1.0 && self->sample_type >= 2));
}

static PyGetSetDef StreamSink_getset[]
    = { { "clip_fraction", (getter)StreamSink_get_clip_fraction, NULL, NULL,
          NULL },
        { "peak_dbfs", (getter)StreamSink_get_peak_dbfs, NULL, NULL, NULL },
        { "clipped", (getter)StreamSink_get_clipped, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
StreamSink_close (StreamSinkObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->h)
    {
      dp_wfm_stream_sink_close (self->h);
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
StreamSink_enter (StreamSinkObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
StreamSink_exit (StreamSinkObject *self, PyObject *args)
{
  (void)args;
  return StreamSink_close (self, NULL);
}
static void
StreamSink_dealloc (StreamSinkObject *self)
{
  if (!self->closed && self->h)
    {
      dp_wfm_stream_sink_close (self->h);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyMethodDef StreamSink_methods[] = {
  { "send", (PyCFunction)StreamSink_send, METH_VARARGS | METH_KEYWORDS,
    "Convert a cf32 block to the wire type and publish it.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "iq : NDArray[Any]\n"
    "    Complex-float samples; @param n complex sample count.\n"
    "fs : float\n"
    "    sample rate (Hz); @param fc center frequency (Hz) — wire header.\n"
    "fc : float\n"
    "    the sink handle.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    0 on success, non-zero on a send/allocation error.\n" },
  { "send_eos", (PyCFunction)StreamSink_send_eos, METH_VARARGS,
    "Tell subscribers this stream has ended.\n"
    "\n"
    "Publishes an end-of-stream frame, so a consumer learns the sender\n"
    "finished rather than inferring it from silence. Send it BEFORE\n"
    "draining: a drain cannot be reversed and refuses sends once it reaches\n"
    "its publish-flushing phase.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``dp_wfm_stream_sink_send_eos failed``, with the return code\n"
    "    appended (gh-869).\n" },
  { "drain", (PyCFunction)StreamSink_drain, METH_VARARGS | METH_KEYWORDS,
    "Let everything already sent reach the server, then stop.\n"
    "\n"
    "A send hands a block to the NATS client and returns; the client writes\n"
    "it in the background. So \"send returned\" is not \"the server has "
    "it\",\n"
    "and closing without asking relies on the client's own best-effort flush\n"
    "-- capped at 500 ms, with no way to report failure, so a backlog that\n"
    "cannot clear in half a second is dropped silently.\n"
    "\n"
    "Call this before closing the sink on any run whose tail matters. After\n"
    "it returns the sink is finished: close it next, which is then just the\n"
    "free.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "timeout_ms : int\n"
    "    Budget; <= 0 uses the stream layer's 5 s default.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``dp_wfm_stream_sink_drain failed``, with the return code appended\n"
    "    (gh-869).\n" },
  { "track_clipping", (PyCFunction)StreamSink_track_clipping,
    METH_VARARGS | METH_KEYWORDS,
    "Enable the per-component clip counter (off by default; peak always\n"
    "on).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "on : int\n"
    "    Input.\n" },
  { "close", (PyCFunction)StreamSink_close, METH_NOARGS,
    "Release the handle and free resources." },
  { "__enter__", (PyCFunction)StreamSink_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)StreamSink_exit, METH_VARARGS, NULL },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject StreamSinkType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wfm.StreamSink",
  .tp_basicsize                           = sizeof (StreamSinkObject),
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_new                                 = PyType_GenericNew,
  .tp_init                                = (initproc)StreamSink_init,
  .tp_dealloc                             = (destructor)StreamSink_dealloc,
  .tp_getset                              = StreamSink_getset,
  .tp_methods                             = StreamSink_methods,
  .tp_doc                                 = PyDoc_STR (
      "Open a stream sink (PUB) bound to a NATS subject.\n"
      "\n"
      "Parameters\n"
      "----------\n"
      "endpoint : int\n"
      "    Endpoint, e.g. \"nats://127.0.0.1:4222/iq\".\n"
      "sample_type : str, default ``\"cf32\"``\n"
      "    Wire type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8.\n"
      "    Integer types use full-scale ±1.0.\n"
      "    One of ``\"cf32\"``, ``\"cf64\"``, ``\"ci32\"``, ``\"ci16\"``, "
      "``\"ci8\"``,\n"
      "    ``\"f32\"``, ``\"f64\"``, ``\"i32\"``, ``\"i16\"``, ``\"i8\"``.\n"),
};

static struct PyModuleDef _moduledef = {
  PyModuleDef_HEAD_INIT, "wfm_sink", NULL, -1, NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_wfm_sink (void)
{
  import_array ();
  if (PyType_Ready (&StreamSinkType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&StreamSinkType);
  if (PyModule_AddObject (m, "StreamSink", (PyObject *)&StreamSinkType) < 0)
    {
      Py_DECREF (&StreamSinkType);
      Py_DECREF (m);
      return NULL;
    }
  /* gh-1117: adopt dp_interrupt_guard's process-global state from its owner.
   */
  {
    void     *dp_interrupt_guard_state_ptr (void);
    void      dp_interrupt_guard_state_adopt (void *shared);
    PyObject *_own = PyImport_ImportModule ("doppler.interrupt.interrupt");
    if (!_own)
      {
        Py_DECREF (m);
        return NULL;
      }
    PyObject *_pg = PyObject_GetAttrString (_own, "_jm_pg_dp_interrupt_guard");
    Py_DECREF (_own);
    if (!_pg)
      {
        Py_DECREF (m);
        return NULL;
      }
    void *_p = PyCapsule_GetPointer (
        _pg, "doppler.dp_interrupt_guard._jm_procglobal");
    Py_DECREF (_pg);
    if (!_p)
      {
        Py_DECREF (m);
        return NULL;
      }
    dp_interrupt_guard_state_adopt (_p);
  }
  return m;
}
