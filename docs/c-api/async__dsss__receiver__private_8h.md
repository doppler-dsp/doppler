

# File async\_dsss\_receiver\_private.h



[**FileList**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_private.h**](async__dsss__receiver__private_8h.md)

[Go to the source code of this file](async__dsss__receiver__private_8h_source.md)

_A receiver's roles inside an AsyncDsssPool: pooled (idle until seeded) and search-only (the controller's one search)._ [More...](#detailed-description)

* `#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"`
* `#include <stdbool.h>`
* `#include <stddef.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_async\_dsss\_receiver\_set\_pooled**](#function-dp_async_dsss_receiver_set_pooled) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, bool on) <br>_Make a receiver a pooled one: while it holds nothing it is idle (ASYNC\_DSSS\_RX\_IDLE), consuming samples without searching, until a controller seeds it (docs/design/async-dsss-receiver.md §6)._  |
|  int | [**dp\_async\_dsss\_receiver\_set\_search\_only**](#function-dp_async_dsss_receiver_set_search_only) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, size\_t n\_picks) <br>_Make a receiver a controller's search: each dwell's pick runs for ranks 0 …_ `n_picks` _- 1 and the survivors are kept for take\_picks(), and the receiver never seeds itself._ |
|  size\_t | [**dp\_async\_dsss\_receiver\_take\_picks**](#function-dp_async_dsss_receiver_take_picks) ([**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* state, double \* chip\_phase, double \* doppler\_hz, double \* cn0\_dbhz, size\_t max) <br>_Take a search-only receiver's waiting picks, strongest first, and clear them._  |




























## Detailed Description


Not part of the receiver's public API: these are the pool's plumbing (docs/design/async-dsss-receiver.md §5, §6), declared here so the pool and the receiver's own test reach them and no caller is promised them. The pool is the one user, and it sets both at create. Both values still travel in the receiver's blob (v15)  a pooled receiver restores pooled and idle, a search restores its pick count  pinned by test\_async\_dsss\_receiver\_core's round trip. 


    
## Public Functions Documentation




### function dp\_async\_dsss\_receiver\_set\_pooled 

_Make a receiver a pooled one: while it holds nothing it is idle (ASYNC\_DSSS\_RX\_IDLE), consuming samples without searching, until a controller seeds it (docs/design/async-dsss-receiver.md §6)._ 
```C++
int dp_async_dsss_receiver_set_pooled (
    dp_async_dsss_receiver_state_t * state,
    bool on
) 
```



A pool's receivers do not search for themselves: the controller's one search finds the signals and seed() hands each to an idle receiver (§5). Pooled, reset() and a release with `reacquire` return the receiver to idle instead of to its own search, and seed() is taken while idle as while searching. Turning it on while searching goes idle at once; turning it off while idle restarts the search. A receiver that holds a signal keeps it either way.




**Parameters:**


* `state` Must be non-NULL. 
* `on` Pooled or not. 



**Returns:**

`DP_OK`. 





        

<hr>



### function dp\_async\_dsss\_receiver\_set\_search\_only 

_Make a receiver a controller's search: each dwell's pick runs for ranks 0 …_ `n_picks` _- 1 and the survivors are kept for take\_picks(), and the receiver never seeds itself._
```C++
int dp_async_dsss_receiver_set_search_only (
    dp_async_dsss_receiver_state_t * state,
    size_t n_picks
) 
```



The pool's one search (docs/design/async-dsss-receiver.md §5) is a receiver in this mode, fed the same samples as the pool's receivers. A dwell that decides inside a call runs the pick of §7.2 once per rank against the exclusions (set\_exclusions(), read at the call's first sample and carried forward with it), and each surviving peak is kept with its code phase carried to the sample after the call: the phase seed() wants on a receiver fed the same call. A peak outside the seed domain is counted in `refused_hits`, as a normal receiver counts it. Picks accumulate, up to ASYNC\_DSSS\_RX\_MAX\_PEAKS, until taken. 0 is a normal receiver again, its waiting picks dropped.




**Parameters:**


* `state` Must be non-NULL. 
* `n_picks` Ranks a dwell reports, at most ASYNC\_DSSS\_RX\_MAX\_PEAKS; 0 = off. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` (nothing changes) past the cap. 





        

<hr>



### function dp\_async\_dsss\_receiver\_take\_picks 

_Take a search-only receiver's waiting picks, strongest first, and clear them._ 
```C++
size_t dp_async_dsss_receiver_take_picks (
    dp_async_dsss_receiver_state_t * state,
    double * chip_phase,
    double * doppler_hz,
    double * cn0_dbhz,
    size_t max
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `chip_phase` Out: each pick's code phase at the sample after the call that found it, chips; at least `max`. 
* `doppler_hz` Out: its Doppler estimate, Hz. 
* `cn0_dbhz` Out: its C/N0 estimate, dB-Hz. 
* `max` Capacity of each array. 



**Returns:**

The picks written, at most `max`; any past it are dropped. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_private.h`

