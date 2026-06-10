/*
 * wfmcompose_ext.c — hand-written CPython binding for the wfmgen composer
 * subsystem (multi-segment composition, file writers, ZMQ sink, DSP helpers).
 *
 * This is a `no_generate` module (just-makeit only wires the CMake
 * add_subdirectory): the whole file is hand-owned, like ddc_fn_ext.c. It
 * exposes the low-level primitives over opaque PyCapsules; the ergonomic
 * Segment/Composer/Writer/ZmqSink API lives in src/doppler/wfmgen/compose.py.
 *
 * Segments cross the boundary as fixed-order 12-tuples (see _SEG_FMT) built by
 * the Python wrapper from its Segment dataclass — keeps parsing trivial and
 * the field order in one place.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

#include "synth/synth_core.h" /* synth_mls_poly */
#include "wfmgen/wfm_compose.h"
#include "wfmgen/wfm_dsp.h"
#include "wfmgen/wfm_reader.h"
#include "wfmgen/wfm_writer.h"
#ifndef _WIN32
#include "timing/timing_core.h"
#include "wfmgen/wfm_sink.h"
#endif

/* type fs freq snr snr_mode seed sps pn_length pn_poly lfsr num off */
#define _SEG_FMT "idddiIiiKinn"

/* Parse a Python list of 12-tuples into a malloc'd wfm_segment_t[].
 * Returns the array (caller frees) and writes the count to *n_out, or NULL on
 * error (with a Python exception set). */
static wfm_segment_t *
_parse_segments (PyObject *list, size_t *n_out)
{
  if (!PyList_Check (list))
    {
      PyErr_SetString (PyExc_TypeError, "segments must be a list");
      return NULL;
    }
  Py_ssize_t n = PyList_GET_SIZE (list);
  if (n < 1)
    {
      PyErr_SetString (PyExc_ValueError, "need at least one segment");
      return NULL;
    }
  wfm_segment_t *segs = (wfm_segment_t *)calloc ((size_t)n, sizeof *segs);
  if (!segs)
    return (wfm_segment_t *)PyErr_NoMemory ();

  for (Py_ssize_t i = 0; i < n; i++)
    {
      PyObject      *t = PyList_GET_ITEM (list, i);
      wfm_segment_t *s = &segs[i];
      if (!PyArg_ParseTuple (t, _SEG_FMT, &s->type, &s->fs, &s->freq, &s->snr,
                             &s->snr_mode, &s->seed, &s->sps, &s->pn_length,
                             &s->pn_poly, &s->lfsr, &s->num_samples,
                             &s->off_samples))
        {
          free (segs);
          return NULL;
        }
    }
  *n_out = (size_t)n;
  return segs;
}

/* Build a Python list of 12-tuples from a wfm_segment_t[]. */
static PyObject *
_segments_to_list (const wfm_segment_t *segs, size_t n)
{
  PyObject *list = PyList_New ((Py_ssize_t)n);
  if (!list)
    return NULL;
  for (size_t i = 0; i < n; i++)
    {
      const wfm_segment_t *s = &segs[i];
      PyObject            *t = Py_BuildValue (
          "(" _SEG_FMT ")", s->type, s->fs, s->freq, s->snr, s->snr_mode,
          s->seed, s->sps, s->pn_length, s->pn_poly, s->lfsr,
          (Py_ssize_t)s->num_samples, (Py_ssize_t)s->off_samples);
      if (!t)
        {
          Py_DECREF (list);
          return NULL;
        }
      PyList_SET_ITEM (list, (Py_ssize_t)i, t); /* steals ref */
    }
  return list;
}

/* ───────────────────────── composer capsule ───────────────────────────────
 */

static const char _COMP_CAPS[] = "doppler.wfmgen.compose.composer";

typedef struct
{
  wfm_compose_state_t *state;
  int                  destroyed;
} _comp_wrap_t;

