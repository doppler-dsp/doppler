/**
 * dp_state_pyhelp.h — the state bytes interface for hand-written (no_generate)
 * CPython bindings.
 *
 * jm's `serializable` flag generates the Python triplet (state_bytes /
 * get_state / set_state) for every jm-managed object.  A `no_generate` module
 * (e.g. `ddc_fn`) owns its binding by hand, so jm cannot generate it there —
 * this macro provides the *same* three methods so the hand-written face is
 * byte-identical in behaviour and cannot drift from jm's output.
 *
 * Include from a `*_ext.c` translation unit (after Python.h).  Instantiate with
 * the PyObject struct type, an expression yielding the live C handle (or NULL
 * when closed/destroyed), and the C object prefix, e.g.:
 *
 *   DP_PY_STATE_METHODS(Ddcr, DdcrObject,
 *                       (self->closed ? NULL : self->h), ddcr)
 *
 * then add to the type's PyMethodDef table:
 *
 *   {"state_bytes", (PyCFunction)Ddcr_state_bytes, METH_NOARGS,
 *    "Serialized state size in bytes."},
 *   {"get_state",   (PyCFunction)Ddcr_get_state,   METH_NOARGS,
 *    "Serialize the engine's mutable state to bytes."},
 *   {"set_state",   (PyCFunction)Ddcr_set_state,   METH_O,
 *    "Restore mutable state from a get_state() blob."},
 *
 * The C triplet (`<pfx>_state_bytes/get_state/set_state`) must follow the
 * standard ABI (see dp_state.h): set_state returns DP_OK (0) or a negative
 * error.  set_state here enforces an exact size match, mirroring jm.
 */
#ifndef DP_STATE_PYHELP_H
#define DP_STATE_PYHELP_H

#define DP_PY_STATE_METHODS(FNPFX, SELF_T, HANDLE_EXPR, CPFX)                 \
  static PyObject *FNPFX##_state_bytes (SELF_T *self,                         \
                                        PyObject *Py_UNUSED (ignored))        \
  {                                                                           \
    void *_h = (HANDLE_EXPR);                                                 \
    if (!_h)                                                                  \
      {                                                                       \
        PyErr_SetString (PyExc_RuntimeError, "object is closed");            \
        return NULL;                                                          \
      }                                                                       \
    return PyLong_FromSize_t (CPFX##_state_bytes (_h));                       \
  }                                                                           \
  static PyObject *FNPFX##_get_state (SELF_T *self,                           \
                                      PyObject *Py_UNUSED (ignored))          \
  {                                                                           \
    void *_h = (HANDLE_EXPR);                                                 \
    if (!_h)                                                                  \
      {                                                                       \
        PyErr_SetString (PyExc_RuntimeError, "object is closed");            \
        return NULL;                                                          \
      }                                                                       \
    size_t    _n = CPFX##_state_bytes (_h);                                   \
    PyObject *_b = PyBytes_FromStringAndSize (NULL, (Py_ssize_t) _n);         \
    if (!_b)                                                                  \
      return NULL;                                                            \
    CPFX##_get_state (_h, PyBytes_AS_STRING (_b));                            \
    return _b;                                                                \
  }                                                                           \
  static PyObject *FNPFX##_set_state (SELF_T *self, PyObject *arg)            \
  {                                                                           \
    void *_h = (HANDLE_EXPR);                                                 \
    if (!_h)                                                                  \
      {                                                                       \
        PyErr_SetString (PyExc_RuntimeError, "object is closed");            \
        return NULL;                                                          \
      }                                                                       \
    if (!PyBytes_Check (arg))                                                 \
      {                                                                       \
        PyErr_SetString (PyExc_TypeError, "set_state expects bytes");        \
        return NULL;                                                          \
      }                                                                       \
    if ((size_t) PyBytes_GET_SIZE (arg) != CPFX##_state_bytes (_h))          \
      {                                                                       \
        PyErr_SetString (PyExc_ValueError, "state blob size mismatch");      \
        return NULL;                                                          \
      }                                                                       \
    if (CPFX##_set_state (_h, PyBytes_AS_STRING (arg)) != 0)                  \
      {                                                                       \
        PyErr_SetString (PyExc_ValueError, "set_state rejected the blob");   \
        return NULL;                                                          \
      }                                                                       \
    Py_RETURN_NONE;                                                           \
  }

#endif /* DP_STATE_PYHELP_H */
