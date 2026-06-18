/*
 * wfmcompose_ext.c — hand-written CPython binding for the wfmgen *transport*
 * subsystem (file writers, container readers, SigMF metadata, ZMQ sink,
 * sample-clock pacing, DSP helpers).
 *
 * This is a `no_generate` module (just-makeit only wires the CMake
 * add_subdirectory): the whole file is hand-owned, like ddc_fn_ext.c. It
 * exposes the low-level transport primitives over opaque PyCapsules.
 *
 * The composer (Synth/Segment/Timeline/Composer) and the transport handles
 * (Writer/Reader/ZmqSink/SampleClock) are now jm-generated; SigMF metadata is
 * the generated Composer.to_sigmf() serializer. The only surface still wired
 * through here is blue_write_hcb, backing the hand-Python write_blue_header()
 * (a path/string-enum I/O helper jm module functions can't yet express).
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wfm/wfm_compose.h"
#include "wfm/wfm_dsp.h"
#include "wfm/wfm_reader.h"
#include "wfm/wfm_writer.h"
#ifndef _WIN32
#include "timing/timing_core.h"
#include "wfm/wfm_sink.h"
#endif

/* ───────────────────────── writer capsule ─────────────────────────────────
 */

static const char _WR_CAPS[] = "doppler.wfm.compose.writer";

typedef struct
{
  wfm_writer_t *w;
  FILE         *fp;
  int           closed;
  double        peak;     /* snapshot at close (writer is freed there) */
  double        clipfrac; /* snapshot at close */
} _wr_wrap_t;

static void
_wr_destructor (PyObject *cap)
{
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return;
  if (!p->closed)
    {
      wfm_writer_close (p->w);
      fclose (p->fp);
    }
  free (p);
}

static PyObject *
_fn_writer_open (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *path;
  int         ft, st, endian;
  double      fs, fc;
  Py_ssize_t  total;
  if (!PyArg_ParseTuple (args, "siiiddn", &path, &ft, &st, &endian, &fs, &fc,
                         &total))
    return NULL;
  FILE *fp = fopen (path, "wb");
  if (!fp)
    {
      PyErr_SetFromErrnoWithFilename (PyExc_OSError, path);
      return NULL;
    }
  wfm_writer_t *w = wfm_writer_open (fp, (wfm_filetype_t)ft, st, endian, fs,
                                     fc, (size_t)total);
  if (!w)
    {
      fclose (fp);
      PyErr_SetString (PyExc_RuntimeError, "wfm_writer_open failed");
      return NULL;
    }
  _wr_wrap_t *p = (_wr_wrap_t *)malloc (sizeof *p);
  if (!p)
    {
      wfm_writer_close (w);
      fclose (fp);
      return PyErr_NoMemory ();
    }
  p->w          = w;
  p->fp         = fp;
  p->closed     = 0;
  p->peak       = 0.0;
  p->clipfrac   = 0.0;
  PyObject *cap = PyCapsule_New (p, _WR_CAPS, _wr_destructor);
  if (!cap)
    {
      wfm_writer_close (w);
      fclose (fp);
      free (p);
      return NULL;
    }
  return cap;
}

static PyObject *
_fn_writer_write (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap, *iq_obj;
  if (!PyArg_ParseTuple (args, "OO", &cap, &iq_obj))
    return NULL;
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return NULL;
  if (p->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "writer already closed");
      return NULL;
    }
  PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_OTF (
      iq_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!arr)
    return NULL;
  size_t          n  = (size_t)PyArray_SIZE (arr);
  float _Complex *iq = (float _Complex *)PyArray_DATA (arr);
  size_t          wrote;
  Py_BEGIN_ALLOW_THREADS
    wrote = wfm_writer_write (p->w, iq, n);
  Py_END_ALLOW_THREADS
  Py_DECREF (arr);
  return PyLong_FromSize_t (wrote);
}

