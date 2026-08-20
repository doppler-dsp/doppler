/*
 * coding_ext.c — Python extension module coding
 *
 * Objects: ConvEncoder, Viterbi
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "coding_ext_conv_enc.c"
#include "coding_ext_viterbi.c"

/* ======================================================== */
/* Module                                                    */
/* ======================================================== */

static PyModuleDef coding_moduledef = {
  PyModuleDef_HEAD_INIT,
  .m_name = "coding",
  .m_doc
  = "The general channel codes — the families a standard configures, rather "
    "than\n"
    "any standard's picks. `ConvEncoder` and `Viterbi` are the two directions "
    "of a\n"
    "rate-1/n convolutional code, and both take the generator polynomials, so "
    "a\n"
    "caller names their own code rather than choosing from a menu.\n"
    "\n"
    "`ccsds_tm` holds CCSDS 131.0-B's configuration of these, the way any "
    "other\n"
    "caller would.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ConvEncoder, Viterbi\n"
    ">>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)\n"
    ">>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)\n"
    ">>> sym.size == 2 * bits.size\n"
    "True\n"
    ">>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)\n"
    ">>> bits_out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)\n"
    ">>> bits_out.dtype\n"
    "dtype('uint8')\n",
  .m_size    = -1,
  .m_methods = NULL,
};

PyMODINIT_FUNC
PyInit_coding (void)
{
  import_array ();
  if (PyType_Ready (&ConvEncoderObjType) < 0)
    return NULL;
  if (PyType_Ready (&ViterbiObjType) < 0)
    return NULL;
  PyObject *m = PyModule_Create (&coding_moduledef);
  if (!m)
    return NULL;
  Py_INCREF (&ConvEncoderObjType);
  if (PyModule_AddObject (m, "ConvEncoder", (PyObject *)&ConvEncoderObjType)
      < 0)
    {
      Py_DECREF (&ConvEncoderObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&ViterbiObjType);
  if (PyModule_AddObject (m, "Viterbi", (PyObject *)&ViterbiObjType) < 0)
    {
      Py_DECREF (&ViterbiObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
