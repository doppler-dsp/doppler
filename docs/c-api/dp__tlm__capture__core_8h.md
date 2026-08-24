

# File dp\_tlm\_capture\_core.h



[**FileList**](files.md) **>** [**dp\_tlm\_capture**](dir_c53721efa35f9e05ec164f1aacd6bf30.md) **>** [**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md)

[Go to the source code of this file](dp__tlm__capture__core_8h_source.md)

_Lossless telemetry capture: sized by arithmetic, not by guesswork._ [More...](#detailed-description)

* `#include "dp_tlm/dp_tlm_core.h"`
* `#include "timing/timing_core.h"`
* `#include "dp_interrupt_guard/dp_interrupt_guard_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) | [**dp\_tlm\_capture\_state\_t**](#typedef-dp_tlm_capture_state_t)  <br>_jm's spelling of_ [_**dp\_tlm\_capture\_t**_](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) _._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_tlm\_capture\_block**](#function-dp_tlm_capture_block) ([**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Block boundary: drains the ring to empty._  |
|  int | [**dp\_tlm\_capture\_close**](#function-dp_tlm_capture_close) ([**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Final boundary, then flush, join, and write the sidecar._  |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**dp\_tlm\_capture\_context**](#function-dp_tlm_capture_context) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_The context this capture drains, borrowed._  |
|  size\_t | [**dp\_tlm\_capture\_count**](#function-dp_tlm_capture_count) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Records captured so far, across memory and file alike._  |
|  int | [**dp\_tlm\_capture\_destroy**](#function-dp_tlm_capture_destroy) ([**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Closes if still open, then frees. NULL-safe._  |
|  uint64\_t | [**dp\_tlm\_capture\_dropped**](#function-dp_tlm_capture_dropped) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Records the ring dropped during this capture._  |
|  [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* | [**dp\_tlm\_capture\_open**](#function-dp_tlm_capture_open) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t block\_samples, const char \* path, const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* clock) <br>_Opens a lossless capture over_ `t` _and arms the boundary drain._ |
|  [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* | [**dp\_tlm\_capture\_open\_memory**](#function-dp_tlm_capture_open_memory) ([**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* t, size\_t block\_samples, const [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) \* clock) <br>_Opens a capture that accumulates in memory instead of a file._  |
|  size\_t | [**dp\_tlm\_capture\_read**](#function-dp_tlm_capture_read) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c, size\_t n, [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) \* out, size\_t max\_out) <br>_Copies accumulated records out. Memory mode only._  |
|  size\_t | [**dp\_tlm\_capture\_read\_max\_out**](#function-dp_tlm_capture_read_max_out) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_Upper bound on what_ [_**dp\_tlm\_capture\_read()**_](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_read) _can return right now._ |
|  const [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) \* | [**dp\_tlm\_capture\_records**](#function-dp_tlm_capture_records) (const [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* c) <br>_The accumulated records, contiguous and in emission order._  |




























## Detailed Description


The ring ([**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)) drops on overrun so the DSP thread can never stall — right for the emit path, useless as an answer to "did I get
everything?". Every capture so far hand-rolled the same loop (`set_now`, step, `read`, append, concatenate) and asked the user to pick a ring size, which is a question nobody can answer: too small silently loses data, too big wastes memory, and neither shows up until after the run.


A capture retires the question. It rests on one bound ([**dp\_tlm\_block\_bound()**](dp__tlm__core_8h.md#function-dp_tlm_block_bound)): **no probe emits more than once per input sample**, so a block of `N` inputs emits at most `probe_count * N` records. Size the ring to that and drain it to empty at every block boundary, and the ring _cannot_ overflow — the producer never gets more than one block ahead of the consumer, by construction.


That is a proof, not a heuristic. There is no polling interval to tune, no scheduling assumption, no safety factor, and no background drain racing the producer: the drain runs on the caller's thread, at the boundary, where the producer is by definition quiescent. It is also the _fastest_ arrangement available — one `memcpy` per block and nothing added to the emit path.


## Two problems, kept separate



Losslessness is bought by the sizing above. **Flat memory** on a long run is a different problem, bought by handing drained blocks to a file: the capture ping-pongs two staging buffers so a writer thread can be draining one while the producer fills the other. If the writer falls behind, the _boundary_ blocks. That is backpressure — the capture waits, the data survives. Nothing is ever dropped to keep up.



## Where the boundary comes from



dp\_tlm\_set\_now() delegates here whenever a capture is open. Callers already put it at the top of the block loop, before stepping, so an existing `set_now / steps / read` loop becomes lossless by opening a capture and changing nothing else. Call [**dp\_tlm\_capture\_block()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_block) directly if you stamp the sample index some other way.



## On disk



The 16-byte [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) layout _is_ the file — no framing, no header, no version. `np.fromfile(path, dtype=REC_DTYPE)` reads it, and so does a plain `fread`. A `<path>-meta` JSON sidecar carries what the records cannot: the probe table, the counters, and the sample clock.



## The time base is borrowed, never re-declared



A record carries `n` and no time; time is `t0 + n / fs`. That pair, and that computation, are already [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md) — so a capture takes the clock **by reference** rather than growing a private `fs`/`t0` of its own. Two copies of a time base drift, and the one in the file is the copy nobody can correct afterwards. Passing NULL states "no time base", and the sidecar then omits the keys rather than fabricating a plausible rate.



```C++
rx_set_telemetry (rx, tlm, "rx", 1);   // probes first: they
                                       // set the bound
dp_sample_clock_t clk;
dp_sample_clock_init (&clk, 1e6, 1);
dp_tlm_capture_t *cap =
  dp_tlm_capture_open (tlm, 256, "rx.tlm", &clk);
for (size_t i = 0; i < n; i += 256)
  {
    dp_tlm_set_now (tlm, i);           // drains the block
                                       // just finished
    rx_steps (rx, x + i, 256, y);
  }
int rc = dp_tlm_capture_close (cap);   // DP_OK == nothing lost
dp_tlm_capture_destroy (cap);
```
 



    
## Public Types Documentation




### typedef dp\_tlm\_capture\_state\_t 

_jm's spelling of_ [_**dp\_tlm\_capture\_t**_](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) _._
```C++
typedef dp_tlm_capture_t dp_tlm_capture_state_t;
```



Same bridge [**dp\_tlm\_core.h**](dp__tlm__core_8h.md) carries, for the same reason: jm derives an object's state struct as `<component>_state_t` with no override (just-makeit#797), and this type predates jm. It is in the signature of every function above and in dp\_tlm\_t's own `capture` member, so renaming it is not on the table. An alias costs one line and nothing at runtime, and goes away when jm#797 lands `state_type`. 


        

<hr>
## Public Functions Documentation




### function dp\_tlm\_capture\_block 

_Block boundary: drains the ring to empty._ 
```C++
int dp_tlm_capture_block (
    dp_tlm_capture_t * c
) 
```



Grows the ring first if probes appeared since the last boundary, which is safe precisely here — the ring is about to be emptied and the producer is between blocks. Then copies everything available into the active staging buffer, handing it to the sink and swapping when it can no longer hold another block.


**May block** in file mode, if the writer still holds the other buffer. That wait is the backpressure that keeps the capture lossless; it happens at the boundary, never inside the DSP loop.


Usually reached through dp\_tlm\_set\_now() rather than called directly.




**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL / a closed capture, [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory) if a buffer could not grow, or [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) if the writer failed.



```C++
>>> from doppler.telemetry import Telemetry, MemoryCapture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
>>> tlm.emit(pid, 1.5)

An explicit boundary; set_now() reaches this for you:

>>> cap.block()
>>> cap.count
1
```
 


        

<hr>



### function dp\_tlm\_capture\_close 

_Final boundary, then flush, join, and write the sidecar._ 
```C++
int dp_tlm_capture_close (
    dp_tlm_capture_t * c
) 
```



Sweeps the tail the last block left behind, drains the staging buffers, joins the writer thread, closes the file and writes `<path>-meta`. Idempotent: a second call is a no-op returning the first call's verdict.




**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok) when the capture is provably complete. \*\*[**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) when records were dropped\*\* — the invariant makes that impossible, so a non-zero count means the contract was broken (a step longer than `block_samples`, or no boundary at all) and the capture has a hole in it. A capture with a hole is not a smaller capture, it is a wrong one, so this fails loudly rather than returning quietly. [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) on a write failure.



```C++
>>> from doppler.telemetry import Telemetry, MemoryCapture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
>>> for blk in range(4):
...     tlm.set_now(blk * 256)
...     tlm.emit(pid, float(blk))
>>> cap.close()          # silent: the block contract was honoured
>>> cap.close()          # idempotent, same verdict

Breaking the contract -- here, never reaching a boundary at all -- is the
one way to lose a record, and it is reported rather than absorbed:

>>> tlm2 = Telemetry(1 << 12)
>>> p2 = tlm2.probe("x")
>>> bad = MemoryCapture(tlm2, 8, SampleClock(1e6))
>>> for i in range(20000):
...     tlm2.emit(p2, float(i))
>>> bad.close()  # doctest: +ELLIPSIS
Traceback (most recent call last):
ValueError: the capture has a hole: ...
```
 


        

<hr>



### function dp\_tlm\_capture\_context 

_The context this capture drains, borrowed._ 
```C++
dp_tlm_t * dp_tlm_capture_context (
    const dp_tlm_capture_t * c
) 
```



A capture's records carry probe _ids_; turning those back into names needs the registry, which lives on the context. Exposing the borrow is what lets a consumer name what it captured without being handed the context separately and having to keep the two associated by hand.


Borrowed, not owned: the context outlives the capture by construction (it is what the capture was opened on), and destroying the capture does not touch it. 


        

<hr>



### function dp\_tlm\_capture\_count 

_Records captured so far, across memory and file alike._ 
```C++
size_t dp_tlm_capture_count (
    const dp_tlm_capture_t * c
) 
```




<hr>



### function dp\_tlm\_capture\_destroy 

_Closes if still open, then frees. NULL-safe._ 
```C++
int dp_tlm_capture_destroy (
    dp_tlm_capture_t * c
) 
```



Returns the close verdict rather than discarding it. That is the whole point: a `with` block's exit and a garbage collection both land here, and a capture with a hole reporting nothing on the way out would defeat [**dp\_tlm\_capture\_close()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_close)'s reason for existing. The state is released whatever the verdict says — this never leaks on the error path.




**Returns:**

[**dp\_tlm\_capture\_close()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_close)'s result, or [**DP\_OK**](clib__common_8h.md#define-dp_ok) for a NULL or already-closed capture (nothing was left to go wrong). 





        

<hr>



### function dp\_tlm\_capture\_dropped 

_Records the ring dropped during this capture._ 
```C++
uint64_t dp_tlm_capture_dropped (
    const dp_tlm_capture_t * c
) 
```



Latched against the context's monotonic counter at open, so it reports this capture rather than the context's lifetime. Non-zero means a hole; see [**dp\_tlm\_capture\_close()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_close). 


        

<hr>



### function dp\_tlm\_capture\_open 

_Opens a lossless capture over_ `t` _and arms the boundary drain._
```C++
dp_tlm_capture_t * dp_tlm_capture_open (
    dp_tlm_t * t,
    size_t block_samples,
    const char * path,
    const dp_sample_clock_t * clock
) 
```



Sizes the ring to `dp_tlm_block_bound (t, block_samples)`, so **attach every probe first** — an object's `*_set_telemetry` is what registers them. A probe registered later is not a disaster: [**dp\_tlm\_capture\_block()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_block) re-checks the bound and grows before the larger block can cost a record.


At most one capture per context; opening a second fails.




**Parameters:**


* `t` Context to capture. Must outlive the capture. 
* `block_samples` The LARGEST number of input samples processed between two boundaries. Not a buffer size to tune — the step of the caller's own block loop. Over-stating it costs only memory; under-stating it is the one way to lose a record, and [**dp\_tlm\_capture\_close()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_close) reports it. 
* `path` Output file. NULL accumulates in memory instead, for [**dp\_tlm\_capture\_records()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_records). Truncated if it exists. 
* `clock` The pipeline's sample clock, borrowed for the sidecar's time base. Read at close(), so later `track()` corrections to the epoch are picked up. Must outlive the capture. NULL = no time base stated, and the sidecar says so by omission. 



**Returns:**

New capture, or NULL on a NULL/zero `t` / `block_samples`, a context that already has a capture, an unopenable `path`, or allocation failure.



```C++
>>> import os, tempfile
>>> from doppler.telemetry import Telemetry, Capture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")   # probes FIRST: they set the bound
>>> path = os.path.join(tempfile.mkdtemp(), "rx.tlm")
>>> with Capture(tlm, 256, path, SampleClock(1e6)) as cap:
...     for blk in range(4):
...         tlm.set_now(blk * 256)   # drains the block just finished
...         tlm.emit(pid, float(blk))
>>> os.path.exists(path + "-meta")   # the sidecar, written at close
True

The 16-byte record layout IS the file, so nothing doppler-specific is
needed to read it back:

>>> import numpy as np
>>> dt = np.dtype({"names": ["n", "value", "probe", "flags"],
...                "formats": ["<u8", "<f4", "<u2", "<u2"],
...                "offsets": [0, 8, 12, 14], "itemsize": 16})
>>> [float(v) for v in np.fromfile(path, dtype=dt)["value"]]
[0.0, 1.0, 2.0, 3.0]
```
 


        

<hr>



### function dp\_tlm\_capture\_open\_memory 

_Opens a capture that accumulates in memory instead of a file._ 
```C++
dp_tlm_capture_t * dp_tlm_capture_open_memory (
    dp_tlm_t * t,
    size_t block_samples,
    const dp_sample_clock_t * clock
) 
```



Identical to [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open) with a NULL `path`, and exists as its own entry point because "no file" is a _different constructor_, not a degenerate path string: the two flavours differ in what they can answer afterwards. A memory capture can hand back its records ([**dp\_tlm\_capture\_read()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_read)); a file capture cannot, because the file IS the capture. Splitting the constructors lets each Python flavour carry only the methods that mean something for it, rather than one class with an accessor that returns nothing half the time.




**Parameters:**


* `t` Context to capture. Must outlive the capture. 
* `block_samples` The largest number of input samples between two boundaries — see [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open). 
* `clock` Borrowed sample clock, or NULL for no time base. 



**Returns:**

New capture, or NULL on the same conditions as [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open) minus the file ones.



```C++
>>> from doppler.telemetry import Telemetry, MemoryCapture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")   # probes FIRST: they set the bound
>>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
>>> for blk in range(4):
...     tlm.set_now(blk * 256)       # drains the block just finished
...     tlm.emit(pid, float(blk))
>>> cap.close()                      # raises if anything was lost
>>> [float(v) for v in cap.records()["value"]]
[0.0, 1.0, 2.0, 3.0]
>>> cap.dropped
0
```
 


        

<hr>



### function dp\_tlm\_capture\_read 

_Copies accumulated records out. Memory mode only._ 
```C++
size_t dp_tlm_capture_read (
    const dp_tlm_capture_t * c,
    size_t n,
    dp_tlm_rec_t * out,
    size_t max_out
) 
```



The copying twin of [**dp\_tlm\_capture\_records()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_records): same records, same order, but into caller memory rather than a borrowed pointer. Both exist because they serve opposite callers — a C consumer wants the zero-copy view, and a binding must not hand out a pointer the capture can free underneath it.


Deliberately the same shape as [**dp\_tlm\_read()**](dp__tlm__core_8h.md#function-dp_tlm_read), so the two drains bind identically and neither needs a second convention invented for it.




**Parameters:**


* `c` Capture. A file-mode capture holds no records and yields 0. 
* `n` Records wanted; 0 means "everything accumulated". 
* `out` Destination. 
* `max_out` Capacity of `out`, in records. 



**Returns:**

Number of records copied out.



```C++
>>> import numpy as np
>>> from doppler.telemetry import Telemetry, MemoryCapture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
>>> for blk in range(4):
...     tlm.set_now(blk * 256)
...     tlm.emit(pid, float(blk))
>>> cap.close()
>>> [float(v) for v in cap.records()["value"]]
[0.0, 1.0, 2.0, 3.0]
>>> cap.records(2).shape             # 0 (the default) means "all"
(2,)
```
 


        

<hr>



### function dp\_tlm\_capture\_read\_max\_out 

_Upper bound on what_ [_**dp\_tlm\_capture\_read()**_](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_read) _can return right now._
```C++
size_t dp_tlm_capture_read_max_out (
    const dp_tlm_capture_t * c
) 
```



The accumulated count: a caller sizing a destination cannot know its own request will be smaller, and the generated binding allocates this much, reads, then resizes to what actually came back. 


        

<hr>



### function dp\_tlm\_capture\_records 

_The accumulated records, contiguous and in emission order._ 
```C++
const dp_tlm_rec_t * dp_tlm_capture_records (
    const dp_tlm_capture_t * c
) 
```



Memory mode only (`path` was NULL) — in file mode the file _is_ the capture and this returns NULL. Owned by the capture and invalidated by [**dp\_tlm\_capture\_destroy()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_destroy); NULL when nothing was captured, so use [**dp\_tlm\_capture\_count()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_count) to tell empty from absent.


The Python face binds the COPYING twin, [**dp\_tlm\_capture\_read()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_read), because a borrowed pointer the capture can free is not something a binding may hand out. This example is duplicated there deliberately: jm derives a method's docstring from the `<component>_<method>` symbol while `fn` chooses the one it calls, so the two must carry the same text or the .pyi and the runtime `__doc__` disagree (checked by scripts/check\_doc\_face\_parity.py).



```C++
>>> import numpy as np
>>> from doppler.telemetry import Telemetry, MemoryCapture
>>> from doppler.wfm import SampleClock
>>> tlm = Telemetry(1 << 12)
>>> pid = tlm.probe("agc.gain_db")
>>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
>>> for blk in range(4):
...     tlm.set_now(blk * 256)
...     tlm.emit(pid, float(blk))
>>> cap.close()
>>> [float(v) for v in cap.records()["value"]]
[0.0, 1.0, 2.0, 3.0]
>>> cap.records(2).shape             # 0 (the default) means "all"
(2,)
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_tlm_capture/dp_tlm_capture_core.h`