static void
_comp_destructor (PyObject *cap)
{
  _comp_wrap_t *w = (_comp_wrap_t *)PyCapsule_GetPointer (cap, _COMP_CAPS);
  if (!w)
    return;
  if (!w->destroyed)
    wfm_compose_destroy (w->state);
  free (w);
}

static _comp_wrap_t *
_comp_get (PyObject *cap)
{
  _comp_wrap_t *w = (_comp_wrap_t *)PyCapsule_GetPointer (cap, _COMP_CAPS);
  if (!w)
    return NULL;
  if (w->destroyed)
    {
      PyErr_SetString (PyExc_RuntimeError, "composer already destroyed");
      return NULL;
    }
  return w;
}

static PyObject *
_wrap_compose (wfm_compose_state_t *state)
{
  if (!state)
    return PyErr_NoMemory ();
  _comp_wrap_t *w = (_comp_wrap_t *)malloc (sizeof *w);
  if (!w)
    {
      wfm_compose_destroy (state);
      return PyErr_NoMemory ();
    }
  w->state      = state;
  w->destroyed  = 0;
  PyObject *cap = PyCapsule_New (w, _COMP_CAPS, _comp_destructor);
  if (!cap)
    {
      wfm_compose_destroy (state);
      free (w);
      return NULL;
    }
  return cap;
}

static PyObject *
_fn_composer_create (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *seg_list;
  int       repeat = 0, continuous = 0;
  if (!PyArg_ParseTuple (args, "Opp", &seg_list, &repeat, &continuous))
    return NULL;
  size_t         n_segs;
  wfm_segment_t *segs = _parse_segments (seg_list, &n_segs);
  if (!segs)
    return NULL;
  wfm_compose_state_t *st
      = wfm_compose_create (segs, n_segs, repeat, continuous);
  free (segs);
  return _wrap_compose (st);
}

static PyObject *
_fn_composer_from_json (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *json;
  if (!PyArg_ParseTuple (args, "s", &json))
    return NULL;
  return _wrap_compose (wfm_compose_from_json (json));
}

static PyObject *
_fn_composer_from_file (PyObject *mod, PyObject *args)
{
  (void)mod;
  const char *path;
  if (!PyArg_ParseTuple (args, "s", &path))
    return NULL;
  return _wrap_compose (wfm_compose_from_file (path));
}

static PyObject *
_fn_composer_execute (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject  *cap;
  Py_ssize_t max;
  if (!PyArg_ParseTuple (args, "On", &cap, &max))
    return NULL;
  _comp_wrap_t *w = _comp_get (cap);
  if (!w)
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
    n = wfm_compose_execute (w->state, out, (size_t)max);
  Py_END_ALLOW_THREADS

  /* return out[:n] */
  PyObject *stop  = PyLong_FromSsize_t ((Py_ssize_t)n);
  PyObject *slice = stop ? PySlice_New (NULL, stop, NULL) : NULL;
  Py_XDECREF (stop);
  PyObject *view = slice ? PyObject_GetItem (arr, slice) : NULL;
  Py_XDECREF (slice);
  Py_DECREF (arr);
  return view;
}

static PyObject *
_fn_composer_segments (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _comp_wrap_t *w = _comp_get (cap);
  if (!w)
    return NULL;
  size_t               n;
  int                  repeat, continuous;
  const wfm_segment_t *segs
      = wfm_compose_segments (w->state, &n, &repeat, &continuous);
  PyObject *list = _segments_to_list (segs, n);
  if (!list)
    return NULL;
  /* Py_BuildValue has no 'p' (bool) code — that's parse-only; build the
   * bools explicitly with 'O' (which INCREFs the immortal Py_True/False). */
  return Py_BuildValue ("(NOO)", list, repeat ? Py_True : Py_False,
                        continuous ? Py_True : Py_False);
}

