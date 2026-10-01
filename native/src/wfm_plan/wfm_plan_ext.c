/*
 * wfm_plan_ext.c — handle extension: typed `Plan` over `wfm_plan` (jm;
 * gh-306).
 *
 * `Plan` wraps an opaque wfm_plan_t *; the resource logic
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

#include "doppler/wfm/wfm_plan.h"

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

typedef struct
{
  PyObject_HEAD wfm_plan_t *h;
  int                       closed;
} PlanObject;

static int
Plan_init (PlanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[]  = { "spec_json", NULL };
  const char  *spec_json = NULL;
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "s", kwlist, &spec_json))
    {
      return -1;
    }

  if (!self->closed && self->h)
    {
      dp_wfm_plan_destroy (self->h);
      self->h      = NULL;
      self->closed = 1;
    }
  self->h = dp_wfm_plan_prepare (spec_json);
  if (!self->h)
    {
      PyErr_SetString (PyExc_RuntimeError, "dp_wfm_plan_prepare failed");
      return -1;
    }
  self->closed = 0;

  return 0;
}

static PyObject *
Plan_render (PlanObject *self, PyObject *args)
{
  const char *overrides_json = NULL;
  if (!PyArg_ParseTuple (args, "s", &overrides_json))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  size_t _n_need = (size_t)(dp_wfm_plan_len (self->h));
  if (_n_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "Plan.render: output of %zu elements is too large",
                    _n_need);
      return NULL;
    }
  npy_intp  _n  = (npy_intp)_n_need;
  PyObject *arr = PyArray_SimpleNew (1, &_n, NPY_COMPLEX64);
  if (!arr)
    return NULL;
  float _Complex *_out = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr);
  size_t          _got;
  Py_BEGIN_ALLOW_THREADS
    _got = dp_wfm_plan_render (self->h, overrides_json, _out);
  Py_END_ALLOW_THREADS
  PyArray_DIMS ((PyArrayObject *)arr)[0] = (npy_intp)_got; /* trim */
  return arr;
}

static PyObject *
Plan_at (PlanObject *self, PyObject *args)
{
  double             snr_raw  = 0;
  unsigned long long seed_raw = 0;
  if (!PyArg_ParseTuple (args, "dK", &snr_raw, &seed_raw))
    return NULL;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  size_t _n_need = (size_t)(dp_wfm_plan_len (self->h));
  if (_n_need > (size_t)NPY_MAX_INTP)
    {
      PyErr_Format (PyExc_OverflowError,
                    "Plan.at: output of %zu elements is too large", _n_need);
      return NULL;
    }
  npy_intp  _n  = (npy_intp)_n_need;
  PyObject *arr = PyArray_SimpleNew (1, &_n, NPY_COMPLEX64);
  if (!arr)
    return NULL;
  float _Complex *_out = (float _Complex *)PyArray_DATA ((PyArrayObject *)arr);
  size_t          _got;
  Py_BEGIN_ALLOW_THREADS
    _got = dp_wfm_plan_at (self->h, snr_raw, (uint64_t)seed_raw, _out);
  Py_END_ALLOW_THREADS
  PyArray_DIMS ((PyArrayObject *)arr)[0] = (npy_intp)_got; /* trim */
  return arr;
}

