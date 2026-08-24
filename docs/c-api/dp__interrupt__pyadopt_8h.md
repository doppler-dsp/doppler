

# File dp\_interrupt\_pyadopt.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md)

[Go to the source code of this file](dp__interrupt__pyadopt_8h_source.md)



* `#include <Python.h>`
* `#include "dp_interrupt_guard/dp_interrupt_guard_procglobal.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_interrupt\_pyadopt**](#function-dp_interrupt_pyadopt) (PyObject \* m) <br> |


























## Public Static Functions Documentation




### function dp\_interrupt\_pyadopt 

```C++
static inline int dp_interrupt_pyadopt (
    PyObject * m
) 
```



[**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md) — the process-global interrupt rendezvous, for a hand-written (`no_generate`) CPython binding.


just-makeit's `process_global = true` (gh-1117) generates this rendezvous into every extension module whose `PyInit_` it writes: the owning module publishes a `PyCapsule` over the interrupt state, and every other linking module imports the owner and adopts the pointer. A `no_generate` module has no generated `PyInit_`, so jm cannot put it there — and a module that skips it keeps its own copy of the flag while every other module shares one, which is doppler#976 exactly.


Same problem and same shape as [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md): one definition, so the hand-written face cannot drift from jm's output. The three names it needs — owner import path, module attribute, capsule name — are jm's invention and are read from the GENERATED [**dp\_interrupt\_guard\_procglobal.h**](dp__interrupt__guard__procglobal_8h.md) rather than spelled here, so renaming the component moves them by itself.


Include after Python.h and call once from `PyInit_<mod>`, after the module object exists and before returning it:


PyObject \*m = PyModule\_Create (&mymod); if (!m) return NULL; ... if (!dp\_interrupt\_pyadopt (m)) return NULL; return m;




**Parameters:**


* `m` The module being initialised. Released on failure, so the caller returns NULL directly and does not decref it again. 



**Returns:**

Non-zero on success. Zero with a Python exception set when the rendezvous failed.


A failure is FATAL to the import, matching what jm generates verbatim. That is deliberate rather than defensive: the alternative is a module that imports fine and silently reads a flag nobody sets, which is the defect this file exists to close, arrived at a second time. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interrupt_pyadopt.h`

