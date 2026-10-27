

# File dp\_interrupt\_pyadopt.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md)

[Go to the source code of this file](dp__interrupt__pyadopt_8h_source.md)

_The process-global interrupt rendezvous, for a hand-written (_ `no_generate` _) CPython binding._[More...](#detailed-description)

* `#include <Python.h>`
* `#include "doppler/dp_interrupt_guard/dp_interrupt_guard_procglobal.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_interrupt\_pyadopt**](#function-dp_interrupt_pyadopt) (PyObject \* m) <br>_Adopt the owner module's interrupt state into module_ `m` _._ |


























## Detailed Description


just-makeit's `process_global = true` (gh-1117) generates this rendezvous into every extension module whose `PyInit_` it writes: the owning module publishes a `PyCapsule` over the interrupt state, and every other linking module imports the owner and adopts the pointer. A `no_generate` module has no generated `PyInit_`, so jm cannot put it there — and a module that skips it keeps its own copy of the flag while every other module shares one, which is doppler#976 exactly.


Same problem and same shape as [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md): one definition, so the hand-written face cannot drift from jm's output. The three names it needs — owner import path, module attribute, capsule name — are jm's invention and are read from the GENERATED [**dp\_interrupt\_guard\_procglobal.h**](dp__interrupt__guard__procglobal_8h.md) rather than spelled here, so renaming the component moves them by itself.


Include after Python.h and call once from `PyInit_<mod>`, after the module object exists and before returning it:


PyObject \*m = PyModule\_Create (&mymod); if (!m) return NULL; ... if (!dp\_interrupt\_pyadopt (m)) return NULL; return m;


A failure is FATAL to the import, matching what jm generates verbatim. That is deliberate rather than defensive: the alternative is a module that imports fine and silently reads a flag nobody sets, which is the defect this file exists to close, arrived at a second time. 


    
## Public Static Functions Documentation




### function dp\_interrupt\_pyadopt 

_Adopt the owner module's interrupt state into module_ `m` _._
```C++
static inline int dp_interrupt_pyadopt (
    PyObject * m
) 
```



Imports the owner module (DP\_INTERRUPT\_GUARD\_PG\_OWNER), reads its capsule attribute (DP\_INTERRUPT\_GUARD\_PG\_ATTR), unwraps the pointer under the capsule name DP\_INTERRUPT\_GUARD\_PG\_CAPSULE, and hands it to [**dp\_interrupt\_guard\_state\_adopt()**](dp__interrupt__guard__procglobal_8h.md#function-dp_interrupt_guard_state_adopt), so this module reads and writes the one process-wide flag rather than a private copy. Call it once from `PyInit_<mod>`, as shown in the file description.




**Parameters:**


* `m` The module being initialized. Released on failure, so the caller returns NULL directly and does not decref it again. 



**Returns:**

Non-zero on success. Zero with a Python exception set when the rendezvous failed. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_interrupt_pyadopt.h`

