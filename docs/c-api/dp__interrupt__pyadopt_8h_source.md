

# File dp\_interrupt\_pyadopt.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md)

[Go to the documentation of this file](dp__interrupt__pyadopt_8h.md)


```C++

#ifndef DP_INTERRUPT_PYADOPT_H
#define DP_INTERRUPT_PYADOPT_H

#include <Python.h>

#include "dp_interrupt_guard/dp_interrupt_guard_procglobal.h"

static inline int
dp_interrupt_pyadopt (PyObject *m)
{
  PyObject *own = PyImport_ImportModule (DP_INTERRUPT_GUARD_PG_OWNER);
  if (!own)
    {
      Py_DECREF (m);
      return 0;
    }
  PyObject *pg = PyObject_GetAttrString (own, DP_INTERRUPT_GUARD_PG_ATTR);
  Py_DECREF (own);
  if (!pg)
    {
      Py_DECREF (m);
      return 0;
    }
  void *p = PyCapsule_GetPointer (pg, DP_INTERRUPT_GUARD_PG_CAPSULE);
  Py_DECREF (pg);
  if (!p)
    {
      Py_DECREF (m);
      return 0;
    }
  dp_interrupt_guard_state_adopt (p);
  return 1;
}

#endif /* DP_INTERRUPT_PYADOPT_H */
```


