/*
 * coding_ext.c — Python extension module coding
 *
 * Objects: ConvEncoder, Viterbi, ReedSolomon, Interleaver, Deinterleaver
 * GENERATED — do not hand-edit. Patches belong in the _ext_<obj>.c fragments.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <complex.h>
#include <numpy/arrayobject.h>

#include "coding_ext_conv_enc.c"
#include "coding_ext_deinterleaver.c"
#include "coding_ext_interleaver.c"
#include "coding_ext_rs_codec.c"
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
    "rate-1/n convolutional code and both take the generator polynomials;\n"
    "`ReedSolomon` is both directions of an RS code over `GF(2**J)` and takes "
    "the\n"
    "five numbers that define one. A caller names their own code rather than\n"
    "choosing from a menu.\n"
    "\n"
    "`ccsds_tm` holds CCSDS 131.0-B's configuration of these, the way any "
    "other\n"
    "caller would — and note that a standard adds things that are NOT "
    "properties\n"
    "of the code, such as the dual-basis symbol representation CCSDS "
    "transmits its\n"
    "Reed-Solomon symbols in. Matching the algebra is not the same as "
    "matching the\n"
    "wire.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> import numpy as np\n"
    ">>> from doppler.coding import ConvEncoder, ReedSolomon, Viterbi\n"
    ">>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)\n"
    ">>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)\n"
    ">>> sym.size == 2 * bits.size\n"
    "True\n"
    ">>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)\n"
    ">>> bits_out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)\n"
    ">>> bits_out.dtype\n"
    "dtype('uint8')\n"
    ">>> rs = ReedSolomon(nroots=32)             # RS(255,223), corrects 16\n"
    ">>> rs.n, rs.k, rs.e\n"
    "(255, 223, 16)\n",
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
  if (PyType_Ready (&ReedSolomonObjType) < 0)
    return NULL;
  if (PyType_Ready (&InterleaverObjType) < 0)
    return NULL;
  if (PyType_Ready (&DeinterleaverObjType) < 0)
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
  Py_INCREF (&ReedSolomonObjType);
  if (PyModule_AddObject (m, "ReedSolomon", (PyObject *)&ReedSolomonObjType)
      < 0)
    {
      Py_DECREF (&ReedSolomonObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&InterleaverObjType);
  if (PyModule_AddObject (m, "Interleaver", (PyObject *)&InterleaverObjType)
      < 0)
    {
      Py_DECREF (&InterleaverObjType);
      Py_DECREF (m);
      return NULL;
    }
  Py_INCREF (&DeinterleaverObjType);
  if (PyModule_AddObject (m, "Deinterleaver",
                          (PyObject *)&DeinterleaverObjType)
      < 0)
    {
      Py_DECREF (&DeinterleaverObjType);
      Py_DECREF (m);
      return NULL;
    }
  return m;
}