static PyObject *
_fn_composer_destroy (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *cap;
  if (!PyArg_ParseTuple (args, "O", &cap))
    return NULL;
  _comp_wrap_t *w = (_comp_wrap_t *)PyCapsule_GetPointer (cap, _COMP_CAPS);
  if (!w)
    return NULL;
  if (!w->destroyed)
    {
      wfm_compose_destroy (w->state);
      w->destroyed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
_fn_spec_to_json (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject *seg_list;
  int       repeat = 0, continuous = 0;
  if (!PyArg_ParseTuple (args, "Opp", &seg_list, &repeat, &continuous))
    return NULL;
  size_t         n_segs;
  wfm_segment_t *segs = _parse_segments (seg_list, &n_segs);
  if (!segs)
    return NULL;
  char *json = wfm_spec_to_json (segs, n_segs, repeat, continuous);
  free (segs);
  if (!json)
    {
      PyErr_SetString (PyExc_RuntimeError, "wfm_spec_to_json failed");
      return NULL;
    }
  PyObject *s = PyUnicode_FromString (json);
  free (json);
  return s;
}

/* ───────────────────────── writer capsule ─────────────────────────────────
 */

static const char _WR_CAPS[] = "doppler.wfmgen.compose.writer";

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

static PyObject *
_fn_sigmf_meta_json (PyObject *mod, PyObject *args)
{
  (void)mod;
  int       st, endian;
  double    fs, fc;
  PyObject *seg_list;
  if (!PyArg_ParseTuple (args, "iiddO", &st, &endian, &fs, &fc, &seg_list))
    return NULL;
  size_t         n_segs;
  wfm_segment_t *segs = _parse_segments (seg_list, &n_segs);
  if (!segs)
    return NULL;
  char *json = wfm_sigmf_meta_json (st, endian, fs, fc, segs, n_segs);
  free (segs);
  if (!json)
    {
      PyErr_SetString (PyExc_RuntimeError, "wfm_sigmf_meta_json failed");
      return NULL;
    }
  PyObject *s = PyUnicode_FromString (json);
  free (json);
  return s;
}

/* ─────────────────────────── reader capsule ───────────────────────────────
 *
 * Wraps wfm_reader_t — the dual of the writer. Container detection, header
 * parsing and the wire→unit conversion all live in C; this is pure binding.
 */

static const char _READER_CAPS[] = "doppler.wfmgen.compose.reader";

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
static const char _SINK_CAPS[] = "doppler.wfmgen.compose.sink";

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

static const char _CLOCK_CAPS[] = "doppler.wfmgen.compose.clock";

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

/* ───────────────────────── DSP helpers + mls_poly ─────────────────────────
 */

static PyObject *
_fn_rrc_taps (PyObject *mod, PyObject *args)
{
  (void)mod;
  double beta;
  int    sps, span;
  if (!PyArg_ParseTuple (args, "dii", &beta, &sps, &span))
    return NULL;
  if (sps < 1 || span < 1)
    {
      PyErr_SetString (PyExc_ValueError, "sps and span must be >= 1");
      return NULL;
    }
  npy_intp  dims[] = { (npy_intp)wfm_rrc_ntaps (sps, span) };
  PyObject *arr    = PyArray_SimpleNew (1, dims, NPY_FLOAT32);
  if (!arr)
    return NULL;
  wfm_rrc_taps (beta, sps, span, (float *)PyArray_DATA ((PyArrayObject *)arr));
  return arr;
}

static PyObject *
_fn_dsss_spread (PyObject *mod, PyObject *args)
{
  (void)mod;
  PyObject  *syms_obj, *code_obj;
  Py_ssize_t sf;
  if (!PyArg_ParseTuple (args, "OOn", &syms_obj, &code_obj, &sf))
    return NULL;
  if (sf < 1)
    {
      PyErr_SetString (PyExc_ValueError, "sf must be >= 1");
      return NULL;
    }
  PyArrayObject *syms = (PyArrayObject *)PyArray_FROM_OTF (
      syms_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!syms)
    return NULL;
  PyArrayObject *code = (PyArrayObject *)PyArray_FROM_OTF (
      code_obj, NPY_UINT8, NPY_ARRAY_C_CONTIGUOUS);
  if (!code)
    {
      Py_DECREF (syms);
      return NULL;
    }
  if ((size_t)PyArray_SIZE (code) < (size_t)sf)
    {
      PyErr_SetString (PyExc_ValueError, "code shorter than sf");
      Py_DECREF (syms);
      Py_DECREF (code);
      return NULL;
    }
  size_t    n_sym  = (size_t)PyArray_SIZE (syms);
  npy_intp  dims[] = { (npy_intp)(n_sym * (size_t)sf) };
  PyObject *arr    = PyArray_SimpleNew (1, dims, NPY_COMPLEX64);
  if (!arr)
    {
      Py_DECREF (syms);
      Py_DECREF (code);
      return NULL;
    }
  wfm_dsss_spread ((const float _Complex *)PyArray_DATA (syms), n_sym,
                   (const uint8_t *)PyArray_DATA (code), (size_t)sf,
                   (float _Complex *)PyArray_DATA ((PyArrayObject *)arr));
  Py_DECREF (syms);
  Py_DECREF (code);
  return arr;
}

static PyObject *
_fn_mls_poly (PyObject *mod, PyObject *args)
{
  (void)mod;
  unsigned int n;
  if (!PyArg_ParseTuple (args, "I", &n))
    return NULL;
  return PyLong_FromUnsignedLongLong (synth_mls_poly (n));
}

/* ───────────────────────── module table ───────────────────────────────────
 */

static PyMethodDef _methods[] = {
  { "composer_create", _fn_composer_create, METH_VARARGS,
    "composer_create(segments, repeat, continuous) -> capsule" },
  { "composer_from_json", _fn_composer_from_json, METH_VARARGS,
    "composer_from_json(json) -> capsule" },
  { "composer_from_file", _fn_composer_from_file, METH_VARARGS,
    "composer_from_file(path) -> capsule" },
  { "composer_execute", _fn_composer_execute, METH_VARARGS,
    "composer_execute(state, max) -> ndarray[complex64]" },
  { "composer_segments", _fn_composer_segments, METH_VARARGS,
    "composer_segments(state) -> (segments, repeat, continuous)" },
  { "composer_destroy", _fn_composer_destroy, METH_VARARGS,
    "composer_destroy(state) -> None" },
  { "spec_to_json", _fn_spec_to_json, METH_VARARGS,
    "spec_to_json(segments, repeat, continuous) -> str" },
  { "writer_open", _fn_writer_open, METH_VARARGS,
    "writer_open(path, file_type, sample_type, endian, fs, fc, total)"
    " -> capsule" },
  { "writer_write", _fn_writer_write, METH_VARARGS,
    "writer_write(state, iq) -> int" },
  { "writer_close", _fn_writer_close, METH_VARARGS,
    "writer_close(state) -> None" },
  { "writer_track_clipping", _fn_writer_track_clipping, METH_VARARGS,
    "writer_track_clipping(state, on) -> None" },
  { "writer_stats", _fn_writer_stats, METH_VARARGS,
    "writer_stats(state) -> (peak, clip_fraction)" },
  { "blue_write_hcb", _fn_blue_write_hcb, METH_VARARGS,
    "blue_write_hcb(path, sample_type, endian, fs, fc, data_start, total,"
    " detached) -> None" },
  { "sigmf_meta_json", _fn_sigmf_meta_json, METH_VARARGS,
    "sigmf_meta_json(sample_type, endian, fs, fc, segments) -> str" },
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
  { "rrc_taps", _fn_rrc_taps, METH_VARARGS,
    "rrc_taps(beta, sps, span) -> ndarray[float32]" },
  { "dsss_spread", _fn_dsss_spread, METH_VARARGS,
    "dsss_spread(syms, code, sf) -> ndarray[complex64]" },
  { "mls_poly", _fn_mls_poly, METH_VARARGS, "mls_poly(n) -> int" },
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