static PyObject *
_fn_writer_close (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return NULL;
  if (!p->closed)
    {
      p->peak     = wfm_writer_peak (p->w);
      p->clipfrac = wfm_writer_clip_fraction (p->w);
      int rc      = wfm_writer_close (p->w);
      fclose (p->fp);
      p->closed = 1;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_RuntimeError, "wfm_writer_close failed");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
_fn_writer_track_clipping (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  int       on;
  if (!PyArg_ParseTuple (args, "Op", &cap, &on))
    return NULL;
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return NULL;
  if (!p->closed)
    wfm_writer_track_clipping (p->w, on);
  Py_RETURN_NONE;
}

static PyObject *
_fn_writer_set_gain (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  double    gain;
  if (!PyArg_ParseTuple (args, "Od", &cap, &gain))
    return NULL;
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return NULL;
  if (!p->closed)
    wfm_writer_set_gain (p->w, gain);
  Py_RETURN_NONE;
}

/* (peak, clip_fraction): the live writer's values, or the close-time snapshot
 * once closed (the writer is freed at close). */
static PyObject *
_fn_writer_stats (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _wr_wrap_t *p = (_wr_wrap_t *)PyCapsule_GetPointer (cap, _WR_CAPS);
  if (!p)
    return NULL;
  double peak = p->closed ? p->peak : wfm_writer_peak (p->w);
  double frac = p->closed ? p->clipfrac : wfm_writer_clip_fraction (p->w);
  return Py_BuildValue ("(dd)", peak, frac);
}

static PyObject *
_fn_blue_write_hcb (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *path;
  int         st, endian, detached;
  double      fs, fc, data_start;
  Py_ssize_t  total;
  if (!PyArg_ParseTuple (args, "siidddnp", &path, &st, &endian, &fs, &fc,
                         &data_start, &total, &detached))
    return NULL;
  FILE *fp = fopen (path, "wb");
  if (!fp)
    {
      PyErr_SetFromErrnoWithFilename (PyExc_OSError, path);
      return NULL;
    }
  int rc = wfm_blue_write_hcb (fp, st, endian, fs, fc, data_start,
                               (size_t)total, detached);
  fclose (fp);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_RuntimeError, "wfm_blue_write_hcb failed");
      return NULL;
    }
  Py_RETURN_NONE;
}

/* ─────────────────────────── reader capsule ───────────────────────────────
 *
 * Wraps wfm_reader_t — the dual of the writer. Container detection, header
 * parsing and the wire→unit conversion all live in C; this is pure binding.
 */

static const char _READER_CAPS[] = "doppler.wfm.compose.reader";

typedef struct
{
  wfm_reader_t *reader;
  int           closed;
} _reader_wrap_t;

static void
_reader_destructor (PyObject *cap)
{
  _reader_wrap_t *p
      = (_reader_wrap_t *)PyCapsule_GetPointer (cap, _READER_CAPS);
  if (!p)
    return;
  if (!p->closed)
    wfm_reader_close (p->reader);
  free (p);
}

static PyObject *
_fn_reader_open (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *path;
  int         hint_stype, hint_endian;
  if (!PyArg_ParseTuple (args, "sii", &path, &hint_stype, &hint_endian))
    return NULL;
  wfm_reader_t *r = wfm_reader_open (path, hint_stype, hint_endian);
  if (!r)
    {
      PyErr_Format (PyExc_OSError, "cannot open capture %s", path);
      return NULL;
    }
  _reader_wrap_t *p = (_reader_wrap_t *)malloc (sizeof *p);
  if (!p)
    {
      wfm_reader_close (r);
      return PyErr_NoMemory ();
    }
  p->reader     = r;
  p->closed     = 0;
  PyObject *cap = PyCapsule_New (p, _READER_CAPS, _reader_destructor);
  if (!cap)
    {
      wfm_reader_close (r);
      free (p);
      return NULL;
    }
  return cap;
}

static _reader_wrap_t *
_reader_get (PyObject *cap)
{
  _reader_wrap_t *p
      = (_reader_wrap_t *)PyCapsule_GetPointer (cap, _READER_CAPS);
  if (!p)
    return NULL;
  if (p->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "reader already closed");
      return NULL;
    }
  return p;
}

static PyObject *
_fn_reader_info (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _reader_wrap_t *p = _reader_get (cap);
  if (!p)
    return NULL;
  wfm_reader_info_t info;
  wfm_reader_info (p->reader, &info);
  return Py_BuildValue ("iiiddK", info.file_type, info.sample_type,
                        info.endian, info.fs, info.fc,
                        (unsigned long long)info.num_samples);
}

static PyObject *
_fn_reader_read (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject  *cap;
  Py_ssize_t max;
  if (!PyArg_ParseTuple (args, "On", &cap, &max))
    return NULL;
  _reader_wrap_t *p = _reader_get (cap);
  if (!p)
    return NULL;
  if (max < 0)
    {
      PyErr_SetString (PyExc_ValueError, "max must be >= 0");
      return NULL;
    }
  npy_intp  dims[] = { max };
  PyObject *arr    = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!arr)
    return NULL;
  float _Complex *out = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr);
  size_t          n;
  Py_BEGIN_ALLOW_THREADS
    n = wfm_reader_read (p->reader, out, (size_t)max);
  Py_END_ALLOW_THREADS

  PyObject *stop  = PyLong_FromSsize_t ((Py_ssize_t)n);
  PyObject *slice = stop ? PySlice_New (NULL, stop, NULL) : NULL;
  Py_XDECREF (stop);
  PyObject *view = slice ? PyObject_GetItem (arr, slice) : NULL;
  Py_XDECREF (slice);
  Py_DECREF (arr);
  return view;
}

