

# File syncword\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**syncword**](dir_e299403a0e03806f7aa46ec15fc4c91a.md) **>** [**syncword\_core.h**](syncword__core_8h.md)

[Go to the source code of this file](syncword__core_8h_source.md)

_Frame synchronisation: find a known marker in a bit stream, and choose the threshold that decides what counts as finding it._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_syncword.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/detection/detection_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) <br>_A searcher for one marker._  |
| struct | [**syncword\_hit\_t**](structsyncword__hit__t.md) <br>_What_ [_**dp\_syncword\_find**_](syncword__core_8h.md#function-dp_syncword_find) _found._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) \* | [**dp\_syncword\_create**](#function-dp_syncword_create) (const uint8\_t \* marker, size\_t marker\_len) <br>_Create a searcher for_ `marker` _._ |
|  void | [**dp\_syncword\_destroy**](#function-dp_syncword_destroy) ([**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) \* state) <br>_Destroy a searcher and release all memory._  |
|  [**syncword\_hit\_t**](structsyncword__hit__t.md) | [**dp\_syncword\_find**](#function-dp_syncword_find) ([**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) \* state, const uint8\_t \* bits, size\_t bits\_len, uint32\_t max\_errors) <br>_Find the first marker in_ `bits` _, either polarity._ |
|  int | [**dp\_syncword\_max\_errors\_for**](#function-dp_syncword_max_errors_for) ([**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) \* state, size\_t window\_bits, double pfa) <br>_The largest tolerance whose false-frame rate over a search window still meets_ `pfa` _._ |
|  double | [**dp\_syncword\_pfa**](#function-dp_syncword_pfa) ([**dp\_syncword\_state\_t**](structdp__syncword__state__t.md) \* state, uint32\_t max\_errors) <br>_Probability that ONE random offset false-hits this marker at a tolerance of_ `max_errors` _._ |




























## Detailed Description


`dp_syncword.h` owns the kernel — correlate a known pattern against every bit offset, in both polarities, and report the first offset close enough. This owns the DETECTOR built over one: a caller names the marker and gets a searcher for it, plus the arithmetic for setting its tolerance. Nothing here knows about CCSDS, which is a configuration of the same kernel (see `ccsds_tm`); reach it with `doppler.ccsds.asm_bits()`.


### The threshold is not a property of the marker



`max_errors` is the whole of the trade, and the number a caller needs is a function of **how much stream they search**, not of how long the marker is. A 32-bit marker invites "half of 32 is 16, so 8 sounds safe", and 8 finds the marker at its true offset only 58 % of the time on a stream with no channel errors at all — because the search reports the FIRST acceptable offset, and each of the offsets ahead of the real one is an independent chance to false-hit first (doppler#897).


So `pfa` and `max_errors_for` sit beside the search, answering FOR the marker being searched — the same pairing `det_threshold` has with `det_pd` in this module.


Bit convention: **unpacked** bits, one per byte in the LSB, which is what `wfm_frame_bits`, `dp_crc16_ccitt` and `ccsds_tm_randomise` already pass around.


Lifecycle: `create -> [find / pfa / max_errors_for]* -> destroy`.



```C++
>>> import numpy as np
>>> from doppler.detection import SyncFinder
>>> from doppler.ccsds import asm_bits
>>> asm = asm_bits()
>>> f = SyncFinder(asm)
>>> rx = np.concatenate([np.zeros(96, np.uint8), asm])
>>> hit = f.find(rx, max_errors=4)
>>> hit.found, hit.offset, hit.inverted, hit.errors
(1, 96, 0, 0)
```
 



    
## Public Functions Documentation




### function dp\_syncword\_create 

_Create a searcher for_ `marker` _._
```C++
dp_syncword_state_t * dp_syncword_create (
    const uint8_t * marker,
    size_t marker_len
) 
```



The marker is COPIED. A searcher outlives the array it was built from, which is what lets a caller construct one from a temporary — the CCSDS marker arrives from `dp_asm_bits()` as exactly that.




**Parameters:**


* `marker` Unpacked bits, one per byte; only the LSB is used. 
* `marker_len` Marker length in bits; must be non-zero. 



**Returns:**

Heap-allocated state, or NULL for an empty marker or on allocation failure. 




**Note:**

Caller must call [**dp\_syncword\_destroy()**](syncword__core_8h.md#function-dp_syncword_destroy) when done.



```C++
  >>> import numpy as np
  >>> from doppler.detection import SyncFinder
  >>> from doppler.ccsds import asm_bits
>>> asm = asm_bits()          # 0x1ACFFC1D, no transcription
  >>> f = SyncFinder(asm)
  >>> f.nbits
  32
  >>> rx = np.concatenate([np.zeros(96, np.uint8), asm])
  >>> hit = f.find(rx, max_errors=f.max_errors_for(96, pfa=1e-3))
  >>> hit.found, hit.offset, hit.inverted
  (1, 96, 0)
```
 


        

<hr>



### function dp\_syncword\_destroy 

_Destroy a searcher and release all memory._ 
```C++
void dp_syncword_destroy (
    dp_syncword_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_syncword\_find 

_Find the first marker in_ `bits` _, either polarity._
```C++
syncword_hit_t dp_syncword_find (
    dp_syncword_state_t * state,
    const uint8_t * bits,
    size_t bits_len,
    uint32_t max_errors
) 
```



The FIRST offset whose Hamming distance to the marker, or to its complement, is at most `max_errors`. First rather than best, because a best-match search has to see the whole stream before it can answer and a synchroniser reading a live capture cannot wait for that.


Choose `max_errors` with `max_errors_for`, against the window this caller actually searches — the marker length is the wrong thing to halve.




**Parameters:**


* `state` The searcher. 
* `bits` Unpacked bits, one per byte. 
* `bits_len` Number of bits. 
* `max_errors` Largest tolerated Hamming distance, in bits. 



**Returns:**

A record whose `found` says whether the rest of it means anything; a miss returns it zeroed.



```C++
>>> import numpy as np
>>> from doppler.detection import SyncFinder
>>> m = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
>>> rx = np.concatenate([np.zeros(20, np.uint8), 1 - m])
>>> hit = SyncFinder(m).find(rx, max_errors=1)
>>> hit.found, hit.offset, hit.inverted
(1, 20, 1)
```
 


        

<hr>



### function dp\_syncword\_max\_errors\_for 

_The largest tolerance whose false-frame rate over a search window still meets_ `pfa` _._
```C++
int dp_syncword_max_errors_for (
    dp_syncword_state_t * state,
    size_t window_bits,
    double pfa
) 
```



The question `find`'s signature cannot ask. Every offset ahead of the true marker is an independent chance to win the race, so the probability the window produces a false frame is `1 - (1 - pfa(t))^window_bits`, which rises with `t`. The largest `t` that still holds is the most tolerant threshold a caller can afford — and it falls as they search further, which is the whole of doppler#897.




**Parameters:**


* `state` The searcher. 
* `window_bits` Offsets tried AHEAD of the marker: the length of stream searched, not the length of the frame. 
* `pfa` Tolerated probability of a false frame over that window. 



**Returns:**

Tolerance in bits, or -1 when even an exact match exceeds `pfa` over that window.



```C++
>>> from doppler.detection import SyncFinder
>>> from doppler.ccsds import asm_bits
>>> f = SyncFinder(asm_bits())
>>> f.max_errors_for(window_bits=96, pfa=1e-3)
3
>>> f.max_errors_for(window_bits=100000, pfa=1e-3)   # search further
0
```
 


        

<hr>



### function dp\_syncword\_pfa 

_Probability that ONE random offset false-hits this marker at a tolerance of_ `max_errors` _._
```C++
double dp_syncword_pfa (
    dp_syncword_state_t * state,
    uint32_t max_errors
) 
```



`2 * sum_{i <= max_errors} C(n, i) / 2^n`, the factor of two because `find` searches the complement too. Measured against the 32-bit CCSDS marker, this tracks the observed false-alarm rate to within 20 % at every threshold where the count supports a rate (`src/doppler/tests/validation/ccsds_tm/results.md` §2.2).


This is the PER-OFFSET number. What a synchroniser cares about is its whole window; `max_errors_for` is this inverted through it.




**Parameters:**


* `state` The searcher. 
* `max_errors` Tolerance in bits. 



**Returns:**

Probability in &#91;0, 1&#93;.



```C++
>>> import numpy as np
>>> from doppler.detection import SyncFinder
>>> from doppler.ccsds import asm_bits
>>> f = SyncFinder(asm_bits())
>>> # the marker and its complement, out of 2**32 windows
>>> round(f.pfa(0) * 2**32)
2
>>> # ...plus each one's 32 one-bit neighbours
>>> round(f.pfa(1) * 2**32)
66
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/syncword/syncword_core.h`

