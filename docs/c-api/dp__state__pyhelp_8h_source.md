

# File dp\_state\_pyhelp.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md)

[Go to the documentation of this file](dp__state__pyhelp_8h.md)


```C++

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
```