static PyObject *
Plan_check_snr (PlanObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  int _rc;
  _rc = dp_wfm_plan_check_snr (self->h);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_ValueError, "%s (rc=%lld)",
                    "this scene carries no noise, so the Plan has no noise "
                    "floor for snr to move: give a source a finite snr "
                    "(below 100 dB) and an snr_mode",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyObject *
Plan_length (PlanObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  size_t r;
  r = dp_wfm_plan_len (self->h);
  return PyLong_FromUnsignedLongLong ((unsigned long long)r);
}

static PyObject *
Plan_n_sources (PlanObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  size_t r;
  r = dp_wfm_plan_n_sources (self->h);
  return PyLong_FromUnsignedLongLong ((unsigned long long)r);
}

static PyObject *
Plan_anchor_seed (PlanObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  uint64_t r;
  r = dp_wfm_plan_anchor_seed (self->h);
  return PyLong_FromUnsignedLongLong ((unsigned long long)r);
}

static PyObject *
Plan_save (PlanObject *self, PyObject *args)
{
  (void)args;
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  size_t _n   = (size_t)dp_wfm_plan_save_bytes (self->h);
  char  *_buf = (char *)PyMem_Malloc (_n ? _n : 1);
  if (!_buf)
    return PyErr_NoMemory ();
  size_t _got;
  _got         = dp_wfm_plan_save (self->h, _buf);
  PyObject *_r = PyBytes_FromStringAndSize (_buf, (Py_ssize_t)_got);
  PyMem_Free (_buf);
  return _r;
}

static PyObject *
Plan_dump (PlanObject *self, PyObject *args, PyObject *kwds)
{
  static char *kwlist[] = { "path", NULL };
  PyObject    *path     = NULL; /* fspath -> bytes */
  if (!PyArg_ParseTupleAndKeywords (args, kwds, "O&", kwlist,
                                    PyUnicode_FSConverter, &path))
    {
      Py_XDECREF (path);
      return NULL;
    }
  if (self->closed)
    {
      PyErr_SetString (PyExc_RuntimeError, "Plan is closed");
      return NULL;
    }
  int _rc;
  _rc = dp_wfm_plan_dump (self->h, PyBytes_AS_STRING (path));
  Py_XDECREF (path);
  if (_rc != 0)
    {
      PyErr_Format (PyExc_OSError, "%s (rc=%lld)", "dp_wfm_plan_dump failed",
                    (long long)_rc);
      return NULL;
    }
  Py_RETURN_NONE;
}

static PyGetSetDef Plan_getset[] = { { NULL, NULL, NULL, NULL, NULL } };

static PyObject *
Plan_close (PlanObject *self, PyObject *Py_UNUSED (ignored))
{
  if (!self->closed && self->h)
    {
      dp_wfm_plan_destroy (self->h);
      self->closed = 1;
    }
  Py_RETURN_NONE;
}

static PyObject *
Plan_enter (PlanObject *self, PyObject *Py_UNUSED (ignored))
{
  Py_INCREF (self);
  return (PyObject *)self;
}

static PyObject *
Plan_exit (PlanObject *self, PyObject *args)
{
  (void)args;
  return Plan_close (self, NULL);
}
static void
Plan_dealloc (PlanObject *self)
{
  if (!self->closed && self->h)
    {
      dp_wfm_plan_destroy (self->h);
    }
  Py_TYPE (self)->tp_free ((PyObject *)self);
}

static PyMethodDef Plan_methods[] = {
  { "render", (PyCFunction)Plan_render, METH_VARARGS,
    "General render: apply a JSON override spec, return a cf32 array.\n"
    "\n"
    "`overrides_json` is a small JSON object, all keys optional:\n"
    "`{\"gains\":[dB…], \"phases\":[rad…], \"enable\":[bool…], \"snr\":dB,\n"
    "\"seed\":u}` (`gains`/`phases`/`enable` are per-source, flat and\n"
    "segment-major, length = dp_wfm_plan_n_sources()). An empty object (or\n"
    "NULL) renders the baseline — bit-identical to\n"
    "`Composer(scene).compose()`. Writes up to `dp_wfm_plan_len(p)` samples\n"
    "to `out`. An `\"snr\"` key on a Plan whose scene carries no noise is\n"
    "refused (see dp_wfm_plan_check_snr()).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "overrides_json : str\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Samples actually written for this draw (<= dp_wfm_plan_len(p)), or\n"
    "    0 when refused, with `out` untouched.\n" },
  { "at", (PyCFunction)Plan_at, METH_VARARGS,
    "Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse).\n"
    "\n"
    "`out = Σ gain_k·cache_k + gain(snr)·noise(seed)` per segment/instance;\n"
    "writes up to `dp_wfm_plan_len(p)` samples. Equivalent to `render` with\n"
    "only `{\"snr\":snr,\"seed\":seed}` — `seed` is always an explicit "
    "override\n"
    "here, and so is `snr`: on a Plan whose scene carries no noise it is\n"
    "refused (see dp_wfm_plan_check_snr()).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "snr : float\n"
    "    Input.\n"
    "seed : int\n"
    "    Input.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "NDArray[Any]\n"
    "    Samples actually written for this draw (<= dp_wfm_plan_len(p)), or\n"
    "    0 when refused, with `out` untouched.\n" },
  { "check_snr", (PyCFunction)Plan_check_snr, METH_VARARGS,
    "Whether an `snr` can be applied to this Plan.\n"
    "\n"
    "O(1): the answer is fixed at prepare time. A caller about to sweep SNR\n"
    "checks it once; `dp_wfm_plan_at()` and `dp_wfm_plan_render()` apply the\n"
    "same rule themselves.\n"
    "\n"
    "Raises\n"
    "------\n"
    "ValueError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``this scene carries no noise, so the Plan has no noise floor for\n"
    "    snr to move: give a source a finite snr (below 100 dB) and an\n"
    "    snr_mode``, with the return code appended (gh-869).\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> from doppler.wfm import Composer, prepare\n"
    ">>> clean = prepare(Composer(type=\"tone\", num_samples=64))\n"
    ">>> clean.at(6.0)\n"
    "Traceback (most recent call last):\n"
    "    ...\n"
    "ValueError: this scene carries no noise, so the Plan has no noise floor "
    "for snr to move: give a source a finite snr (below 100 dB) and an "
    "snr_mode (rc=-4)\n"
    ">>> len(prepare(Composer(type=\"tone\", num_samples=64, "
    "snr=10.0)).at(6.0))\n"
    "64\n" },
  { "length", (PyCFunction)Plan_length, METH_VARARGS,
    "Worst-case materialized length in samples (every ranged gap at its\n"
    "`hi` bound) — the jm binding's out_len_fn / allocation capacity.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "n_sources", (PyCFunction)Plan_n_sources, METH_VARARGS,
    "Number of cached signal sources across every segment (excludes noise\n"
    "floors); the length of the `gains`/`phases`/`enable` arrays.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "anchor_seed", (PyCFunction)Plan_anchor_seed, METH_VARARGS,
    "The noise seed that reproduces a full compose.\n"
    "\n"
    "The first noisy segment's default seed (its first source's `seed`\n"
    "field). Passing this as `dp_wfm_plan_at`'s seed (with the scene's base\n"
    "SNR) yields the byte-identical output of `wfm_compose` for a\n"
    "single-segment scene; for a multi-segment scene each segment still\n"
    "draws from its own default seed unless overridden. Varying the seed\n"
    "draws independent Monte-Carlo noise (and, for a ranged-gap scene,\n"
    "timing) realizations.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "int\n"
    "    Output.\n" },
  { "save", (PyCFunction)Plan_save, METH_VARARGS,
    "Serialize a Plan into blob (dp_wfm_plan_save_bytes(p) bytes).\n"
    "\n"
    "Native-endian. The blob embeds the spec JSON, so a restore is\n"
    "self-contained. Returns the number of bytes written (==\n"
    "dp_wfm_plan_save_bytes(p)) — the actual-length contract a\n"
    "variable-output binding needs, so `save() -> bytes` generates with no\n"
    "hand-written glue.\n"
    "\n"
    "Returns\n"
    "-------\n"
    "bytes\n"
    "    Output.\n" },
  { "dump", (PyCFunction)Plan_dump, METH_VARARGS | METH_KEYWORDS,
    "Save a Plan to a file (dp_wfm_plan_save() bytes at path).\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "path : str | os.PathLike\n"
    "    Input.\n"
    "\n"
    "Raises\n"
    "------\n"
    "OSError\n"
    "    If the C call returns a non-zero status. The exception message is\n"
    "    ``dp_wfm_plan_dump failed``, with the return code appended\n"
    "    (gh-869).\n" },
  { "close", (PyCFunction)Plan_close, METH_NOARGS,
    "Release the handle and free resources." },
  { "__enter__", (PyCFunction)Plan_enter, METH_NOARGS, NULL },
  { "__exit__", (PyCFunction)Plan_exit, METH_VARARGS, NULL },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject PlanType = {
  PyVarObject_HEAD_INIT (NULL, 0).tp_name = "doppler.wfm.Plan",
  .tp_basicsize                           = sizeof (PlanObject),
  .tp_flags                               = Py_TPFLAGS_DEFAULT,
  .tp_new                                 = PyType_GenericNew,
  .tp_init                                = (initproc)Plan_init,
  .tp_dealloc                             = (destructor)Plan_dealloc,
  .tp_getset                              = Plan_getset,
  .tp_methods                             = Plan_methods,
  .tp_doc                                 = PyDoc_STR (
      "Prepare a Plan from a composer spec JSON (Composer.to_json()).\n"
      "\n"
      "Parses + resolves the scene, validates scope per segment, then renders "
      "and\n"
      "caches each segment's clean signal ON-time at gain 1. Returns NULL on "
      "parse\n"
      "failure or an out-of-scope spec (continuous/repeat scene, a ranged "
      "on-time,\n"
      "a ranged per-source field, a non-trailing/multiple noise source within "
      "a\n"
      "segment, or a source carrying clock Doppler).\n"
      "\n"
      "The last is a refusal rather than a limitation to work around. This "
      "cache\n"
      "holds one source's clean ON-time in isolation; a Doppler channel is a\n"
      "resampler with state that runs through the gaps too, so what a burst\n"
      "renders as depends on the leading delay and on the previous instance's "
      "gap,\n"
      "and the cache has nowhere to keep that. It also puts the AWGN outside "
      "the\n"
      "channel where compose() puts it inside. Both were measured against\n"
      "compose(), not assumed -- see the note in plan_build(). Refusing "
      "beats\n"
      "caching a render that differs from compose() invisibly; teaching the "
      "cache\n"
      "to carry a channel's history is gh-1109.\n"
      "\n"
      "Parameters\n"
      "----------\n"
      "spec_json : str\n"
      "    A NUL-terminated composer spec JSON string.\n"),
};

static PyObject *
wfm_plan_PlanFromBlob (PyObject *_mod, PyObject *args)
{
  (void)_mod;
  const char *blob     = NULL; /* borrowed bytes buffer */
  Py_ssize_t  blob_len = 0;
  if (!PyArg_ParseTuple (args, "y#", &blob, &blob_len))
    return NULL;
  wfm_plan_t *_h = dp_wfm_plan_restore ((const void *)blob, (size_t)blob_len);
  if (!_h)
    {
      PyErr_SetString (PyExc_ValueError, "PlanFromBlob failed");
      return NULL;
    }
  PlanObject *self = (PlanObject *)PlanType.tp_alloc (&PlanType, 0);
  if (!self)
    {
      dp_wfm_plan_destroy (_h);
      return NULL;
    }
  self->h      = _h;
  self->closed = 0;
  return (PyObject *)self;
}

static PyObject *
wfm_plan_PlanFromFile (PyObject *_mod, PyObject *args)
{
  (void)_mod;
  PyObject *path = NULL; /* fspath -> bytes */
  if (!PyArg_ParseTuple (args, "O&", PyUnicode_FSConverter, &path))
    return NULL;
  wfm_plan_t *_h = dp_wfm_plan_load (PyBytes_AS_STRING (path));
  Py_XDECREF (path);
  if (!_h)
    {
      PyErr_SetString (PyExc_ValueError, "PlanFromFile failed");
      return NULL;
    }
  PlanObject *self = (PlanObject *)PlanType.tp_alloc (&PlanType, 0);
  if (!self)
    {
      dp_wfm_plan_destroy (_h);
      return NULL;
    }
  self->h      = _h;
  self->closed = 0;
  return (PyObject *)self;
}

static PyMethodDef wfm_plan_functions[]
    = { { "PlanFromBlob", (PyCFunction)wfm_plan_PlanFromBlob, METH_VARARGS,
          "Construct a Plan via dp_wfm_plan_restore.\n" },
        { "PlanFromFile", (PyCFunction)wfm_plan_PlanFromFile, METH_VARARGS,
          "Construct a Plan via dp_wfm_plan_load.\n" },
        { NULL, NULL, 0, NULL } };

static struct PyModuleDef _moduledef = { PyModuleDef_HEAD_INIT,
                                         "wfm_plan",
                                         NULL,
                                         -1,
                                         wfm_plan_functions,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL };

PyMODINIT_FUNC
PyInit_wfm_plan (void)
{
  import_array ();
  if (PyType_Ready (&PlanType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&PlanType);
  if (PyModule_AddObject (m, "Plan", (PyObject *)&PlanType) < 0)
    {
      Py_DECREF (&PlanType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
