

# File acq\_private.h



[**FileList**](files.md) **>** [**acq**](dir_34926d0c3adbcf3f65d9e19727d3354d.md) **>** [**acq\_private.h**](acq__private_8h.md)

[Go to the source code of this file](acq__private_8h_source.md)

_Shared internals for acq\_core.c and its test._ [More...](#detailed-description)

* `#include "doppler/acq/acq_core.h"`
* `#include <math.h>`
* `#include <stddef.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**acq\_rate\_better**](#function-acq_rate_better) (size\_t h, float stat, float best) <br>_Does hypothesis_ `h's` _statistic_`stat` _replace the best so far,_`best` _? The rule the rate search picks its winner by._ |
|  size\_t | [**acq\_resolve\_tile\_alias**](#function-acq_resolve_tile_alias) (const [**dp\_acq\_state\_t**](structdp__acq__state__t.md) \* st, size\_t row, size\_t col) <br> |


























## Detailed Description


Not part of the public API: nothing here is exported, and nothing here is promised to a caller. It exists so a rule that decides a result (the rate search's pick, the tile-edge alias's hop) can be unit-tested as a rule (test\_acq\_core.c includes it), the pattern [**detector/det\_private.h**](det__private_8h.md) follows. 


    
## Public Static Functions Documentation




### function acq\_rate\_better 

_Does hypothesis_ `h's` _statistic_`stat` _replace the best so far,_`best` _? The rule the rate search picks its winner by._
```C++
static inline int acq_rate_better (
    size_t h,
    float stat,
    float best
) 
```



Hypothesis 0 always wins the first place, whatever it reads: a dwell whose every statistic is NaN (a power that overflows float is inf/inf) would otherwise fail `stat > best` at every hypothesis, pick none, and leave the search restoring the PREVIOUS dwell's winning surface. After it, a hypothesis must beat the best strictly, so a NaN never displaces one and ties keep the earlier.


Unit-tested as a rule on NaN / inf / finite values, because the end-to-end route to a NaN statistic (a float power overflowing) depends on the compiler's code generation and differs between Release and Debug. 


        

<hr>



### function acq\_resolve\_tile\_alias 

```C++
static inline size_t acq_resolve_tile_alias (
    const dp_acq_state_t * st,
    size_t row,
    size_t col
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/acq/acq_private.h`

