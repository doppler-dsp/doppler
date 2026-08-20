/*
 * viterbi_ext.c — Python extension module viterbi
 *
 * Objects: Viterbi
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "viterbi_ext_viterbi.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef viterbi_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "viterbi",
  .m_doc
  = "Soft-decision Viterbi decoding of convolutional codes. The code itself "
    "—\n"
    "polynomials, encoder, trellis arithmetic — lives in the `conv` "
    "component;\n"
    "this is the decoder built over one, so a caller names the polynomials "
    "and\n"
    "gets a decoder for them.\n"
    "\n"
    "Soft in, hard out: `decode` takes log-likelihood ratios, one per "
    "channel\n"
    "symbol, and returns information bits. A hard-decision decoder throws "
    "away\n"
    "most of the gain the code exists to provide, which is why the input is "
    "LLRs.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.viterbi import Viterbi\n"
    ">>> v = Viterbi([0o171, 0o133], k=7, depth=35)\n"
    ">>> bits = v.decode(np.array([2.0, -2.0] * 128, dtype=np.float32))\n"
    ">>> bits.dtype\n"
    "dtype('uint8')\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_viterbi (void)
{
  import_array ();
  if (PyType_Ready (&ViterbiObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&viterbi_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&ViterbiObjType);
  if (PyModule_AddObject (m, "Viterbi", (PyObject *)&ViterbiObjType) < 0)
    {
      Py_DECREF (&ViterbiObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
