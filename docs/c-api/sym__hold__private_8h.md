

# File sym\_hold\_private.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md) **>** [**sym\_hold\_private.h**](sym__hold__private_8h.md)

[Go to the source code of this file](sym__hold__private_8h_source.md)

_The held-symbols store both continuous DSSS receivers deliver through (#2469): internals, never a caller's API._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include <assert.h>`
* `#include <string.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef size\_t(\* | [**dp\_sym\_hold\_call\_fn**](#typedef-dp_sym_hold_call_fn)  <br>_A receiver's own call: every symbol it produces from_ `x` _written to_`out` _, at most_`max_out` _of them._ |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_sym\_hold\_steps**](#function-dp_sym_hold_steps) (float \_Complex \* hold, size\_t \* hold\_len, size\_t hold\_cap, float \_Complex \*\* scratch, size\_t \* scratch\_cap, size\_t \* consumed, size\_t bound, [**dp\_sym\_hold\_call\_fn**](sym__hold__private_8h.md#typedef-dp_sym_hold_call_fn) call, void \* ctx, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_One steps() call through a receiver's store._  |


























## Detailed Description


AsyncDsssReceiver and DsssReceiver each keep a store of fixed capacity, the most one call of a code period can produce, and their steps() is this one function around their own call: held symbols first, then the call's, what does not fit `out` held for the next call, and the input taken only when nothing stays held and the call's bound fits what is left of `out` plus the store. One copy, so a fix to the rule lands in both receivers at once. 


    
## Public Types Documentation




### typedef dp\_sym\_hold\_call\_fn 

_A receiver's own call: every symbol it produces from_ `x` _written to_`out` _, at most_`max_out` _of them._
```C++
typedef size_t(* dp_sym_hold_call_fn) (void *ctx, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);
```




<hr>
## Public Static Functions Documentation




### function dp\_sym\_hold\_steps 

_One steps() call through a receiver's store._ 
```C++
static inline size_t dp_sym_hold_steps (
    float _Complex * hold,
    size_t * hold_len,
    size_t hold_cap,
    float _Complex ** scratch,
    size_t * scratch_cap,
    size_t * consumed,
    size_t bound,
    dp_sym_hold_call_fn call,
    void * ctx,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `hold` The store, `hold_cap` symbols. 
* `hold_len` Symbols held; updated. 
* `hold_cap` The store's capacity. 
* `scratch` The scratch the call produces into, grown on demand. 
* `scratch_cap` Its capacity; updated. 
* `consumed` Set to `x_len` when the input is taken, else 0. 
* `bound` `P(x_len)`: the most `call` can produce from `x`. 
* `call` The receiver's own call. 
* `ctx` Handed to `call`. 
* `x` Input samples. 
* `x_len` Their count. 
* `out` Output symbols. 
* `max_out` Capacity of `out`. 



**Returns:**

Symbols written to `out:` held ones first, then the call's.


The call is handed exactly `bound` slots in a Release build, so even a wrong bound cannot write past the store: the gate keeps `bound <= room + hold_cap`, and what is left after `out` takes its room is at most `bound - room`. A Debug build hands it one more, so a call that made more than the bound shows, and aborts, instead of stopping at it. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dsss_receiver/sym_hold_private.h`

