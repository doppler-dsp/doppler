/*
 * wfm_writer_ext_wfm_writer.c — Writer type for the wfm_writer module.
 *
 * Included by wfm_writer_ext.c (the module aggregator).
 * Hand-patches to this file are preserved across jm commands.
 * Do NOT compile this file directly — only wfm_writer_ext.c is compiled.
 */
/* ======================================================== */
/* WriterObject — wraps wfm_writer_state_t *       */
/* ======================================================== */

#include "wfm_writer/wfm_writer_core.h"

typedef struct
{
  PyObject_HEAD wfm_writer_state_t *handle;
} WriterObject;

static void
WriterObj_dealloc (WriterObject *self)
{
  if (self->handle)
    {
      /* gh-541: tp_dealloc has no exception context — there
         is no caller to raise to, and an in-flight exception
         must not be clobbered. Discarding the status is the
         only correct choice here; the explicit teardown and
         __exit__ paths do report it. */
      (void)wfm_writer_destroy (self->handle);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyObject *
WriterObj_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  WriterObject *self = (WriterObject *)type->tp_alloc (type, 0);
  if (self)
    self->handle = NULL;
  return (PyObject *)self;
}

static int
WriterObj_init (WriterObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]
      = { "path",  "fs",       "file_type", "sample_type", "endian", "fc",
          "total", "headroom", "t0",        "sidecar",     NULL };
  PyObject          *path            = NULL; /* fspath -> bytes */
  double             fs              = 0.0;
  const char        *file_type_str   = "raw";
  const char        *sample_type_str = "cf32";
  const char        *endian_str      = "le";
  double             fc              = 0.0;
  unsigned long long total_raw       = 0;
  double             headroom        = 0.0;
  double             t0              = 0.0;
  int                sidecar_raw     = true;

  if (!PyArg_ParseTupleAndKeywords (
          args, kwds, "O&d|sssdKddp", kwlist, PyUnicode_FSConverter, &path,
          &fs, &file_type_str, &sample_type_str, &endian_str, &fc, &total_raw,
          &headroom, &t0, &sidecar_raw))
    {
      Py_XDECREF (path);
      return -1;
    }
  int file_type = 0;
  if (strcmp (file_type_str, "raw") == 0)
    file_type = 0;
  else if (strcmp (file_type_str, "csv") == 0)
    file_type = 1;
  else if (strcmp (file_type_str, "blue") == 0)
    file_type = 2;
  else if (strcmp (file_type_str, "sigmf") == 0)
    file_type = 3;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "file_type must be one of \"raw\", \"csv\", \"blue\", "
                    "\"sigmf\", got '%s'",
                    file_type_str);
      Py_XDECREF (path);
      return -1;
    }
  int sample_type = 0;
  if (strcmp (sample_type_str, "cf32") == 0)
    sample_type = 0;
  else if (strcmp (sample_type_str, "cf64") == 0)
    sample_type = 1;
  else if (strcmp (sample_type_str, "ci32") == 0)
    sample_type = 2;
  else if (strcmp (sample_type_str, "ci16") == 0)
    sample_type = 3;
  else if (strcmp (sample_type_str, "ci8") == 0)
    sample_type = 4;
  else if (strcmp (sample_type_str, "f32") == 0)
    sample_type = 5;
  else if (strcmp (sample_type_str, "f64") == 0)
    sample_type = 6;
  else if (strcmp (sample_type_str, "i32") == 0)
    sample_type = 7;
  else if (strcmp (sample_type_str, "i16") == 0)
    sample_type = 8;
  else if (strcmp (sample_type_str, "i8") == 0)
    sample_type = 9;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "sample_type must be one of \"cf32\", \"cf64\", \"ci32\", "
                    "\"ci16\", \"ci8\", \"f32\", \"f64\", \"i32\", \"i16\", "
                    "\"i8\", got '%s'",
                    sample_type_str);
      Py_XDECREF (path);
      return -1;
    }
  int endian = 0;
  if (strcmp (endian_str, "le") == 0)
    endian = 0;
  else if (strcmp (endian_str, "be") == 0)
    endian = 1;
  else
    {
      PyErr_Format (PyExc_ValueError,
                    "endian must be one of \"le\", \"be\", got '%s'",
                    endian_str);
      Py_XDECREF (path);
      return -1;
    }
  size_t total   = (size_t)total_raw;
  bool   sidecar = (int)sidecar_raw;
  self->handle   = wfm_writer_create (PyBytes_AS_STRING (path), fs, file_type,
                                      sample_type, endian, fc, total, headroom,
                                      t0, sidecar);
  Py_XDECREF (path);
  if (!self->handle)
    {
      PyErr_SetString (PyExc_OSError,
                       "cannot open the capture for writing: check the path, "
                       "the directory, and permissions -- and note that "
                       "file_type=\"sigmf\" requires a path ending in "
                       ".sigmf-data, since a SigMF capture is a "
                       "<base>.sigmf-data + <base>.sigmf-meta pair found by "
                       "name");
      return -1;
    }
  return 0;
}

