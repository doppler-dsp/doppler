

# File f32\_buffer\_core.h



[**FileList**](files.md) **>** [**f32\_buffer**](dir_73bc8939a0d066ce4b56550e20e88de7.md) **>** [**f32\_buffer\_core.h**](f32__buffer__core_8h.md)

[Go to the source code of this file](f32__buffer__core_8h_source.md)

_The complex64 ring as the component just-makeit binds._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "buffer/buffer.h"`
* `#include "dp_interrupt_guard/dp_interrupt_guard_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef dp\_f32\_t | [**f32\_buffer\_state\_t**](#typedef-f32_buffer_state_t)  <br>_The component's state IS the ring._  |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_f32\_close**](#function-dp_f32_close) (dp\_f32\_t \* state) <br>_Say that no more data is coming._  |
|  void | [**dp\_f32\_consume**](#function-dp_f32_consume) (dp\_f32\_t \* state, size\_t n) <br>_Release_ `n` _samples back to the producer._ |
|  dp\_f32\_t \* | [**dp\_f32\_create**](#function-dp_f32_create) (size\_t capacity) <br>_Lock-free SPSC ring buffer for complex64 (CF32) samples._  |
|  void | [**dp\_f32\_destroy**](#function-dp_f32_destroy) (dp\_f32\_t \* state) <br>_Unmap the double-mapped region and free the buffer struct._  |
|  float \_Complex \* | [**dp\_f32\_peek\_view**](#function-dp_f32_peek_view) (dp\_f32\_t \* state, size\_t n) <br>_:meth:_ `wait` _that never blocks: a view, or None for not yet._ |
|  void | [**dp\_f32\_reset**](#function-dp_f32_reset) (dp\_f32\_t \* state) <br>_Empty the ring and reopen it._  |
|  float \_Complex \* | [**dp\_f32\_wait\_view**](#function-dp_f32_wait_view) (dp\_f32\_t \* state, size\_t n) <br>_Block until_ `n` _samples are available, then return a zero-copy view._ |
|  size\_t | [**dp\_f32\_write\_some\_view**](#function-dp_f32_write_some_view) (dp\_f32\_t \* state, const float \_Complex \* x, size\_t x\_len) <br>_Write as much of_ `x` _as fits and say how much that was._ |
|  bool | [**dp\_f32\_write\_view**](#function-dp_f32_write_view) (dp\_f32\_t \* state, const float \_Complex \* x, size\_t x\_len) <br>_Write samples into the buffer without blocking._  |
|  size\_t | [**f32\_buffer\_get\_available**](#function-f32_buffer_get_available) (const [**f32\_buffer\_state\_t**](f32__buffer__core_8h.md#typedef-f32_buffer_state_t) \* state) <br>_Samples written but not yet consumed._  |
|  size\_t | [**f32\_buffer\_get\_capacity**](#function-f32_buffer_get_capacity) (const [**f32\_buffer\_state\_t**](f32__buffer__core_8h.md#typedef-f32_buffer_state_t) \* state) <br>_Buffer capacity in complex samples._  |
|  bool | [**f32\_buffer\_get\_closed**](#function-f32_buffer_get_closed) (const [**f32\_buffer\_state\_t**](f32__buffer__core_8h.md#typedef-f32_buffer_state_t) \* state) <br>`True` _once the producer has called :meth:_`close` _._ |
|  size\_t | [**f32\_buffer\_get\_dropped**](#function-f32_buffer_get_dropped) (const [**f32\_buffer\_state\_t**](f32__buffer__core_8h.md#typedef-f32_buffer_state_t) \* state) <br>_Cumulative samples in REFUSED writes_  _not samples lost._ |
|  size\_t | [**f32\_buffer\_get\_space**](#function-f32_buffer_get_space) (const [**f32\_buffer\_state\_t**](f32__buffer__core_8h.md#typedef-f32_buffer_state_t) \* state) <br>_Free room in samples: the largest :meth:_ `write` _sure to fit._ |


























## Detailed Description


The ring itself is `dp_f32_*` in [**buffer/buffer.h**](buffer_8h.md), header-only and macro-stamped. This header is what makes it a jm component without changing it:



* `f32_buffer_state_t` IS `dp_f32_t`, so the binding holds the real ring and calls the real functions  nothing is wrapped.
* [**DECLARE\_DP\_BUFFER\_VIEW**](buffer_8h.md#define-declare_dp_buffer_view) stamps the element-typed face (one element per SAMPLE), which is the face a numpy array has.
* The Doxygen below sits on DECLARATIONS. The macros supply the definitions, but a doc extractor reads text, not the preprocessor's output, so the per-width documentation  and the Python examples the stub and `help()` both render  has to be written where it can be seen. The `<obj>_get_<prop>` accessors are the one thing defined here: they are jm's naming, not the ring's.




The two siblings (f32 / f64 / i16) are the same file over a different element; a manifest template (just-makeit#1310) will say so once. 


    
## Public Types Documentation




### typedef f32\_buffer\_state\_t 

_The component's state IS the ring._ 
```C++
typedef dp_f32_t f32_buffer_state_t;
```




<hr>
## Public Static Functions Documentation




### function dp\_f32\_close 

_Say that no more data is coming._ 
```C++
static inline void dp_f32_close (
    dp_f32_t * state
) 
```



The producer's half of end of stream. Until this exists a consumer cannot tell a slow producer from a finished one  both look like an empty ring  so :meth:`wait` had nothing to do but spin. Call it once, after the last write.


Release ordering: every sample written before this is visible to a consumer that observes the flag. Closing does not discard what was already written; :meth:`wait` keeps returning batches until the ring is drained, and only then raises `EOFError`.


See `docs/design/io-termination.md` for the one termination contract shared with the network and disk transports.



```C++
>>> import numpy as np
>>> from doppler.buffer import F32Buffer
>>> buf = F32Buffer(1024)
>>> buf.close()
>>> buf.closed
True
>>> buf.wait(4)
Traceback (most recent call last):
    ...
EOFError: end of stream: the producer closed the ring
```
 


        

<hr>



### function dp\_f32\_consume 

_Release_ `n` _samples back to the producer._
```C++
static inline void dp_f32_consume (
    dp_f32_t * state,
    size_t n
) 
```



Advances the consumer tail pointer by `n`, making that space available for the producer to overwrite, and ends the loan: the view a :meth:`wait` or :meth:`peek` lent must not be used afterwards. If `n` is omitted it is the count of that outstanding view, so the number is written once. `n` smaller than the view is how overlapped frames are read: release a hop, keep the rest.




**Parameters:**


* `n` Number of samples to release. Defaults to the count of the outstanding :meth:`wait` / :meth:`peek` view.



**Exception:**


* `RuntimeError` `n` was omitted and nothing is outstanding  no view was lent since the last release, so there is no count to default to.


```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.write(np.ones(4, dtype=np.complex64))
True
>>> _ = buf.wait(4)
>>> buf.consume()
```
 


        

<hr>



### function dp\_f32\_create 

_Lock-free SPSC ring buffer for complex64 (CF32) samples._ 
```C++
static inline dp_f32_t * dp_f32_create (
    size_t capacity
) 
```



Uses virtual-memory double-mapping so the consumer always sees a contiguous window across the wrap boundary. The same physical pages are mapped twice at adjacent virtual addresses, so a read that crosses the end of the ring returns data from the beginning without any memcpy or branch. Intended for single-producer / single-consumer use; do not share one instance between multiple producer threads or multiple consumer threads.


Head and tail indices are separated by a full cache line (64 bytes) to prevent false-sharing between the producer and consumer cores. On x86-64 the spin-wait loop in :meth:`wait` uses `PAUSE` to reduce power consumption and avoid branch-predictor pollution.




**Parameters:**


* `capacity` Requested buffer size in complex samples. Must be a power of two. The VM mirror is built at page granularity, so `capacity * 8` must span a whole page; a sub-page request is rounded **up** to the smallest power-of-two that does (minimum 512 on 4 KiB pages, 2048 on 16 KiB pages such as macOS arm64). Read :attr:`capacity` back for the size actually allocated.


```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.capacity >= 1024
True
>>> buf.write(np.ones(512, dtype=np.complex64))
True
```
 


        

<hr>



### function dp\_f32\_destroy 

_Unmap the double-mapped region and free the buffer struct._ 
```C++
static inline void dp_f32_destroy (
    dp_f32_t * state
) 
```



Releases both virtual-address views via `munmap` (POSIX) or `UnmapViewOfFile` (Windows) and frees the struct allocated by the constructor. After calling `destroy` the object must not be used again. Calling `destroy` more than once is safe; the second call is a no-op.



```C++
>>> from doppler.buffer import F32Buffer
>>> buf = F32Buffer(1024)
>>> buf.destroy()
```
 


        

<hr>



### function dp\_f32\_peek\_view 

_:meth:_ `wait` _that never blocks: a view, or None for not yet._
```C++
static inline float _Complex * dp_f32_peek_view (
    dp_f32_t * state,
    size_t n
) 
```



The single-threaded consumer's read. :meth:`wait` spins until a producer on _another_ thread delivers, so a caller that is its own producer would deadlock in it; `peek` answers at once instead. When `n` samples are buffered it returns the same zero-copy, always-contiguous view :meth:`wait` would (1-D complex64); otherwise it returns `None`.


`None` means **not yet** and nothing else. The two conditions no amount of waiting can cure are raised, exactly as :meth:`wait` raises them, so a poll loop cannot mistake either for a slow producer.


Peeking does not consume. Follow it with :meth:`consume`; a `consume(k)` with `k < n` advances by a hop smaller than the frame, which is how overlapped frames are read.




**Parameters:**


* `n` Number of samples wanted. Must be positive and not larger than :attr:`capacity`.



**Returns:**

Zero-copy view of the next `n` samples, or `None` when fewer than `n` have been written so far.




**Exception:**


* `EOFError` The ring is closed and fewer than `n` samples remain: the rest is never coming. 
* `ValueError` `n` exceeds :attr:`capacity` (or is not positive), so no producer could ever satisfy it.


```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.peek(4) is None
True
>>> buf.write_some(np.ones(8, dtype=np.complex64))
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



### function dp\_f32\_reset 

_Empty the ring and reopen it._ 
```C++
static inline void dp_f32_reset (
    dp_f32_t * state
) 
```



Discards everything buffered, and clears :attr:`closed` so the same ring can carry a second stream  without it, reuse after :meth:`close` means destroying and re-mapping. :attr:`dropped` is a lifetime count and is kept.


Not safe against a concurrent producer or consumer: it moves both ends of the ring. Call it only when both sides are idle.



```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.write_some(np.ones(8, dtype=np.complex64))
8
>>> buf.close()
>>> buf.reset()
>>> buf.available, buf.closed
(0, False)
```
 


        

<hr>



### function dp\_f32\_wait\_view 

_Block until_ `n` _samples are available, then return a zero-copy view._
```C++
static inline float _Complex * dp_f32_wait_view (
    dp_f32_t * state,
    size_t n
) 
```



Spins (releasing the GIL so a producer thread can run concurrently) until at least `n` samples have been written by the producer. Returns a 1-D complex64 NumPy array that is a _direct view_ into the double-mapped ring buffer — no data is copied. Because of the double-mapping, the view is always contiguous even when the requested range wraps around the physical end of the ring.


The caller **must** call :meth:`consume` before the next call to `wait`. Using the returned array after `consume` is undefined behaviour; the producer may overwrite it at any time.




**Parameters:**


* `n` Number of complex samples to wait for. Must be positive and not larger than :attr:`capacity`.



**Returns:**

Zero-copy view of the next `n` samples in the ring.




**Exception:**


* `EOFError` The producer called :meth:`close` and fewer than `n` samples remain. The tail is drained and no more is coming, so the wait ends rather than blocking forever. 
* `KeyboardInterrupt` Somebody asked this process to stop, through a :class:`doppler.interrupt.Interrupt` guard  from any module: the flag is process-wide. Without a guard the spin checks for no signals at all.


```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.write(np.array([1+2j, 3+4j, 5+6j], dtype=np.complex64))
True
>>> view = buf.wait(3)
>>> view.dtype
dtype('complex64')
>>> view.shape
(3,)
>>> view.tolist()
[(1+2j), (3+4j), (5+6j)]
>>> buf.consume(3)
```
 


        

<hr>



### function dp\_f32\_write\_some\_view 

_Write as much of_ `x` _as fits and say how much that was._
```C++
static inline size_t dp_f32_write_some_view (
    dp_f32_t * state,
    const float _Complex * x,
    size_t x_len
) 
```



The partial-write twin of :meth:`write`. Where :meth:`write` refuses a block that does not fit whole, this takes the leading samples that do and returns their count  `0` when the ring is full. It never refuses, so it never touches :attr:`dropped`. It is the only way to feed a chunk larger than the ring: loop, advancing by the return value, draining in between.




**Parameters:**


* `x` Samples to write. Must be 1-D and C-contiguous.



**Returns:**

Samples accepted, `0 <= k <= len(x)`. The caller still owns `x[k:]`.



```C++
A chunk three times the size of the ring, fed by looping:

>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> cap = buf.capacity
>>> chunk = np.ones(3 * cap, dtype=np.complex64)
>>> fed = 0
>>> while fed < len(chunk):
...     fed += buf.write_some(chunk[fed:])
...     _ = buf.peek(buf.available); buf.consume()
>>> fed == 3 * cap, buf.dropped
(True, 0)
```
 


        

<hr>



### function dp\_f32\_write\_view 

_Write samples into the buffer without blocking._ 
```C++
static inline bool dp_f32_write_view (
    dp_f32_t * state,
    const float _Complex * x,
    size_t x_len
) 
```



Copies the complex64 array into the ring buffer in a single `memcpy`. If there is not enough free space for all `len(x)` samples the call is **refused entirely** — nothing is copied and `x` is untouched, so you still hold every sample and may retry once the consumer has made room. Nothing is dropped unless you discard it; the refusal is counted in :attr:`dropped`, which is not a loss count. The array must be 1-D and C-contiguous.




**Parameters:**


* `x` Samples to write. Must be 1-D and C-contiguous.



**Returns:**

`True` if all samples were written; `False` if the ring had no room and the call was refused (`x` untouched).



```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex64))
True
>>> buf2 = F32Buffer(1024)
>>> buf2.write(np.zeros(1024, dtype=np.complex64))
True
>>> buf2.write(np.zeros(1, dtype=np.complex64))
False
```
 


        

<hr>



### function f32\_buffer\_get\_available 

_Samples written but not yet consumed._ 
```C++
static inline size_t f32_buffer_get_available (
    const f32_buffer_state_t * state
) 
```



The largest `n` for which :meth:`wait` is guaranteed to return without spinning. Read this rather than tracking the count yourself: :meth:`wait` has no timeout and no short return, so asking for more than has been written spins until the producer catches up  forever, if there is no producer.


Read from the consumer side this is a _lower_ bound. A producer on another thread can only increase it, so a block sized from it is always safe; it may simply be smaller than what has landed by the time :meth:`wait` runs.



```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.available
0
>>> _ = buf.write(np.zeros(100, dtype=np.complex64))
>>> buf.available
100
>>> _ = buf.wait(64); buf.consume(64)
>>> buf.available
36
```
 


        

<hr>



### function f32\_buffer\_get\_capacity 

_Buffer capacity in complex samples._ 
```C++
static inline size_t f32_buffer_get_capacity (
    const f32_buffer_state_t * state
) 
```



Read-only. Set at construction time and never changes. This is the _actual_ allocated size: a sub-page request is rounded up to the page-spanning minimum (512 on 4 KiB pages, 2048 on 16 KiB pages), so it may exceed the value passed to the constructor.



```C++
>>> from doppler.buffer import F32Buffer
>>> F32Buffer(1024).capacity >= 1024
True
```
 


        

<hr>



### function f32\_buffer\_get\_closed 

`True` _once the producer has called :meth:_`close` _._
```C++
static inline bool f32_buffer_get_closed (
    const f32_buffer_state_t * state
) 
```



The consumer's half of end of stream: it distinguishes "the
producer is slow" from "the producer has finished", which an empty ring alone cannot.



```C++
>>> from doppler.buffer import F32Buffer
>>> buf = F32Buffer(1024)
>>> buf.closed
False
>>> buf.close()
>>> buf.closed
True
```
 


        

<hr>



### function f32\_buffer\_get\_dropped 

_Cumulative samples in REFUSED writes_  _not samples lost._
```C++
static inline size_t f32_buffer_get_dropped (
    const f32_buffer_state_t * state
) 
```



**Not a count of lost data.** :meth:`write` is all-or-nothing: with no room it copies nothing, leaves the caller's array untouched and refuses the call  and this counter is then incremented by the length of that refused call, not by 1 and not by anything actually lost.


So a producer that spins on :meth:`write` until it succeeds, the obvious way to apply backpressure, inflates this while losing nothing: a 60,000-sample run written that way reported 5,960,438. Samples are lost only when the caller _discards_ them, which is what ignoring the return value does. Wait for room if you want this to mean what it sounds like.


Resets to zero only when the object is recreated.



```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.dropped
0
>>> buf.write(np.zeros(1024, dtype=np.complex64))
True
>>> buf.write(np.zeros(3, dtype=np.complex64))
False
>>> buf.dropped
3
```
 


        

<hr>



### function f32\_buffer\_get\_space 

_Free room in samples: the largest :meth:_ `write` _sure to fit._
```C++
static inline size_t f32_buffer_get_space (
    const f32_buffer_state_t * state
) 
```



`capacity - available`, read in one place so callers stop deriving it. Read from the producer side it is a _lower_ bound: a consumer on another thread can only increase it, so a block sized from it is always accepted.



```C++
>>> from doppler.buffer import F32Buffer
>>> import numpy as np
>>> buf = F32Buffer(1024)
>>> buf.space == buf.capacity
True
>>> buf.write_some(np.ones(8, dtype=np.complex64))
8
>>> buf.capacity - buf.space
8
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_buffer/f32_buffer_core.h`

