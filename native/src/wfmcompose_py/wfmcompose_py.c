/*
 * wfmcompose_py.c — hand-written CPython binding for the one wfmgen transport
 * primitive just-makeit cannot yet express: blue_write_hcb.
 *
 * This is a `no_generate` module (just-makeit only wires the CMake
 * add_subdirectory): the file is hand-owned, like ddc_fn_ext.c.
 *
 * The composer (Synth/Segment/Timeline/Composer), the transport handles
 * (Writer/Reader/StreamSink/SampleClock) and SigMF metadata
 * (Composer.to_sigmf()) are all jm-generated now. The capsule bindings that
 * used to live here for those — the _fn_writer/reader/sink/clock PyCapsule
 * glue — were superseded by the generated handles and have been removed
 * (gh-178 review #7), along with the orphan includes they needed. The only
 * surface still wired through here is blue_write_hcb, backing the hand-Python
 * write_blue_header() (a path + string-enum I/O helper jm module functions
 * cannot yet express).
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdio.h>

#include "wfm_writer/wfm_writer_core.h" /* wfm_blue_write_hcb */

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

/* ───────────────────────── module table ───────────────────────────────────
 */

static PyMethodDef _methods[] = {
  { "blue_write_hcb", _fn_blue_write_hcb, METH_VARARGS,
    "blue_write_hcb(path, sample_type, endian, fs, fc, data_start, total,"
    " detached) -> None" },
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
  return PyModule_Create (&_moduledef);
}