static PyObject *
_fn_reader_close (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _reader_wrap_t *p
      = (_reader_wrap_t *)PyCapsule_GetPointer (cap, _READER_CAPS);
  if (!p)
    return NULL;
  if (!p->closed)
    {
      wfm_reader_close (p->reader);
      p->closed = 1;
    }
  Py_RETURN_NONE;
}

/* ───────────────────────── ZMQ sink capsule (POSIX) ───────────────────────
 */

#ifndef _WIN32
static const char _SINK_CAPS[] = "doppler.wfm.compose.sink";

typedef struct
{
  wfm_zmq_sink_t *sink;
  int             closed;
} _sink_wrap_t;

static void
_sink_destructor (PyObject *cap)
{
  _sink_wrap_t *p = (_sink_wrap_t *)PyCapsule_GetPointer (cap, _SINK_CAPS);
  if (!p)
    return;
  if (!p->closed)
    wfm_zmq_sink_close (p->sink);
  free (p);
}

static PyObject *
_fn_sink_open (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *endpoint;
  int         st;
  if (!PyArg_ParseTuple (args, "si", &endpoint, &st))
    return NULL;
  wfm_zmq_sink_t *sink = wfm_zmq_sink_open (endpoint, st);
  if (!sink)
    {
      PyErr_SetString (PyExc_RuntimeError, "wfm_zmq_sink_open failed");
      return NULL;
    }
  _sink_wrap_t *p = (_sink_wrap_t *)malloc (sizeof *p);
  if (!p)
    {
      wfm_zmq_sink_close (sink);
      return PyErr_NoMemory ();
    }
  p->sink       = sink;
  p->closed     = 0;
  PyObject *cap = PyCapsule_New (p, _SINK_CAPS, _sink_destructor);
  if (!cap)
    {
      wfm_zmq_sink_close (sink);
      free (p);
      return NULL;
    }
  return cap;
}