static PyObject *
WriterObj_write (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "x", NULL };
  PyObject    *x_obj     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O", _kwlist, &x_obj))
    return NULL;
  PyArrayObject *x_arr = (PyArrayObject *)PyArray_FROM_OTF (
      x_obj, NPY_COMPLEX64, NPY_ARRAY_C_CONTIGUOUS);
  if (!x_arr)
    {
      return NULL;
    }
  const float complex *x     = (const float complex *)PyArray_DATA (x_arr);
  size_t               x_len = (size_t)PyArray_SIZE (x_arr);
  size_t               y     = wfm_writer_write (self->handle, x, x_len);
  Py_DECREF (x_arr);
  return PyLong_FromUnsignedLongLong ((unsigned long long)y);
}

static PyObject *
WriterObj_flush (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  int _rc = wfm_writer_flush (self->handle);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)", "flush failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
WriterObj_track_clipping (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "on", NULL };
  int          on        = 1;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "|i", _kwlist, &on))
    return NULL;
  wfm_writer_track_clipping (self->handle, on);
  Py_RETURN_NONE;
}

static PyObject *
WriterObj_add_keyword (WriterObject *self, PyObject *args, PyObject *kwds)
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  static char *_kwlist[] = { "tag", "type", "value", NULL };
  const char  *tag       = NULL;
  int          _type_i   = 0;
  PyObject    *value     = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "sCO", _kwlist, &tag, &_type_i,
                                    &value))
    return NULL;
  char _type = (char)_type_i;

  size_t _esz      = 0;
  int    _is_float = 0, _is_bytes = 0;
  switch (_type)
    {
    case 'A':
      _is_bytes = 1;
      break;
    case 'B':
      _esz = sizeof (int8_t);
      break;
    case 'I':
      _esz = sizeof (int16_t);
      break;
    case 'L':
      _esz = sizeof (int32_t);
      break;
    case 'T':
      _esz = sizeof (int32_t);
      break;
    case 'X':
      _esz = sizeof (int64_t);
      break;
    case 'F':
      _esz      = sizeof (float);
      _is_float = 1;
      break;
    case 'D':
      _esz      = sizeof (double);
      _is_float = 1;
      break;
    default:
      PyErr_Format (PyExc_ValueError, "unsupported code '%c'", _type);
      return NULL;
    }

  if (_is_bytes)
    {
      if (!PyUnicode_Check (value))
        {
          PyErr_SetString (PyExc_TypeError, "value must be a str");
          return NULL;
        }
      Py_ssize_t  _n = 0;
      const char *_s = PyUnicode_AsUTF8AndSize (value, &_n);
      if (!_s)
        return NULL;
      if (wfm_writer_add_keyword (self->handle, tag, _type, _s, (size_t)_n)
          != 0)
        {
          PyErr_SetString (PyExc_ValueError, "add_keyword failed");
          return NULL;
        }
      Py_RETURN_NONE;
    }

  PyObject *_seq   = NULL;
  size_t    _count = 1;
  if (PySequence_Check (value) && !PyUnicode_Check (value))
    {
      _seq = PySequence_Fast (value, "value must be a number or a sequence");
      if (!_seq)
        return NULL;
      _count = (size_t)PySequence_Fast_GET_SIZE (_seq);
    }
  if (_count == 0)
    {
      PyErr_SetString (PyExc_ValueError, "value sequence is empty");
      Py_XDECREF (_seq);
      return NULL;
    }
  uint8_t *_buf = (uint8_t *)malloc (_count * _esz);
  if (!_buf)
    {
      PyErr_NoMemory ();
      Py_XDECREF (_seq);
      return NULL;
    }
  for (size_t _i = 0; _i < _count; _i++)
    {
      PyObject *_item = _seq ? PySequence_Fast_GET_ITEM (_seq, _i) : value;
      uint8_t  *_p    = _buf + _i * _esz;
      if (_is_float)
        {
          double _d = PyFloat_AsDouble (_item);
          if (_d == -1.0 && PyErr_Occurred ())
            goto _err;
          switch (_type)
            {
            case 'F':
              {
                float _v = (float)_d;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            case 'D':
              {
                double _v = (double)_d;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            default:
              break;
            }
        }
      else
        {
          long long _ll = PyLong_AsLongLong (_item);
          if (_ll == -1 && PyErr_Occurred ())
            goto _err;
          switch (_type)
            {
            case 'B':
              {
                int8_t _v = (int8_t)_ll;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            case 'I':
              {
                int16_t _v = (int16_t)_ll;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            case 'L':
              {
                int32_t _v = (int32_t)_ll;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            case 'T':
              {
                int32_t _v = (int32_t)_ll;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            case 'X':
              {
                int64_t _v = (int64_t)_ll;
                memcpy (_p, &_v, sizeof _v);
                break;
              }
            default:
              break;
            }
        }
      continue;
    _err:
      free (_buf);
      Py_XDECREF (_seq);
      return NULL;
    }
  int _rc = wfm_writer_add_keyword (self->handle, tag, _type, _buf, _count);
  free (_buf);
  Py_XDECREF (_seq);
  if (_rc != 0)
    {
      PyErr_SetString (PyExc_ValueError, "add_keyword failed");
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Writer_getprop_clip_fraction (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_writer_get_clip_fraction (self->handle));
}
static PyObject *
Writer_getprop_peak_dbfs (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyFloat_FromDouble (wfm_writer_get_peak_dbfs (self->handle));
}
static PyObject *
Writer_getprop_clipped (WriterObject *self, void *Py_UNUSED (closure))
{
  if (!self->handle)
    {
      PyErr_SetString (PyExc_RuntimeError, "destroyed");
      return NULL;
    }
  /* <<IMPLEMENT: return the computed or stored value>> */
  return PyBool_FromLong ((long)(wfm_writer_get_clipped (self->handle)));
}

static PyGetSetDef Writer_getset[] = {
  { "clip_fraction", (getter)Writer_getprop_clip_fraction, NULL,
    "Fraction (0..1) of I/Q components that saturated. Always 0.0 unless "
    "`track_clipping()` was enabled before writing -- the counter is the one "
    "extra per-sample compare, so it is opt-in. `peak_dbfs` is always tracked "
    "and is enough to tell you clipping happened; this tells you how much.\n",
    NULL },
  { "peak_dbfs", (getter)Writer_getprop_peak_dbfs, NULL,
    "Largest per-axis magnitude written so far, in dBFS (full scale = 0 dBFS, "
    "so a value above 0 means an integer capture clipped). Always tracked. It "
    "is also the remedy: back off by `ceil(peak_dbfs)` dB of `headroom` and "
    "the capture fits. Float wire types never clip but still report a peak. "
    "`-inf` before anything is written.\n",
    NULL },
  { "clipped", (getter)Writer_getprop_clipped, NULL,
    "True if an integer capture saturated -- `peak_dbfs > 0` and the wire "
    "type is one of `ci32`/`ci16`/`ci8`. Always False for `cf32`/`cf64`, "
    "which cannot clip: a float sample above full scale is merely loud.\n",
    NULL },
  { NULL }
};

static PyObject *
WriterObj_destroy (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  if (self->handle)
    {
      int rc = wfm_writer_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_OSError,
                           "failed to finalise the capture: the trailing "
                           "header patch or extended header was not written");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyObject *
WriterObj_enter (WriterObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
WriterObj_exit (WriterObject *self, PyObject *args)
{
  (void)args;
  if (self->handle)
    {
      int rc = wfm_writer_destroy (self->handle);
      /* gh-541: clear the handle before reporting, so a second
         call is a no-op rather than a double free — the state is
         released whatever the status says. */
      self->handle = NULL;
      if (rc != 0)
        {
          PyErr_SetString (PyExc_OSError,
                           "failed to finalise the capture: the trailing "
                           "header patch or extended header was not written");
          return NULL;
        }
    }
  Py_RETURN_NONE;
}

static PyMethodDef WriterObj_methods[] = {

  { "write", (PyCFunction)(void *)WriterObj_write,
    METH_VARARGS | METH_KEYWORDS,
    "write(x) -> int\n"
    "\n"
    "Convert and write a block of samples.\n"
    "\n"
    "Takes `complex64` at unit scale and emits it in the writer's wire type.\n"
    "Call as many times as you like; the capture is the concatenation.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "x : NDArray[np.complex64]\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    the number of samples that actually landed — equal to what you\n"
    "    passed on success, fewer if the write was short (a full disk, a\n"
    "    quota). A short return is the per-block signal; close() reports the\n"
    "    same failure for the capture as a whole.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> from doppler.wfm import Composer, Reader, Segment, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"capture.blue\"\n"
    ">>> x = Composer([Segment(\"qpsk\", sps=8, "
    "num_samples=1024)]).compose()\n"
    ">>> with Writer(p, file_type=\"blue\", sample_type=\"ci16\",\n"
    "...             fs=2.4e6, fc=1.2e9) as w:\n"
    "...     w.write(x)\n"
    "1024\n"
    ">>> r = Reader(p)\n"
    ">>> r.fs, r.fc, r.num_samples\n"
    "(2400000.0, 1200000000.0, 1024)\n"
    ">>> r.close()\n"
    ">>> tmp.cleanup()   # directory and contents removed\n" },
  { "flush", (PyCFunction)WriterObj_flush, METH_NOARGS,
    "flush() -> None\n"
    "\n"
    "Make every sample written so far durable and observable to a\n"
    "concurrent reader, without ending the capture. Leaves the file on a\n"
    "sample boundary, which is what lets a follower read it without meeting\n"
    "a partial sample. Raises OSError if this or any earlier write failed; a\n"
    "capture is not finished until close().\n"
    "\n"
    "Leaves the file on a sample boundary -- write() emits whole samples, so\n"
    "a flush BETWEEN write calls is what lets a follower read the capture\n"
    "without meeting a partial one. Raises `OSError` if this or any earlier\n"
    "write failed; a capture is not complete until close().\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``flush failed``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Reader, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"live.blue\"\n"
    ">>> w = Writer(p, file_type=\"blue\", sample_type=\"ci16\", fs=2.4e6)\n"
    ">>> _ = w.write(np.zeros(16, dtype=np.complex64))\n"
    ">>> w.flush()                    # the samples are on disk now\n"
    ">>> Reader(p).read_follow(16).size\n"
    "16\n"
    ">>> w.close()\n"
    ">>> tmp.cleanup()\n" },
  { "track_clipping", (PyCFunction)(void *)WriterObj_track_clipping,
    METH_VARARGS | METH_KEYWORDS,
    "track_clipping(on) -> None\n"
    "\n"
    "Enable the per-component clip *counter* (off by default; peak is\n"
    "always on).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "on : int\n"
    "    Input.\n"
    "\n"
    "Examples\n"
    "--------\n"
    "    >>> import numpy as np\n"
    "    >>> from doppler import Writer\n"
    "    >>> obj = Writer(path=..., fs=0.0, file_type=\"raw\", "
    "sample_type=\"cf32\", endian=\"le\", fc=0.0, total=0, headroom=0.0, "
    "t0=0.0, sidecar=True)\n"
    "    >>> obj.track_clipping(0)\n" },
  { "add_keyword", (PyCFunction)(void *)WriterObj_add_keyword,
    METH_VARARGS | METH_KEYWORDS,
    "add_keyword(...) -- add a codec-typed value." },
  { "close", (PyCFunction)WriterObj_destroy, METH_NOARGS,
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
  { "destroy", (PyCFunction)WriterObj_destroy, METH_NOARGS,
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
  { "__enter__", (PyCFunction)WriterObj_enter, METH_NOARGS,
    "Enter a context manager, returning this object.\n"
    "\n"
    "Lets a Writer be used in a `with` statement so its C resources are\n"
    "released deterministically on exit rather than at collection time.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "Writer\n"
    "    This same object, not a copy.\n" },
  { "__exit__", (PyCFunction)WriterObj_exit, METH_VARARGS,
    "Exit a context manager, releasing the Writer.\n"
    "\n"
    "Equivalent to calling `close()`. Returns ``None``, so an exception\n"
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
    "    Traceback object, or None. Ignored.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C destructor reports failure. Raised from an explicit call\n"
    "    and from ``__exit__`` alike, so a failing teardown propagates out\n"
    "    of a ``with`` block (gh-541).\n" },
  { NULL }
};

static PyTypeObject WriterObjType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "wfm_writer.Writer",
  .tp_basicsize                           = sizeof (WriterObject),
  .tp_dealloc                             = (destructor)WriterObj_dealloc,
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_doc
  = "Open a capture for writing.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str | os.PathLike\n"
    "    where to write -- a `str` or any `os.PathLike` from Python. For\n"
    "    `file_type=\"sigmf\"` this MUST end in `.sigmf-data`: a SigMF "
    "capture is\n"
    "    a `<base>.sigmf-data` + `<base>.sigmf-meta` pair found by name, and\n"
    "    close() writes the sidecar beside it.\n"
    "fs : float\n"
    "    sample rate (Hz), and REQUIRED -- there is no default. BLUE stores "
    "it\n"
    "    as `xdelta = 1/fs`, SigMF and the raw/CSV `sidecar` as\n"
    "    `core:sample_rate`. Pass 0.0 to say the rate is not known: that "
    "writes\n"
    "    `xdelta = 0` and omits `core:sample_rate`, where a defaulted value\n"
    "    would have written a rate nobody supplied into a file that outlives "
    "the\n"
    "    process.\n"
    "file_type : Literal[\"raw\", \"csv\", \"blue\", \"sigmf\"], default "
    "\"raw\"\n"
    "    `\"raw\"` (headerless interleaved I/Q), `\"csv\"` (one `I,Q` line "
    "per\n"
    "    sample), `\"blue\"` (self-describing X-Midas/REDHAWK type-1000) or\n"
    "    `\"sigmf\"`. BLUE and SigMF record `fs`/`fc`/`t0` in the capture "
    "itself;\n"
    "    raw and CSV have nowhere to put them and keep them in the `sidecar`\n"
    "    instead.\n"
    "sample_type : Literal[\"cf32\", \"cf64\", \"ci32\", \"ci16\", \"ci8\"], "
    "default \"cf32\"\n"
    "    wire type: `\"cf32\"`, `\"cf64\"`, `\"ci32\"`, `\"ci16\"` or "
    "`\"ci8\"`. The\n"
    "    integer types quantise ±1.0 to full scale and can clip -- see\n"
    "    track_clipping()/peak_dbfs.\n"
    "endian : Literal[\"le\", \"be\"], default \"le\"\n"
    "    `\"le\"` or `\"be\"`; ignored for CSV, which is text.\n"
    "fc : float, default 0.0\n"
    "    centre frequency (Hz). BLUE records it as a `FREQ` keyword, SigMF "
    "as\n"
    "    `captures[0][\"core:frequency\"]`, raw and CSV in the `sidecar`. "
    "0.0\n"
    "    writes nothing, in every one of them -- absent is how this library "
    "says\n"
    "    \"not stated\", which is what `Reader.fc_source` reports back.\n"
    "total : int, default 0\n"
    "    expected sample count, for the BLUE header; close() patches the "
    "real\n"
    "    count, so 0 is fine when unknown.\n"
    "headroom : float, default 0.0\n"
    "    dB of output backoff (gain = 10^(-H/20)) applied before "
    "quantisation. A\n"
    "    single scale, so it does not change any power ratio -- only the\n"
    "    absolute level. 0 is a bit-exact no-op.\n"
    "t0 : float, default 0.0\n"
    "    capture start, seconds since the UNIX epoch. Optional where `fs` is\n"
    "    required, because a capture with no wall-clock anchor is still "
    "readable\n"
    "    and one with no rate is not. BLUE stores it as a J1950 timecode, "
    "SigMF\n"
    "    as `captures[0][\"core:datetime\"]`, raw and CSV in the `sidecar`. "
    "0.0\n"
    "    means unset and stays unset -- it is never written as 1970. "
    "`Reader.t0`\n"
    "    / `Reader.t0_source` read it back.\n"
    "sidecar : bool, default True\n"
    "    write a `<path>.sigmf-meta` JSON beside a `\"raw\"` or `\"csv\"` "
    "capture,\n"
    "    recording the `fs`, `fc` and `t0` those containers have nowhere to\n"
    "    keep. On by default: the caller already supplied the values at\n"
    "    construction, and dropping them on the floor left a file nobody -- "
    "its\n"
    "    own author included -- could interpret. Only what was actually "
    "stated\n"
    "    is written; nothing is invented. It is SigMF-SHAPED, not a SigMF\n"
    "    capture: the spec pairs `.sigmf-data`, so the name is APPENDED "
    "rather\n"
    "    than swapped (`cap.raw` -> `cap.raw.sigmf-meta`), which keeps it "
    "1:1\n"
    "    with its data file and unable to collide with a real capture's\n"
    "    metadata. Ignored for `\"blue\"` (its header already carries all "
    "three)\n"
    "    and for `\"sigmf\"`, where the sidecar is half the capture and "
    "cannot be\n"
    "    turned off. Pass false when an extra file beside the capture would\n"
    "    break a downstream glob.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If construction fails. The exception message is ``cannot open the\n"
    "    capture for writing: check the path, the directory, and permissions "
    "--\n"
    "    and note that file_type=\"sigmf\" requires a path ending in "
    ".sigmf-data,\n"
    "    since a SigMF capture is a <base>.sigmf-data + <base>.sigmf-meta "
    "pair\n"
    "    found by name``.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import pathlib, tempfile\n"
    ">>> import numpy as np\n"
    ">>> from doppler.wfm import Reader, Writer\n"
    ">>> tmp = tempfile.TemporaryDirectory()\n"
    ">>> p = pathlib.Path(tmp.name) / \"capture.blue\"\n"
    ">>> x = np.arange(1024, dtype=np.complex64) / 1024.0\n"
    ">>> with Writer(p, file_type=\"blue\", sample_type=\"cf32\",\n"
    "...             fs=2.4e6, fc=1.2e9) as w:\n"
    "...     w.write(x)                              # samples in\n"
    "...     w.add_keyword(\"COMMENT\", \"A\", \"demo\")   # tag the header\n"
    "1024\n"
    ">>> p.exists()\n"
    "True\n"
    ">>> with Reader(p) as r:                    # everything round-trips\n"
    "...     back = r.read(len(x))\n"
    "...     r.fs, r.fc, r.num_samples, r.keywords[\"COMMENT\"]\n"
    "(2400000.0, 1200000000.0, 1024, 'demo')\n"
    ">>> bool(np.array_equal(back, x))\n"
    "True\n"
    "\n"
    "A raw capture has nowhere to put `fs`/`fc`, so they go beside it:\n"
    "\n"
    ">>> q = pathlib.Path(tmp.name) / \"capture.raw\"\n"
    ">>> with Writer(q, fs=2.4e6, fc=1.2e9) as w:\n"
    "...     w.write(x)\n"
    "1024\n"
    ">>> (q.parent / \"capture.raw.sigmf-meta\").exists()\n"
    "True\n"
    ">>> tmp.cleanup()\n",
  .tp_methods = WriterObj_methods,
  .tp_getset  = Writer_getset,
  .tp_new     = WriterObj_new,
  .tp_init    = (initproc)WriterObj_init,
};
