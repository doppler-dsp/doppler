

# File interp\_table\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**interp\_table**](dir_532d1478dbb04668a5390572613675ee.md) **>** [**interp\_table\_core.h**](interp__table__core_8h.md)

[Go to the source code of this file](interp__table__core_8h_source.md)

_Periodically-extended interpolated lookup table._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**interp\_table\_state\_t**](structinterp__table__state__t.md) <br>_InterpolatedTable state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**interp\_table\_state\_t**](structinterp__table__state__t.md) \* | [**interp\_table\_create**](#function-interp_table_create) (const double complex \* table, size\_t table\_len, int method) <br>_Create an InterpolatedTable instance._  |
|  void | [**interp\_table\_destroy**](#function-interp_table_destroy) ([**interp\_table\_state\_t**](structinterp__table__state__t.md) \* state) <br>_Destroy an interp\_table instance and release all memory._  |
|  size\_t | [**interp\_table\_execute**](#function-interp_table_execute) ([**interp\_table\_state\_t**](structinterp__table__state__t.md) \* state, const double \* in, size\_t n\_in, double complex \* out, size\_t max\_out) <br>_Evaluate the table at each of_ `n_in` _points via periodic interpolation._ |
|  size\_t | [**interp\_table\_execute\_max\_out**](#function-interp_table_execute_max_out) ([**interp\_table\_state\_t**](structinterp__table__state__t.md) \* state) <br>_No fixed cap_  _execute()'s output is always sized to exactly match its own input length, so an_`out=` _buffer only ever needs to be at least that many elements (never a larger, unrelated minimum)._ |
|  void | [**interp\_table\_reset**](#function-interp_table_reset) ([**interp\_table\_state\_t**](structinterp__table__state__t.md) \* state) <br>_No-op: InterpolatedTable is purely a function of (table, method, point) with no running state to reset._  |




























## Detailed Description


Wraps a fixed complex table of one period and evaluates it at any real (possibly fractional, possibly out-of-range) position via periodic (mod-length) wraparound indexing plus one of three interpolation methods: nearest-below ("floor"), nearest-neighbor ("nearest"), or a linear fit between the two bracketing table points ("linear", the default). Purely a function of (table, method, point)  no running state, so create()/execute() is all there is to the lifecycle.


Lifecycle: create -&gt; execute\* -&gt; destroy



```C++
>>> from doppler.interp import InterpolatedTable
>>> import numpy as np
>>> table = InterpolatedTable(
...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))
>>> table.execute(np.array([1.1]))
array([1.1+0.j])
```
 


    
## Public Functions Documentation




### function interp\_table\_create 

_Create an InterpolatedTable instance._ 
```C++
interp_table_state_t * interp_table_create (
    const double complex * table,
    size_t table_len,
    int method
) 
```



Copies `table` internally; the caller's own array can be freed or modified afterward with no effect on this instance.




**Parameters:**


* `table` Complex table, one period, length `table_len`. 
* `table_len` Number of elements in `table` (&gt; 0). 
* `method` 0 = floor, 1 = nearest, 2 = linear. 



**Returns:**

Heap-allocated state, or NULL on allocation failure or table\_len == 0. 




**Note:**

Caller must call [**interp\_table\_destroy()**](interp__table__core_8h.md#function-interp_table_destroy) when done. 
```C++
>>> from doppler.interp import InterpolatedTable
>>> import numpy as np
>>> t = InterpolatedTable(
...     np.array([0.0, 1.0, 2.0], dtype=np.complex128),
...     method="linear")
>>> t.n
3
```
 





        

<hr>



### function interp\_table\_destroy 

_Destroy an interp\_table instance and release all memory._ 
```C++
void interp_table_destroy (
    interp_table_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function interp\_table\_execute 

_Evaluate the table at each of_ `n_in` _points via periodic interpolation._
```C++
size_t interp_table_execute (
    interp_table_state_t * state,
    const double * in,
    size_t n_in,
    double complex * out,
    size_t max_out
) 
```



Each point is wrapped mod the table length (any real value, any sign) and evaluated per the configured `method:` 
* floor: nearest index below (`table[floor(point) mod n]`)
* nearest: closer of the floor/next index (0.5 ties pick floor)
* linear: linear fit across the two bracketing indices






**Parameters:**


* `state` Must be non-NULL. 
* `in` Points to evaluate, length `n_in`. 
* `n_in` Number of points. 
* `out` Output buffer; must hold at least n\_in values. 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

min(n\_in, max\_out) interpolated points. 
```C++
>>> from doppler.interp import InterpolatedTable
>>> import numpy as np
>>> ramp = InterpolatedTable(
...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))
>>> ramp.execute(np.array([0.5, 1.1]))
array([0.5+0.j, 1.1+0.j])
```
 





        

<hr>



### function interp\_table\_execute\_max\_out 

_No fixed cap_  _execute()'s output is always sized to exactly match its own input length, so an_`out=` _buffer only ever needs to be at least that many elements (never a larger, unrelated minimum)._
```C++
size_t interp_table_execute_max_out (
    interp_table_state_t * state
) 
```




<hr>



### function interp\_table\_reset 

_No-op: InterpolatedTable is purely a function of (table, method, point) with no running state to reset._ 
```C++
void interp_table_reset (
    interp_table_state_t * state
) 
```



Present only to satisfy the common object interface; each execute() depends solely on its inputs, so a call before or after reset() returns identical samples.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.interp import InterpolatedTable
>>> table = InterpolatedTable(
...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))
>>> table.reset()                     # no running state to clear
>>> table.execute(np.array([1.5]))   # unchanged: (table, point)
array([1.5+0.j])
```
 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/interp_table/interp_table_core.h`

