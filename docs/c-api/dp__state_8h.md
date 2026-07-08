

# File dp\_state.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_state.h**](dp__state_8h.md)

[Go to the source code of this file](dp__state_8h_source.md)



* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_reader\_t**](structdp__reader__t.md) <br> |
| struct | [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) <br>_Common 16-byte envelope at the head of every state blob._  |
| struct | [**dp\_writer\_t**](structdp__writer__t.md) <br> |
























## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_r\_bytes**](#function-dp_r_bytes) ([**dp\_reader\_t**](structdp__reader__t.md) \* r, void \* dst, size\_t n) <br> |
|  void | [**dp\_r\_cf32**](#function-dp_r_cf32) ([**dp\_reader\_t**](structdp__reader__t.md) \* r, float \_Complex \* p, size\_t n) <br> |
|  void | [**dp\_r\_f32**](#function-dp_r_f32) ([**dp\_reader\_t**](structdp__reader__t.md) \* r, float \* p, size\_t n) <br> |
|  double | [**dp\_r\_f64**](#function-dp_r_f64) ([**dp\_reader\_t**](structdp__reader__t.md) \* r) <br> |
|  const void \* | [**dp\_r\_reserve**](#function-dp_r_reserve) ([**dp\_reader\_t**](structdp__reader__t.md) \* r, size\_t n) <br> |
|  uint32\_t | [**dp\_r\_u32**](#function-dp_r_u32) ([**dp\_reader\_t**](structdp__reader__t.md) \* r) <br> |
|  uint64\_t | [**dp\_r\_u64**](#function-dp_r_u64) ([**dp\_reader\_t**](structdp__reader__t.md) \* r) <br> |
|  [**dp\_reader\_t**](structdp__reader__t.md) | [**dp\_reader\_init**](#function-dp_reader_init) (const void \* blob, size\_t cap) <br> |
|  int | [**dp\_state\_validate**](#function-dp_state_validate) (const void \* blob, size\_t expect\_bytes, uint32\_t magic, uint16\_t version) <br>_Validate a blob's envelope before trusting its payload._  |
|  void | [**dp\_w\_bytes**](#function-dp_w_bytes) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, const void \* src, size\_t n) <br> |
|  void | [**dp\_w\_cf32**](#function-dp_w_cf32) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, const float \_Complex \* p, size\_t n) <br> |
|  void | [**dp\_w\_f32**](#function-dp_w_f32) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, const float \* p, size\_t n) <br> |
|  void | [**dp\_w\_f64**](#function-dp_w_f64) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, double v) <br> |
|  void | [**dp\_w\_hdr**](#function-dp_w_hdr) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, uint32\_t magic, uint16\_t version, size\_t total) <br> |
|  void \* | [**dp\_w\_reserve**](#function-dp_w_reserve) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, size\_t n) <br> |
|  void | [**dp\_w\_u32**](#function-dp_w_u32) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, uint32\_t v) <br> |
|  void | [**dp\_w\_u64**](#function-dp_w_u64) ([**dp\_writer\_t**](structdp__writer__t.md) \* w, uint64\_t v) <br> |
|  [**dp\_writer\_t**](structdp__writer__t.md) | [**dp\_writer\_init**](#function-dp_writer_init) (void \* blob, size\_t cap) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_DEFINE\_POD\_STATE**](dp__state_8h.md#define-dp_define_pod_state) (pfx, STATE\_T, MAGIC, VERSION) `/* multi line expression */`<br>_Define the whole-struct state triplet for a pointer-free POD object._  |
| define  | [**DP\_DEFINE\_RUN**](dp__state_8h.md#define-dp_define_run) (pfx, STATE\_T, IN\_T, OUT\_T) `/* multi line expression */`<br>_Define the standard_ `<pfx>_run` _pure-transducer wrapper._ |
| define  | [**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) (a, b, c, d) `/* multi line expression */`<br> |
| define  | [**DP\_GET\_OPEN**](dp__state_8h.md#define-dp_get_open) (MAGIC, VERSION, BYTES) `/* multi line expression */`<br> |
| define  | [**DP\_R\_CHILD**](dp__state_8h.md#define-dp_r_child) (r, pfx, child\_ptr) `/* multi line expression */`<br> |
| define  | [**DP\_SET\_OPEN**](dp__state_8h.md#define-dp_set_open) (MAGIC, VERSION, BYTES) `/* multi line expression */`<br> |
| define  | [**DP\_STATE\_ENDIAN**](dp__state_8h.md#define-dp_state_endian)  `1u /\* little-endian (x86-64, arm64 — doppler targets) \*/`<br> |
| define  | [**DP\_W\_CHILD**](dp__state_8h.md#define-dp_w_child) (w, pfx, child\_ptr) `/* multi line expression */`<br> |

## Public Static Functions Documentation




### function dp\_r\_bytes 

```C++
static inline void dp_r_bytes (
    dp_reader_t * r,
    void * dst,
    size_t n
) 
```




<hr>



### function dp\_r\_cf32 

```C++
static inline void dp_r_cf32 (
    dp_reader_t * r,
    float _Complex * p,
    size_t n
) 
```




<hr>



### function dp\_r\_f32 

```C++
static inline void dp_r_f32 (
    dp_reader_t * r,
    float * p,
    size_t n
) 
```




<hr>



### function dp\_r\_f64 

```C++
static inline double dp_r_f64 (
    dp_reader_t * r
) 
```




<hr>



### function dp\_r\_reserve 

```C++
static inline const void * dp_r_reserve (
    dp_reader_t * r,
    size_t n
) 
```



Borrow `n` bytes in place (for a child's set\_state to read); NULL on overrun. 


        

<hr>



### function dp\_r\_u32 

```C++
static inline uint32_t dp_r_u32 (
    dp_reader_t * r
) 
```




<hr>



### function dp\_r\_u64 

```C++
static inline uint64_t dp_r_u64 (
    dp_reader_t * r
) 
```




<hr>



### function dp\_reader\_init 

```C++
static inline dp_reader_t dp_reader_init (
    const void * blob,
    size_t cap
) 
```




<hr>



### function dp\_state\_validate 

_Validate a blob's envelope before trusting its payload._ 
```C++
static inline int dp_state_validate (
    const void * blob,
    size_t expect_bytes,
    uint32_t magic,
    uint16_t version
) 
```



Every obj\_set\_state opens with this. `expect_bytes` is the receiving object's obj\_state\_bytes() — a blob from a different object (magic), format (version), endianness, or config/size (bytes) is rejected rather than reinterpreted.




**Returns:**

DP\_OK, or DP\_ERR\_INVALID on any mismatch. 





        

<hr>



### function dp\_w\_bytes 

```C++
static inline void dp_w_bytes (
    dp_writer_t * w,
    const void * src,
    size_t n
) 
```




<hr>



### function dp\_w\_cf32 

```C++
static inline void dp_w_cf32 (
    dp_writer_t * w,
    const float _Complex * p,
    size_t n
) 
```




<hr>



### function dp\_w\_f32 

```C++
static inline void dp_w_f32 (
    dp_writer_t * w,
    const float * p,
    size_t n
) 
```




<hr>



### function dp\_w\_f64 

```C++
static inline void dp_w_f64 (
    dp_writer_t * w,
    double v
) 
```




<hr>



### function dp\_w\_hdr 

```C++
static inline void dp_w_hdr (
    dp_writer_t * w,
    uint32_t magic,
    uint16_t version,
    size_t total
) 
```



Stamp the standard envelope. `total` must equal obj\_state\_bytes(). 


        

<hr>



### function dp\_w\_reserve 

```C++
static inline void * dp_w_reserve (
    dp_writer_t * w,
    size_t n
) 
```



Reserve `n` bytes and return the region (for a child's get\_state to fill); NULL on overrun. 


        

<hr>



### function dp\_w\_u32 

```C++
static inline void dp_w_u32 (
    dp_writer_t * w,
    uint32_t v
) 
```




<hr>



### function dp\_w\_u64 

```C++
static inline void dp_w_u64 (
    dp_writer_t * w,
    uint64_t v
) 
```




<hr>



### function dp\_writer\_init 

```C++
static inline dp_writer_t dp_writer_init (
    void * blob,
    size_t cap
) 
```




<hr>
## Macro Definition Documentation





### define DP\_DEFINE\_POD\_STATE 

_Define the whole-struct state triplet for a pointer-free POD object._ 
```C++
#define DP_DEFINE_POD_STATE (
    pfx,
    STATE_T,
    MAGIC,
    VERSION
) `/* multi line expression */`
```



Generates `<pfx>_state_bytes/get_state/set_state` that snapshot the entire `STATE_T` after the envelope. Correct only when the struct holds no pointers (the snapshot would capture a stale address): the running state _and_ the derived config are serialized, and config restores identically into an identically-built instance. For a struct with pointers or a composition, hand-write the triplet (pack running fields / delegate to children) instead. 


        

<hr>



### define DP\_DEFINE\_RUN 

_Define the standard_ `<pfx>_run` _pure-transducer wrapper._
```C++
#define DP_DEFINE_RUN (
    pfx,
    STATE_T,
    IN_T,
    OUT_T
) `/* multi line expression */`
```



Generates `size_t <pfx>_run(state, state_in, state_out, in, n_in, out,
max_out)`: restore `state_in` (or keep current), call `<pfx>_execute`, then export `state_out` — the `(state_in, input) -> (state_out, output)` face for elastic fan-out. For objects whose middle step is a single `_execute`; an object with a different step shape (e.g. acq's frame/push) defines its own. 


        

<hr>



### define DP\_FOURCC 

```C++
#define DP_FOURCC (
    a,
    b,
    c,
    d
) `/* multi line expression */`
```



[**dp\_state.h**](dp__state_8h.md) — the standard state **bytes interface** for doppler.


Serialization is module-specific (only `lo` knows it holds a phase, only `fir` knows it holds a delay line); the _bytes interface_ around it is not. This header owns that universal layer, once, for every serializable object:



* a self-describing envelope (`dp_state_hdr_t`: magic / version / endian / size) that prefixes every state blob,
* writer / reader cursors that turn hand-packing into a few bounds-checked calls,
* `dp_state_validate()` — the one check every `set_state` opens with, so a wrong-object / wrong-config / foreign-endian blob is rejected, never silently reinterpreted, and
* `DP_DEFINE_RUN()` — the identical `obj_run` pure-transducer wrapper.




The per-object ABI (the contract jm's `serializable` binding and the Rust FFI call) stays:


size\_t obj\_state\_bytes(const obj\_state\_t \*); void obj\_get\_state (const obj\_state\_t \*, void \*blob); int obj\_set\_state (obj\_state\_t \*, const void \*blob); // DP\_OK / -err


Blob layout, every object: [ [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) ] [ module payload ] Compositions embed children as self-contained sub-blobs (each carries its own header): [ hdr ] [ extra? ] [ child blob ]... with state\_bytes = sizeof(hdr) + sizeof(extra) + Σ child\_state\_bytes.


Blobs are native-endian POD for same-machine / same-arch resume (thread, process, pod). The endian byte is stamped and rejected on mismatch; there is deliberately no cross-endian byte-swap. 


        

<hr>



### define DP\_GET\_OPEN 

```C++
#define DP_GET_OPEN (
    MAGIC,
    VERSION,
    BYTES
) `/* multi line expression */`
```




<hr>



### define DP\_R\_CHILD 

```C++
#define DP_R_CHILD (
    r,
    pfx,
    child_ptr
) `/* multi line expression */`
```




<hr>



### define DP\_SET\_OPEN 

```C++
#define DP_SET_OPEN (
    MAGIC,
    VERSION,
    BYTES
) `/* multi line expression */`
```




<hr>



### define DP\_STATE\_ENDIAN 

```C++
#define DP_STATE_ENDIAN `1u /* little-endian (x86-64, arm64 — doppler targets) */`
```




<hr>



### define DP\_W\_CHILD 

```C++
#define DP_W_CHILD (
    w,
    pfx,
    child_ptr
) `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_state.h`