static PyObject *
_fn_sink_send (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap, *iq_obj;
  double    fs, fc;
  if (!PyArg_ParseTuple (args, "OOdd", &cap, &iq_obj, &fs, &fc))
    return NULL;
  _sink_wrap_t *p = (_sink_wrap_t *)PyCapsule_GetPointer (cap, _SINK_CAPS);
  if (!p)
    return NULL;
  if (p->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "sink already closed");
      return NULL;
    }
  PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_OTF (
      iq_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!arr)
    return NULL;
  size_t          n  = (size_t)PyArray_SIZE (arr);
  float _Complex *iq = (float _Complex *)PyArray_DATA (arr);
  int             rc;
  Py_BEGIN_ALLOW_THREADS
    rc = wfm_zmq_sink_send (p->sink, iq, n, fs, fc);
  Py_END_ALLOW_THREADS
  Py_DECREF (arr);
  if (rc != 0)
    {
      PyErr_SetString (PyExc_RuntimeError, "wfm_zmq_sink_send failed");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
_fn_sink_close (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _sink_wrap_t *p = (_sink_wrap_t *)PyCapsule_GetPointer (cap, _SINK_CAPS);
  if (!p)
    return NULL;
  if (!p->closed)
    {
      wfm_zmq_sink_close (p->sink);
      p->closed = 1;
    }
  Py_RETURN_NONE;
}

/* ─────────────────────── sample-clock capsule (POSIX) ─────────────────────
 *
 * Wraps dp_sample_clock_t. clock_pace releases the GIL around the sleep so a
 * paced producer thread does not stall the interpreter.
 */

static const char _CLOCK_CAPS[] = "doppler.wfm.compose.clock";

static void
_clock_destructor (PyObject *cap)
{
  void *p = PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  free (p);
}

static PyObject *
_fn_clock_create (PyObject *mod, PyObject *args)
{
  (void)mod;
  double fs;
  int    resync;
  if (!PyArg_ParseTuple (args, "dp", &fs, &resync))
    return NULL;
  if (!(fs > 0.0))
    {
      PyErr_SetString (PyExc_ValueError, "fs must be > 0");
      return NULL;
    }
  dp_sample_clock_t *c = (dp_sample_clock_t *)malloc (sizeof *c);
  if (!c)
    return PyErr_NoMemory ();
  dp_sample_clock_init (c, fs, resync);
  PyObject *cap = PyCapsule_New (c, _CLOCK_CAPS, _clock_destructor);
  if (!cap)
    {
      free (c);
      return NULL;
    }
  return cap;
}

static PyObject *
_fn_clock_pace (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject          *cap;
  unsigned long long count;
  if (!PyArg_ParseTuple (args, "OK", &cap, &count))
    return NULL;
  dp_sample_clock_t *c
      = (dp_sample_clock_t *)PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  if (!c)
    return NULL;
  double slack;
  Py_BEGIN_ALLOW_THREADS
    slack = dp_sample_clock_pace (c, (size_t)count);
  Py_END_ALLOW_THREADS
  return PyFloat_FromDouble (slack);
}

static PyObject *
_fn_clock_stamp (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  dp_sample_clock_t *c
      = (dp_sample_clock_t *)PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  if (!c)
    return NULL;
  return PyLong_FromUnsignedLongLong (dp_sample_clock_stamp (c));
}

static PyObject *
_fn_clock_reset (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  dp_sample_clock_t *c
      = (dp_sample_clock_t *)PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  if (!c)
    return NULL;
  dp_sample_clock_reset (c);
  Py_RETURN_NONE;
}

static PyObject *
_fn_clock_resync (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  dp_sample_clock_t *c
      = (dp_sample_clock_t *)PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  if (!c)
    return NULL;
  dp_sample_clock_resync (c);
  Py_RETURN_NONE;
}

/* (n, underruns, max_late_ns) — backs the Python read-only properties. */
static PyObject *
_fn_clock_stats (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  dp_sample_clock_t *c
      = (dp_sample_clock_t *)PyCapsule_GetPointer (cap, _CLOCK_CAPS);
  if (!c)
    return NULL;
  return Py_BuildValue ("KKK", (unsigned long long)c->n,
                        (unsigned long long)c->underruns,
                        (unsigned long long)c->max_late_ns);
}
#endif /* !_WIN32 */

/* rrc_taps / dsss_spread migrated to generated `variable_output` wfm module
 * functions (native/src/wfm/{rrc_taps,dsss_spread}.c over the wfm_dsp
 * kernels). */

/* ───────────────────────── module table ───────────────────────────────────
 */

static PyMethodDef _methods[] = {
  { "writer_open", _fn_writer_open, METH_VARARGS,
    "writer_open(path, file_type, sample_type, endian, fs, fc, total)"
    " -> capsule" },
  { "writer_write", _fn_writer_write, METH_VARARGS,
    "writer_write(state, iq) -> int" },
  { "writer_close", _fn_writer_close, METH_VARARGS,
    "writer_close(state) -> None" },
  { "writer_track_clipping", _fn_writer_track_clipping, METH_VARARGS,
    "writer_track_clipping(state, on) -> None" },
  { "writer_set_gain", _fn_writer_set_gain, METH_VARARGS,
    "writer_set_gain(state, gain) -> None" },
  { "writer_stats", _fn_writer_stats, METH_VARARGS,
    "writer_stats(state) -> (peak, clip_fraction)" },
  { "blue_write_hcb", _fn_blue_write_hcb, METH_VARARGS,
    "blue_write_hcb(path, sample_type, endian, fs, fc, data_start, total,"
    " detached) -> None" },
  { "reader_open", _fn_reader_open, METH_VARARGS,
    "reader_open(path, hint_sample_type, hint_endian) -> capsule" },
  { "reader_info", _fn_reader_info, METH_VARARGS,
    "reader_info(state) -> (file_type, sample_type, endian, fs, fc, num)" },
  { "reader_read", _fn_reader_read, METH_VARARGS,
    "reader_read(state, max) -> ndarray[complex64]" },
  { "reader_close", _fn_reader_close, METH_VARARGS,
    "reader_close(state) -> None" },
#ifndef _WIN32
  { "sink_open", _fn_sink_open, METH_VARARGS,
    "sink_open(endpoint, sample_type) -> capsule" },
  { "sink_send", _fn_sink_send, METH_VARARGS,
    "sink_send(state, iq, fs, fc) -> None" },
  { "sink_close", _fn_sink_close, METH_VARARGS, "sink_close(state) -> None" },
  { "clock_create", _fn_clock_create, METH_VARARGS,
    "clock_create(fs, resync) -> capsule" },
  { "clock_pace", _fn_clock_pace, METH_VARARGS,
    "clock_pace(state, count) -> float slack_s" },
  { "clock_stamp", _fn_clock_stamp, METH_VARARGS,
    "clock_stamp(state) -> int ns" },
  { "clock_reset", _fn_clock_reset, METH_VARARGS,
    "clock_reset(state) -> None" },
  { "clock_resync", _fn_clock_resync, METH_VARARGS,
    "clock_resync(state) -> None" },
  { "clock_stats", _fn_clock_stats, METH_VARARGS,
    "clock_stats(state) -> (n, underruns, max_late_ns)" },
#endif
  { NULL, NULL, 0, NULL },
};

static PyModuleDef _moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name    = "_wfmcompose",
  .m_doc     = "Low-level binding for the wfmgen composer subsystem.",
  .m_size    = -1,
  .m_methods = _methods,
};

PyMODINIT_FUNC
PyInit__wfmcompose (void)
{
  import_array ();
  return PyModule_Create (&_moduledef);
}
