/**
 * dp_interrupt_pyadopt.h — the process-global interrupt rendezvous, for a
 * hand-written (`no_generate`) CPython binding.
 *
 * just-makeit's `process_global = true` (gh-1117) generates this rendezvous
 * into every extension module whose `PyInit_` it writes: the owning module
 * publishes a `PyCapsule` over the interrupt state, and every other linking
 * module imports the owner and adopts the pointer. A `no_generate` module
 * has no generated `PyInit_`, so jm cannot put it there — and a module that
 * skips it keeps its own copy of the flag while every other module shares
 * one, which is doppler#976 exactly.
 *
 * Same problem and same shape as dp_state_pyhelp.h: one definition, so the
 * hand-written face cannot drift from jm's output. The three names it needs
 * — owner import path, module attribute, capsule name — are jm's invention
 * and are read from the GENERATED dp_interrupt_guard_procglobal.h rather
 * than spelled here, so renaming the component moves them by itself.
 *
 * Include after Python.h and call once from `PyInit_<mod>`, after the module
 * object exists and before returning it:
 *
 *   PyObject *m = PyModule_Create (&mymod);
 *   if (!m)
 *     return NULL;
 *   ...
 *   if (!dp_interrupt_pyadopt (m))
 *     return NULL;
 *   return m;
 *
 * @param m The module being initialised. Released on failure, so the caller
 *          returns NULL directly and does not decref it again.
 * @return  Non-zero on success. Zero with a Python exception set when the
 *          rendezvous failed.
 *
 * A failure is FATAL to the import, matching what jm generates verbatim.
 * That is deliberate rather than defensive: the alternative is a module
 * that imports fine and silently reads a flag nobody sets, which is the
 * defect this file exists to close, arrived at a second time.
 */
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
