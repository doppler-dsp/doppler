

# File wfm\_plan.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_plan.h**](wfm__plan_8h.md)

[Go to the source code of this file](wfm__plan_8h_source.md)

_"Prepare once, materialize many" stimulus engine._ [More...](#detailed-description)

* `#include "doppler/dp_complex.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_plan | [**wfm\_plan\_t**](#typedef-wfm_plan_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**dp\_wfm\_plan\_anchor\_seed**](#function-dp_wfm_plan_anchor_seed) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_The noise seed that reproduces a full compose._  |
|  size\_t | [**dp\_wfm\_plan\_at**](#function-dp_wfm_plan_at) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, double snr, uint64\_t seed, float \_Complex \* out) <br>_Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse)._  |
|  int | [**dp\_wfm\_plan\_check\_snr**](#function-dp_wfm_plan_check_snr) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Whether an_ `snr` _can be applied to this Plan._ |
|  void | [**dp\_wfm\_plan\_destroy**](#function-dp_wfm_plan_destroy) ([**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Destroy a Plan and free its caches. NULL is a no-op._  |
|  int | [**dp\_wfm\_plan\_dump**](#function-dp_wfm_plan_dump) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, const char \* path) <br>_Save a Plan to a file (_ [_**dp\_wfm\_plan\_save()**_](wfm__plan_8h.md#function-dp_wfm_plan_save) _bytes at_`path` _)._ |
|  size\_t | [**dp\_wfm\_plan\_len**](#function-dp_wfm_plan_len) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Worst-case materialized length in samples (every ranged gap at its_ `hi` _bound) — the jm binding's out\_len\_fn / allocation capacity._ |
|  [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* | [**dp\_wfm\_plan\_load**](#function-dp_wfm_plan_load) (const char \* path) <br>_Load a Plan from a file written by_ [_**dp\_wfm\_plan\_dump()**_](wfm__plan_8h.md#function-dp_wfm_plan_dump) _._ |
|  size\_t | [**dp\_wfm\_plan\_n\_sources**](#function-dp_wfm_plan_n_sources) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Number of cached signal sources across every segment (excludes noise floors); the length of the_ `gains` _/_`phases` _/_`enable` _arrays._ |
|  [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* | [**dp\_wfm\_plan\_prepare**](#function-dp_wfm_plan_prepare) (const char \* spec\_json) <br>_Prepare a Plan from a composer spec JSON (Composer.to\_json())._  |
|  size\_t | [**dp\_wfm\_plan\_render**](#function-dp_wfm_plan_render) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, const char \* overrides\_json, float \_Complex \* out) <br>_General render: apply a JSON override spec, return a cf32 array._  |
|  [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* | [**dp\_wfm\_plan\_restore**](#function-dp_wfm_plan_restore) (const void \* blob, size\_t n) <br>_Reconstruct a Plan from a blob produced by_ [_**dp\_wfm\_plan\_save()**_](wfm__plan_8h.md#function-dp_wfm_plan_save) _._ |
|  size\_t | [**dp\_wfm\_plan\_save**](#function-dp_wfm_plan_save) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, void \* blob) <br>_Serialize a Plan into_ `blob` _(dp\_wfm\_plan\_save\_bytes(p) bytes)._ |
|  size\_t | [**dp\_wfm\_plan\_save\_bytes**](#function-dp_wfm_plan_save_bytes) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Serialized size of a Plan blob (envelope + spec + cached buffers)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_WFM\_PLAN\_WHY\_NO\_NOISE**](wfm__plan_8h.md#define-dp_wfm_plan_why_no_noise)  `/* multi line expression */`<br>_Why a Plan refuses an_ `snr` _: its scene carries no noise._ |

## Detailed Description


A composed scene is a sequence of segments, each a linear form  ([**dp\_wfm\_resolve\_noise()**](wfm__compose_8h.md#function-dp_wfm_resolve_noise) cleans every signal source and appends one WFM\_SYNTH\_NOISE source at the shared floor, for a multi-source segment; a lone source carrying its own real SNR keeps it — its AWGN is baked into its own synth, "bundled"). The expensive DSP — LFSR spread, RRC convolution, transcendental LO — lives entirely in the signal terms, which are INVARIANT across a parameter sweep and across a segment's `repeats` instances (only the AWGN, and any ranged gap length, vary per instance). So a Plan renders each segment's signal ON-time ONCE (via the composer's own dp\_wfm\_compose\_build\_synth, so a cached render is byte-identical to a full compose), caches it, and then re-materializes any variation as a cheap re-weighted sum per segment/instance plus a regenerated noise synth spanning that instance's delay+on+off:  \| noise(off) ]


v1 axes (all bit-exact vs a full compose): per-source gain/level, phase, enable/disable, global SNR/noise-floor and Monte-Carlo noise-seed — applied uniformly across every segment/instance that carries noise. Clock Doppler is served too, by a different mechanism: the cache holds the signal BEFORE the channel, and the channel is run at render time over it (see [**dp\_wfm\_plan\_prepare()**](wfm__plan_8h.md#function-dp_wfm_plan_prepare)). Multipath delay is a staged follow-up.


Scope: any number of finite segments (no continuous/repeat scene — that has no fixed capacity); each segment may declare `repeats` (bounded instancing, AWGN fresh per instance, signal fixed) and ranged `off_samples`/`delay_samples` (redrawn per instance at materialize time, via the same deterministic hash the streaming composer uses). Still out of scope: a ranged on-time (`num_samples`) — it would invalidate the fixed-length signal cache — and any ranged per-source field (freq/snr/level/f\_end) — redrawing a source's frequency or SNR would invalidate its cached render, defeating the "expensive DSP once" guarantee this exists to provide.


`dp_wfm_plan_len()` is a WORST-CASE capacity (every ranged gap at its `hi` bound): `render()`/`at()` write up to that many samples but return the ACTUAL length of that specific draw — trailing samples beyond the return value are zero padding. A `seed` override drives both the noise realization AND (for a scene with a ranged gap/delay) the gap/delay redraw; the "no-override reproduces wfm\_compose to the bit" guarantee is scoped to `render()` with no `"seed"` key (or `NULL`/`"{}"` overrides) — each segment then draws from its own baked default seed (its first source's `seed` field), same as a plain compose. `at()`'s `seed` is always an explicit override (parallel to its always-explicit `snr`), so it does not carry that baseline guarantee for a ranged-gap scene.


`gains`/`phases`/`enable` override arrays are flat and segment-major (segment 0's sources in scene order, then segment 1's, ...) — length `dp_wfm_plan_n_sources()`.


The Plan is a re-creatable derived cache, not evolving state. The cheapest, most portable persistence is therefore the spec JSON (Composer.to\_json()) plus a rebuild — KB, and always tracks the current DSP. [**dp\_wfm\_plan\_save()**](wfm__plan_8h.md#function-dp_wfm_plan_save) / [**dp\_wfm\_plan\_restore()**](wfm__plan_8h.md#function-dp_wfm_plan_restore) are the OPTIONAL fast-path alternative: they serialize the cached sample buffers alongside the embedded spec so a restore SKIPS the expensive DSP (build\_synth) that dominates prepare(). This is a versioned performance cache, NOT the [**dp\_state.h**](dp__state_8h.md) elastic-state contract — it is gated by a build-time DSP fingerprint (wfm\_plan\_dsp\_hash.h), and on any mismatch (different DSP build, or a spec/buffer inconsistency) restore transparently REBUILDS from the embedded spec rather than reinterpreting stale bytes. So a restore is never silently wrong: worst case it is as slow as prepare(); best case (matching build) it is a memcpy. Prefer save/restore only when the DSP cost is high and the spec-rebuild is undesirable (e.g. fanning one prepared Plan across many workers); otherwise persist the spec and rebuild. 


    
## Public Types Documentation




### typedef wfm\_plan\_t 

```C++
typedef struct wfm_plan wfm_plan_t;
```



Opaque prepared-plan state. 


        

<hr>
## Public Functions Documentation




### function dp\_wfm\_plan\_anchor\_seed 

_The noise seed that reproduces a full compose._ 
```C++
uint64_t dp_wfm_plan_anchor_seed (
    const wfm_plan_t * p
) 
```



The first noisy segment's default seed (its first source's `seed` field). Passing this as `dp_wfm_plan_at`'s seed (with the scene's base SNR) yields the byte-identical output of `wfm_compose` for a single-segment scene; for a multi-segment scene each segment still draws from its own default seed unless overridden. Varying the seed draws independent Monte-Carlo noise (and, for a ranged-gap scene, timing) realizations.




**Parameters:**


* `p` The Plan, or NULL. 



**Returns:**

The first noisy segment's noise seed; 0 for a NULL Plan or a scene with no noisy segment. 





        

<hr>



### function dp\_wfm\_plan\_at 

_Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse)._ 
```C++
size_t dp_wfm_plan_at (
    const wfm_plan_t * p,
    double snr,
    uint64_t seed,
    float _Complex * out
) 
```



`out = Σ gain_k·cache_k + gain(snr)·noise(seed)` per segment/instance; writes up to `dp_wfm_plan_len(p)` samples. Equivalent to `render` with only `{"snr":snr,"seed":seed}` — `seed` is always an explicit override here, and so is `snr`: on a Plan whose scene carries no noise it is refused (see [**dp\_wfm\_plan\_check\_snr()**](wfm__plan_8h.md#function-dp_wfm_plan_check_snr)).




**Parameters:**


* `p` The Plan. 
* `snr` SNR override, dB, read per segment in the snr\_mode of the source that sets its SNR: a bundled segment's lone source, else the source sharing the noise source's seed. A segment whose noise floor was set explicitly keeps that floor. 
* `seed` Noise (and ranged-gap) seed override. Only its low 32 bits are used: the seeds it replaces are 32-bit. 
* `out` Receives the samples; must hold dp\_wfm\_plan\_len(p) of them. 



**Returns:**

Samples actually written for this draw (&lt;= dp\_wfm\_plan\_len(p)), or 0 when refused, with `out` untouched. 





        

<hr>



### function dp\_wfm\_plan\_check\_snr 

_Whether an_ `snr` _can be applied to this Plan._
```C++
int dp_wfm_plan_check_snr (
    const wfm_plan_t * p
) 
```



O(1): the answer is fixed at prepare time. A caller about to sweep SNR checks it once; `dp_wfm_plan_at()` and `dp_wfm_plan_render()` apply the same rule themselves.



```C++
>>> from doppler.wfm import Composer, prepare
>>> clean = prepare(Composer(type="tone", num_samples=64))
>>> clean.at(6.0)
Traceback (most recent call last):
    ...
ValueError: this scene carries no noise, so the Plan has no noise floor for snr to move: give a source a finite snr (below 100 dB) and an snr_mode (rc=-4)
>>> len(prepare(Composer(type="tone", num_samples=64, snr=10.0)).at(6.0))
64
```





**Parameters:**


* `p` The Plan; NULL is refused like a noiseless scene. 



**Returns:**

0 (DP\_OK) when some segment carries noise; DP\_ERR\_INVALID (-4) when none does, for the reason DP\_WFM\_PLAN\_WHY\_NO\_NOISE. 





        

<hr>



### function dp\_wfm\_plan\_destroy 

_Destroy a Plan and free its caches. NULL is a no-op._ 
```C++
void dp_wfm_plan_destroy (
    wfm_plan_t * p
) 
```





**Parameters:**


* `p` The Plan, or NULL. 




        

<hr>



### function dp\_wfm\_plan\_dump 

_Save a Plan to a file (_ [_**dp\_wfm\_plan\_save()**_](wfm__plan_8h.md#function-dp_wfm_plan_save) _bytes at_`path` _)._
```C++
int dp_wfm_plan_dump (
    const wfm_plan_t * p,
    const char * path
) 
```





**Parameters:**


* `p` The Plan. 
* `path` File to create or truncate. 



**Returns:**

0 on success; -1 on a NULL argument, an allocation failure, or an open/write/close error. 





        

<hr>



### function dp\_wfm\_plan\_len 

_Worst-case materialized length in samples (every ranged gap at its_ `hi` _bound) — the jm binding's out\_len\_fn / allocation capacity._
```C++
size_t dp_wfm_plan_len (
    const wfm_plan_t * p
) 
```





**Parameters:**


* `p` The Plan, or NULL. 



**Returns:**

Capacity in samples that `out` must hold for render()/at(); 0 for a NULL Plan. 





        

<hr>



### function dp\_wfm\_plan\_load 

_Load a Plan from a file written by_ [_**dp\_wfm\_plan\_dump()**_](wfm__plan_8h.md#function-dp_wfm_plan_dump) _._
```C++
wfm_plan_t * dp_wfm_plan_load (
    const char * path
) 
```



Same fingerprint semantics as [**dp\_wfm\_plan\_restore()**](wfm__plan_8h.md#function-dp_wfm_plan_restore): a matching build loads the cached buffers, a mismatch rebuilds from the embedded spec.




**Parameters:**


* `path` File written by [**dp\_wfm\_plan\_dump()**](wfm__plan_8h.md#function-dp_wfm_plan_dump). 



**Returns:**

Heap Plan (caller [**dp\_wfm\_plan\_destroy()**](wfm__plan_8h.md#function-dp_wfm_plan_destroy)s it), or NULL on an open/read error or a malformed/foreign-endian file. 





        

<hr>



### function dp\_wfm\_plan\_n\_sources 

_Number of cached signal sources across every segment (excludes noise floors); the length of the_ `gains` _/_`phases` _/_`enable` _arrays._
```C++
size_t dp_wfm_plan_n_sources (
    const wfm_plan_t * p
) 
```





**Parameters:**


* `p` The Plan, or NULL. 



**Returns:**

Source count; 0 for a NULL Plan. 





        

<hr>



### function dp\_wfm\_plan\_prepare 

_Prepare a Plan from a composer spec JSON (Composer.to\_json())._ 
```C++
wfm_plan_t * dp_wfm_plan_prepare (
    const char * spec_json
) 
```



Parses + resolves the scene, validates scope per segment, then renders and caches each segment's clean signal ON-time at gain 1. Returns NULL on parse failure or an out-of-scope spec (continuous/repeat scene, a ranged on-time, a ranged per-source field, a non-trailing/multiple noise source within a segment, or a BACKGROUND source carrying clock Doppler).


A source with clock Doppler IS served, and the render is compose() to the bit. A Doppler channel is a resampler with state that runs through the gaps too, so what a burst renders as depends on the leading delay and on the previous instance's gap, and it resamples the AWGN of a bundled source along with the signal. A per-source on-time cache cannot hold either, so the cache holds the signal BEFORE the channel and render() runs the channel over it, through the composer's own renderer: the delay, the gaps, the noise and (for a PERSIST lifetime) the earlier segments are all in hand when it runs. A ranged `doppler` / `doppler_rate` is drawn per instance exactly as compose() draws it, and a seed override moves it. Cost: a Doppler scene's render() is no longer a pure re-weight of the cache  each Doppler source runs one resampler pass over its timeline. That pass is what render() parallelises: a segment's repeat instances render concurrently when no source PERSISTs (they share no channel), and a PERSIST segment, whose instances its channels chain, fans out across its sources instead. The result is the same bits as the serial render. A single burst from a single source has nothing to fan out, so there the pass is simply the cost.


The one refusal is a BACKGROUND source with Doppler: the background fold sums those into a single composite before the render, and a channel is per source, so it cannot be applied to a sum.




**Parameters:**


* `spec_json` A NUL-terminated composer spec JSON string. 



**Returns:**

Heap Plan (caller [**dp\_wfm\_plan\_destroy()**](wfm__plan_8h.md#function-dp_wfm_plan_destroy)s it), or NULL. 





        

<hr>



### function dp\_wfm\_plan\_render 

_General render: apply a JSON override spec, return a cf32 array._ 
```C++
size_t dp_wfm_plan_render (
    const wfm_plan_t * p,
    const char * overrides_json,
    float _Complex * out
) 
```



`overrides_json` is a small JSON object, all keys optional: `{"gains":[dB…], "phases":[rad…], "enable":[bool…], "snr":dB, "seed":u}` (`gains`/`phases`/`enable` are per-source, flat and segment-major, length = [**dp\_wfm\_plan\_n\_sources()**](wfm__plan_8h.md#function-dp_wfm_plan_n_sources)). An empty object (or NULL) renders the baseline — bit-identical to `Composer(scene).compose()`. Writes up to `dp_wfm_plan_len(p)` samples to `out`. An `"snr"` key on a Plan whose scene carries no noise is refused (see [**dp\_wfm\_plan\_check\_snr()**](wfm__plan_8h.md#function-dp_wfm_plan_check_snr)).


Keys are read leniently: overrides that do not parse as JSON render the baseline, a `gains`/`phases`/`enable` array whose length is not [**dp\_wfm\_plan\_n\_sources()**](wfm__plan_8h.md#function-dp_wfm_plan_n_sources) (or, for `gains`/`phases`, that holds a non-number) is ignored, and a non-numeric `snr`/`seed` is ignored. An `enable` entry is on when it is `true` or a non-zero number.




**Parameters:**


* `p` The Plan; NULL renders nothing and returns 0. 
* `overrides_json` NUL-terminated override object, or NULL / "" for the baseline. 
* `out` Receives the samples; must hold dp\_wfm\_plan\_len(p) of them. 



**Returns:**

Samples actually written for this draw (&lt;= dp\_wfm\_plan\_len(p)), or 0 when refused, with `out` untouched. 





        

<hr>



### function dp\_wfm\_plan\_restore 

_Reconstruct a Plan from a blob produced by_ [_**dp\_wfm\_plan\_save()**_](wfm__plan_8h.md#function-dp_wfm_plan_save) _._
```C++
wfm_plan_t * dp_wfm_plan_restore (
    const void * blob,
    size_t n
) 
```



If the blob's DSP fingerprint matches this build AND its structure matches the embedded spec, the cached buffers are loaded directly (no DSP). Otherwise the Plan is REBUILT from the embedded spec via the full DSP — same result, just paying prepare()'s cost. Returns NULL only on a malformed/foreign-endian blob or an unparseable/out-of-scope embedded spec (the same cases dp\_wfm\_plan\_prepare rejects), never on a mere fingerprint mismatch.




**Parameters:**


* `blob` Bytes from [**dp\_wfm\_plan\_save()**](wfm__plan_8h.md#function-dp_wfm_plan_save). 
* `n` Length of `blob` in bytes. 



**Returns:**

Heap Plan (caller [**dp\_wfm\_plan\_destroy()**](wfm__plan_8h.md#function-dp_wfm_plan_destroy)s it), or NULL. 





        

<hr>



### function dp\_wfm\_plan\_save 

_Serialize a Plan into_ `blob` _(dp\_wfm\_plan\_save\_bytes(p) bytes)._
```C++
size_t dp_wfm_plan_save (
    const wfm_plan_t * p,
    void * blob
) 
```



Native-endian. The blob embeds the spec JSON, so a restore is self-contained. Returns the number of bytes written (== dp\_wfm\_plan\_save\_bytes(p)) — the actual-length contract a variable-output binding needs, so `save() -> bytes` generates with no hand-written glue.




**Parameters:**


* `p` The Plan. 
* `blob` Receives the blob; must hold dp\_wfm\_plan\_save\_bytes(p) bytes. 



**Returns:**

Bytes written; 0 (nothing written) if `p` or `blob` is NULL. 





        

<hr>



### function dp\_wfm\_plan\_save\_bytes 

_Serialized size of a Plan blob (envelope + spec + cached buffers)._ 
```C++
size_t dp_wfm_plan_save_bytes (
    const wfm_plan_t * p
) 
```



The number of bytes [**dp\_wfm\_plan\_save()**](wfm__plan_8h.md#function-dp_wfm_plan_save) writes: a small envelope with the DSP fingerprint, the embedded spec JSON, and every cached signal buffer. Dominated by the buffers (Σ per-source num\_samples · 8 bytes) — multi-MB for a large scene, which is exactly why the spec-rebuild path is the default.




**Parameters:**


* `p` The Plan, or NULL. 



**Returns:**

Blob size in bytes; 0 for a NULL Plan. 





        

<hr>
## Macro Definition Documentation





### define DP\_WFM\_PLAN\_WHY\_NO\_NOISE 

_Why a Plan refuses an_ `snr` _: its scene carries no noise._
```C++
#define DP_WFM_PLAN_WHY_NO_NOISE `/* multi line expression */`
```



A segment is noisy when a source in it has an `snr` below WFM\_SYNTH\_SNR\_CLEAN (100 dB), which gives the segment a noise floor that an `snr` override moves. A scene with no such segment has nothing for the override to move, so `dp_wfm_plan_at()` and a `"snr"` key to `dp_wfm_plan_render()` are refused rather than returning the clean signal at every SNR  a sweep over it would read a perfect receiver. The static reason every face reports. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_plan.h`

