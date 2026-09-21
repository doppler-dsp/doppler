

# File i16\_buffer\_core.h



[**FileList**](files.md) **>** [**i16\_buffer**](dir_214119e05624f58881fdbfa30e65f3ff.md) **>** [**i16\_buffer\_core.h**](i16__buffer__core_8h.md)

[Go to the source code of this file](i16__buffer__core_8h_source.md)

_The int16 I/Q pair ring as the component just-makeit binds._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "buffer/buffer.h"`
* `#include "dp_interrupt_guard/dp_interrupt_guard_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_iq16\_t**](structdp__iq16__t.md) <br>_One q15 complex sample: the element the i16 ring's view hands back._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef dp\_i16\_t | [**i16\_buffer\_state\_t**](#typedef-i16_buffer_state_t)  <br>_The component's state IS the ring._  |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_i16\_close**](#function-dp_i16_close) (dp\_i16\_t \* state) <br>_Say that no more data is coming._  |
|  int | [**dp\_i16\_consume**](#function-dp_i16_consume) (dp\_i16\_t \* state, size\_t n) <br>_Release_ `n` _samples back to the producer._ |
|  dp\_i16\_t \* | [**dp\_i16\_create**](#function-dp_i16_create) (size\_t capacity) <br>_Lock-free SPSC ring buffer for interleaved int16 IQ pairs._  |
|  void | [**dp\_i16\_destroy**](#function-dp_i16_destroy) (dp\_i16\_t \* state) <br>_Unmap the buffer and free the underlying struct._  |
|  [**dp\_iq16\_t**](structdp__iq16__t.md) \* | [**dp\_i16\_peek\_view**](#function-dp_i16_peek_view) (dp\_i16\_t \* state, size\_t n) <br>_:meth:_ `wait` _that never blocks: a view, or None for not yet._ |
|  void | [**dp\_i16\_reset**](#function-dp_i16_reset) (dp\_i16\_t \* state) <br>_Empty the ring and reopen it._  |
|  [**dp\_iq16\_t**](structdp__iq16__t.md) \* | [**dp\_i16\_wait\_view**](#function-dp_i16_wait_view) (dp\_i16\_t \* state, size\_t n) <br>_Block until_ `n` _samples are available, then lend a zero-copy view._ |
|  size\_t | [**dp\_i16\_write\_some\_view**](#function-dp_i16_write_some_view) (dp\_i16\_t \* state, const [**dp\_iq16\_t**](structdp__iq16__t.md) \* x, size\_t x\_len) <br>_Write as much of_ `x` _as fits and say how much that was._ |
|  bool | [**dp\_i16\_write\_view**](#function-dp_i16_write_view) (dp\_i16\_t \* state, const [**dp\_iq16\_t**](structdp__iq16__t.md) \* x, size\_t x\_len) <br>_Write IQ samples into the buffer without blocking._  |
|  size\_t | [**i16\_buffer\_get\_available**](#function-i16_buffer_get_available) (const [**i16\_buffer\_state\_t**](i16__buffer__core_8h.md#typedef-i16_buffer_state_t) \* state) <br>_Samples written but not yet consumed._  |
|  size\_t | [**i16\_buffer\_get\_capacity**](#function-i16_buffer_get_capacity) (const [**i16\_buffer\_state\_t**](i16__buffer__core_8h.md#typedef-i16_buffer_state_t) \* state) <br>_Buffer capacity in IQ sample pairs._  |
|  bool | [**i16\_buffer\_get\_closed**](#function-i16_buffer_get_closed) (const [**i16\_buffer\_state\_t**](i16__buffer__core_8h.md#typedef-i16_buffer_state_t) \* state) <br>`True` _once the producer has called :meth:_`close` _._ |
|  size\_t | [**i16\_buffer\_get\_dropped**](#function-i16_buffer_get_dropped) (const [**i16\_buffer\_state\_t**](i16__buffer__core_8h.md#typedef-i16_buffer_state_t) \* state) <br>_Cumulative IQ sample pairs in REFUSED writes_  _not pairs lost._ |
|  size\_t | [**i16\_buffer\_get\_space**](#function-i16_buffer_get_space) (const [**i16\_buffer\_state\_t**](i16__buffer__core_8h.md#typedef-i16_buffer_state_t) \* state) <br>_Free room in samples: the largest :meth:_ `write` _sure to fit._ |


























## Detailed Description


The ring itself is `dp_i16_*` in [**buffer/buffer.h**](buffer_8h.md), header-only and macro-stamped. This header is what makes it a jm component without changing it:



* `i16_buffer_state_t` IS `dp_i16_t`, so the binding holds the real ring and calls the real functions  nothing is wrapped.
* [**DECLARE\_DP\_BUFFER\_VIEW**](buffer_8h.md#define-declare_dp_buffer_view) stamps the element-typed face (one element per SAMPLE), which is the face a numpy array has.
* The Doxygen below sits on DECLARATIONS. The macros supply the definitions, but a doc extractor reads text, not the preprocessor's output, so the per-width documentation  and the Python examples the stub and `help()` both render  has to be written where it can be seen. The `<obj>_get_<prop>` accessors are the one thing defined here: they are jm's naming, not the ring's.




The two siblings (f32 / f64 / i16) are the same file over a different element; a manifest template (just-makeit#1310) will say so once. 


    
## Public Types Documentation




### typedef i16\_buffer\_state\_t 

_The component's state IS the ring._ 
```C++
typedef dp_i16_t i16_buffer_state_t;
```




<hr>
## Public Static Functions Documentation




### function dp\_i16\_close 

_Say that no more data is coming._ 
```C++
static inline void dp_i16_close (
    dp_i16_t * state
) 
```



The producer's half of end of stream. Until this exists a consumer cannot tell a slow producer from a finished one  both look like an empty ring  so :meth:`wait` had nothing to do but spin. Call it once, after the last write.


Release ordering: every sample written before this is visible to a consumer that observes the flag. Closing does not discard what was already written; :meth:`wait` keeps returning batches until the ring is drained, and only then raises `EOFError`.


See `docs/design/io-termination.md` for the one termination contract shared with the network and disk transports.



```C++
>>> import numpy as np
>>> from doppler.buffer import I16Buffer
>>> buf = I16Buffer(1024)
>>> buf.close()
>>> buf.closed
True
>>> buf.wait(4)
Traceback (most recent call last):
    ...
EOFError: end of stream: the producer closed the ring
```
 


        

<hr>



### function dp\_i16\_consume 

_Release_ `n` _samples back to the producer._
```C++
static inline int dp_i16_consume (
    dp_i16_t * state,
    size_t n
) 
```



Advances the consumer tail pointer by `n`, making that space available for the producer to overwrite, and ends the loan: the view a :meth:`wait` or :meth:`peek` lent must not be used afterwards. If `n` is omitted it is the count of that outstanding view, so the number is written once. `n` smaller than the view is how overlapped frames are read: release a hop, keep the rest.




**Parameters:**


* `state` The ring. Must be non-NULL. 
* `n` Number of samples to release. Defaults to the count of the outstanding :meth:`wait` / :meth:`peek` view.



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when `n` exceeds what is readable.




**Exception:**


* `ValueError` `n` exceeds :attr:`available`. Nothing is released: past that point the ring's counts would stop describing it. 
* `RuntimeError` `n` was omitted and nothing is outstanding  no view was lent since the last release, so there is no count to default to.


```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.write(np.array([1, 2, 3, 4], dtype=np.int16).view(IQ16))
True
>>> _ = buf.wait(2)
>>> buf.consume()
```
 


        

<hr>



### function dp\_i16\_create 

_Lock-free SPSC ring buffer for interleaved int16 IQ pairs._ 
```C++
static inline dp_i16_t * dp_i16_create (
    size_t capacity
) 
```



Stores raw 16-bit integer I/Q samples as they arrive from SDR hardware (e.g. RTL-SDR, HackRF) before conversion to floating point. Uses the same virtual-memory double-mapping as :class:`F32Buffer` to give zero-copy, branchless access across the wrap boundary.


numpy has no complex-integer dtype, so one sample is a RECORD: `[("i", "<i2"), ("q", "<i2")]`. Both faces speak it  :meth:`write` takes a 1-D array of it and :meth:`wait` lends one  so the ring is 1-D with one element per sample, exactly like its float siblings. The storage underneath is still interleaved int16 (I, Q, I, Q, ...), so `flat.view(IQ16)` and `view.view(np.int16)` convert either way with no copy.


A record rather than a packed `int32` on purpose: both are one element per sample, but `packed + 1` carries across the I/Q boundary and increments I only, silently. A record refuses arithmetic instead.




**Parameters:**


* `capacity` How many samples the ring holds: any size from 1 up, and :attr:`capacity` is exactly this number on every machine. What is rounded is the MAPPING behind it  up to a power of two, because indexing is a mask, and up to a whole page  so a capacity that is not a power of two costs some address space (under 2x) and nothing per call.


```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> buf = I16Buffer(1024)
>>> buf.capacity
1024
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> adc = np.array([10, 20, 30, 40], dtype=np.int16)   # I, Q, I, Q
>>> buf.write(adc.view(IQ16))
True
>>> buf.wait(2)["q"].tolist()
[20, 40]
```
 


        

<hr>



### function dp\_i16\_destroy 

_Unmap the buffer and free the underlying struct._ 
```C++
static inline void dp_i16_destroy (
    dp_i16_t * state
) 
```



Releases both virtual-address views and frees the C struct. Safe to call more than once; subsequent calls are no-ops.



```C++
>>> from doppler.buffer import I16Buffer
>>> buf = I16Buffer(1024)
>>> buf.destroy()
```
 


        

<hr>



### function dp\_i16\_peek\_view 

_:meth:_ `wait` _that never blocks: a view, or None for not yet._
```C++
static inline dp_iq16_t * dp_i16_peek_view (
    dp_i16_t * state,
    size_t n
) 
```



The single-threaded consumer's read. :meth:`wait` spins until a producer on _another_ thread delivers, so a caller that is its own producer would deadlock in it; `peek` answers at once instead. When `n` samples are buffered it returns the same zero-copy, always-contiguous view :meth:`wait` would (1-D, one `(i, q)` record per sample); otherwise it returns `None`.


`None` means **not yet** and nothing else. The two conditions no amount of waiting can cure are raised, exactly as :meth:`wait` raises them, so a poll loop cannot mistake either for a slow producer.


Peeking does not consume. Follow it with :meth:`consume`; a `consume(k)` with `k < n` advances by a hop smaller than the frame, which is how overlapped frames are read.




**Parameters:**


* `state` The ring. Must be non-NULL. 
* `n` Number of samples wanted. Must be positive and not larger than :attr:`capacity`.



**Returns:**

Zero-copy view of the next `n` samples, or `None` when fewer than `n` have been written so far.




**Exception:**


* `EOFError` The ring is closed and fewer than `n` samples remain: the rest is never coming. 
* `ValueError` `n` exceeds :attr:`capacity` (or is not positive), so no producer could ever satisfy it.


```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.peek(4) is None
True
>>> buf.write_some(np.ones(8, dtype=IQ16))
8
>>> buf.peek(4).shape
(4,)
>>> buf.consume(2)
>>> buf.available
6
>>> buf.close()
>>> buf.peek(8)
Traceback (most recent call last):
    ...
EOFError: end of stream: the producer closed the ring
```
 


        

<hr>



### function dp\_i16\_reset 

_Empty the ring and reopen it._ 
```C++
static inline void dp_i16_reset (
    dp_i16_t * state
) 
```



Discards everything buffered, and clears :attr:`closed` so the same ring can carry a second stream  without it, reuse after :meth:`close` means destroying and re-mapping. :attr:`dropped` is a lifetime count and is kept.


Not safe against a concurrent producer or consumer: it moves both ends of the ring. Call it only when both sides are idle.



```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.write_some(np.ones(8, dtype=IQ16))
8
>>> buf.close()
>>> buf.reset()
>>> buf.available, buf.closed
(0, False)
```
 


        

<hr>



### function dp\_i16\_wait\_view 

_Block until_ `n` _samples are available, then lend a zero-copy view._
```C++
static inline dp_iq16_t * dp_i16_wait_view (
    dp_i16_t * state,
    size_t n
) 
```



Spins with the GIL released until the producer has written at least `n` samples. Returns a 1-D record array directly into the double-mapped ring: `view["i"]` is the I channel, `view["q"]` the Q channel, each a strided int16 view with no copy. Caller must call :meth:`consume` before the next `wait`.




**Parameters:**


* `state` The ring. Must be non-NULL. 
* `n` Number of IQ sample pairs to wait for.



**Returns:**

Zero-copy view of the next `n` samples, one record each.




**Exception:**


* `EOFError` The producer called :meth:`close` and fewer than `n` samples remain. The tail is drained and no more is coming, so the wait ends rather than blocking forever. 
* `KeyboardInterrupt` Somebody asked this process to stop, through a :class:`doppler.interrupt.Interrupt` guard  from any module: the flag is process-wide. Without a guard the spin checks for no signals at all.


```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> buf = I16Buffer(1024)
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
True
>>> view = buf.wait(2)
>>> view.dtype
dtype([('i', '<i2'), ('q', '<i2')])
>>> view.shape
(2,)
>>> view.tolist()
[(10, 20), (30, 40)]
>>> buf.consume()
```
 


        

<hr>



### function dp\_i16\_write\_some\_view 

_Write as much of_ `x` _as fits and say how much that was._
```C++
static inline size_t dp_i16_write_some_view (
    dp_i16_t * state,
    const dp_iq16_t * x,
    size_t x_len
) 
```



The partial-write twin of :meth:`write`. Where :meth:`write` refuses a block that does not fit whole, this takes the leading samples that do and returns their count  `0` when the ring is full. It never refuses, so it never touches :attr:`dropped`. It is the only way to feed a chunk larger than the ring: loop, advancing by the return value, draining in between.




**Parameters:**


* `state` The ring. Must be non-NULL. 
* `x` Samples to write: 1-D, C-contiguous, dtype `[("i", "<i2"), ("q", "<i2")]`. 
* `x_len` Length of `x`, in samples.



**Returns:**

Samples accepted, `0 <= k <= len(x)`. The caller still owns `x[k:]`.



```C++
A chunk three times the size of the ring, fed by looping:

>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> cap = buf.capacity
>>> chunk = np.ones(3 * cap, dtype=IQ16)
>>> fed = 0
>>> while fed < len(chunk):
...     fed += buf.write_some(chunk[fed:])
...     _ = buf.peek(buf.available); buf.consume()
>>> fed == 3 * cap, buf.dropped
(True, 0)
```
 


        

<hr>



### function dp\_i16\_write\_view 

_Write IQ samples into the buffer without blocking._ 
```C++
static inline bool dp_i16_write_view (
    dp_i16_t * state,
    const dp_iq16_t * x,
    size_t x_len
) 
```



Copies the record array into the ring in a single `memcpy`. With fewer than `len(x)` free slots the call is **refused entirely**  nothing copied, `x` untouched  and :attr:`dropped` grows by `len(x)`, which is not a loss count: you still hold every sample.


A bare int16 array is refused with `TypeError`, flat or `(n, 2)`: it is not an array of samples. `flat.view(IQ16)` makes it one, with no copy.




**Parameters:**


* `state` The ring. Must be non-NULL. 
* `x` IQ samples to write: 1-D, C-contiguous, dtype `[("i", "<i2"), ("q", "<i2")]`. 
* `x_len` Length of `x`, in samples.



**Returns:**

`True` if all samples were written; `False` if the ring had no room and the call was refused (`x` untouched).



```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> buf = I16Buffer(1024)
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
True
>>> buf2 = I16Buffer(1024)
>>> buf2.write(np.zeros(1024, dtype=IQ16))
True
>>> buf2.write(np.zeros(1, dtype=IQ16))
False
```
 


        

<hr>



### function i16\_buffer\_get\_available 

_Samples written but not yet consumed._ 
```C++
static inline size_t i16_buffer_get_available (
    const i16_buffer_state_t * state
) 
```



The largest `n` for which :meth:`wait` is guaranteed to return without spinning. Read this rather than tracking the count yourself: :meth:`wait` has no timeout and no short return, so asking for more than has been written spins until the producer catches up  forever, if there is no producer.


Read from the consumer side this is a _lower_ bound. A producer on another thread can only increase it, so a block sized from it is always safe; it may simply be smaller than what has landed by the time :meth:`wait` runs.



```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.available
0
>>> _ = buf.write(np.zeros(100, dtype=IQ16))
>>> buf.available
100
>>> _ = buf.wait(64); buf.consume(64)
>>> buf.available
36
```
 


        

<hr>



### function i16\_buffer\_get\_capacity 

_Buffer capacity in IQ sample pairs._ 
```C++
static inline size_t i16_buffer_get_capacity (
    const i16_buffer_state_t * state
) 
```



Read-only. Exactly the number passed to the constructor, whatever the machine's page size; the mapping behind it is larger when that number is not a power of two or spans less than a page, and that slack is never room.



```C++
>>> from doppler.buffer import I16Buffer
>>> I16Buffer(1024).capacity, I16Buffer(1000).capacity
(1024, 1000)
```
 


        

<hr>



### function i16\_buffer\_get\_closed 

`True` _once the producer has called :meth:_`close` _._
```C++
static inline bool i16_buffer_get_closed (
    const i16_buffer_state_t * state
) 
```



The consumer's half of end of stream: it distinguishes "the
producer is slow" from "the producer has finished", which an empty ring alone cannot.



```C++
>>> from doppler.buffer import I16Buffer
>>> buf = I16Buffer(1024)
>>> buf.closed
False
>>> buf.close()
>>> buf.closed
True
```
 


        

<hr>



### function i16\_buffer\_get\_dropped 

_Cumulative IQ sample pairs in REFUSED writes_  _not pairs lost._
```C++
static inline size_t i16_buffer_get_dropped (
    const i16_buffer_state_t * state
) 
```



**Not a count of lost data.** :meth:`write` is all-or-nothing: with no room it copies nothing, leaves the caller's array untouched and refuses the call  and this counter is then incremented by the length of that refused call, not by 1 and not by anything actually lost.


So a producer that spins on :meth:`write` until it succeeds, the obvious way to apply backpressure, inflates this while losing nothing: a 60,000-pair run written that way reported 5,960,438. Samples are lost only when the caller _discards_ them, which is what ignoring the return value does. Wait for room if you want this to mean what it sounds like.



```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.dropped
0
>>> buf.write(np.zeros(1024, dtype=IQ16))
True
>>> buf.write(np.zeros(3, dtype=IQ16))
False
>>> buf.dropped
1
```
 


        

<hr>



### function i16\_buffer\_get\_space 

_Free room in samples: the largest :meth:_ `write` _sure to fit._
```C++
static inline size_t i16_buffer_get_space (
    const i16_buffer_state_t * state
) 
```



`capacity - available`, read in one place so callers stop deriving it. Read from the producer side it is a _lower_ bound: a consumer on another thread can only increase it, so a block sized from it is always accepted.



```C++
>>> from doppler.buffer import I16Buffer
>>> import numpy as np
>>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
>>> buf = I16Buffer(1024)
>>> buf.space == buf.capacity
True
>>> buf.write_some(np.ones(8, dtype=IQ16))
8
>>> buf.capacity - buf.space
8
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/i16_buffer/i16_buffer_core.h`

