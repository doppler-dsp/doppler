

# File wfm\_compose.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the source code of this file](wfm__compose_8h_source.md)

_Multi-segment waveform composer (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "wfm_synth/wfm_synth_core.h"`
* `#include "wfm/wfm_frame.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_segment\_t**](structwfm__segment__t.md) <br>_One composer segment: one or more sources summed over the same span, then a trailing off-time gap._  |
| struct | [**wfm\_source\_t**](structwfm__source__t.md) <br>_One additive source within a segment: a_ `synth` _config + its level._ |
| struct | [**wfm\_span\_t**](structwfm__span__t.md) <br>_One rendered segment instance's exact timing: where it lands in the composed stream and how its_ `delay | on | off` _spans divide it._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_compose\_8h\_1ab04a0655cd1e3bcac5e8f48c18df1a57**](#enum-wfm__compose_8h_1ab04a0655cd1e3bcac5e8f48c18df1a57)  <br>_Per-field "draw uniformly each repeat" flags (_ `ranged` _bitmask)._ |
| typedef struct wfm\_compose\_state | [**wfm\_compose\_state\_t**](#typedef-wfm_compose_state_t)  <br> |
| enum  | [**wfm\_seed\_advance\_t**](#enum-wfm_seed_advance_t)  <br>_Per-repeat seed policy for a looped/continuous stream._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* | [**wfm\_compose\_build\_synth**](#function-wfm_compose_build_synth) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, size\_t on\_len, double freq, double snr, double f\_end, unsigned epoch, int seed\_advance, size\_t instance) <br>_Construct + configure the synth for one resolved source._  |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_create**](#function-wfm_compose_create) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous) <br>_Build a composer over a copy of_ `segs` _._ |
|  void | [**wfm\_compose\_destroy**](#function-wfm_compose_destroy) ([**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state) <br>_Destroy a composer and its active synth._  |
|  size\_t | [**wfm\_compose\_execute**](#function-wfm_compose_execute) ([**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state, float complex \* out, size\_t max) <br>_Emit up to_ `max` _samples of the composed stream._ |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_from\_file**](#function-wfm_compose_from_file) (const char \* path) <br>_Build a composer from a JSON spec file._  |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_from\_json**](#function-wfm_compose_from_json) (const char \* json) <br>_Build a composer from a JSON spec string (for_  _from-file)._ |
|  int | [**wfm\_compose\_seed\_advance**](#function-wfm_compose_seed_advance) (const [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state) <br>_The composer's current seed-advance mode (a_ `wfm_seed_advance_t` _)._ |
|  const [**wfm\_segment\_t**](structwfm__segment__t.md) \* | [**wfm\_compose\_segments**](#function-wfm_compose_segments) (const [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state, size\_t \* n\_out, int \* repeat, int \* continuous) <br>_Borrow the composer's stored segment list (for_  _record / SigMF)._ |
|  void | [**wfm\_compose\_set\_seed\_advance**](#function-wfm_compose_set_seed_advance) ([**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state, int mode) <br>_Choose how the seed advances on each repeat of a looped/continuous stream (a_ `wfm_seed_advance_t` _):_ |
|  size\_t | [**wfm\_compose\_spans**](#function-wfm_compose_spans) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, [**wfm\_span\_t**](structwfm__span__t.md) \* out, size\_t cap) <br>_Replay the (epoch 0) instance timeline of a resolved segment list._  |
|  int | [**wfm\_resolve\_noise**](#function-wfm_resolve_noise) ([**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n) <br>_Resolve a segment list's noise model in place (Phase 4b)._  |
|  double | [**wfm\_snr\_over\_fs**](#function-wfm_snr_over_fs) (int snr\_mode, int type, int sps, size\_t sf, double sym\_span, double snr) <br>_SNR (dB) referred to fs, from a source's snr/snr\_mode/sps/type._  |
|  int | [**wfm\_source\_attach\_dsss**](#function-wfm_source_attach_dsss) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* syn, const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs) <br>_Attach a dsss source's data to a freshly-created synth._  |
|  int | [**wfm\_source\_attach\_frame**](#function-wfm_source_attach_frame) ([**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) \* syn, const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Attach an unspread source's bit pattern, framed or not._  |
|  double | [**wfm\_source\_create\_snr**](#function-wfm_source_create_snr) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, double snr, int \* snr\_mode) <br>_Resolve a source's (snr, snr\_mode) into the pair to hand to_ `wfm_synth_create()` _._ |
|  int | [**wfm\_source\_describe\_frame**](#function-wfm_source_describe_frame) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d) <br>_Describe a source's frame: the fields, the stages, and their covers._  |
|  size\_t | [**wfm\_source\_dsss\_nchips**](#function-wfm_source_dsss_nchips) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Chips one DSSS BURST from this source occupies, description and all._  |
|  const char \* | [**wfm\_source\_frame\_error**](#function-wfm_source_frame_error) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_NULL when this source's frame fields can be honoured; else why not._  |
|  int | [**wfm\_source\_has\_frame**](#function-wfm_source_has_frame) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Non-zero when this source describes a FRAME._  |
|  double | [**wfm\_spec\_headroom**](#function-wfm_spec_headroom) (const char \* json) <br>_The top-level_ `headroom` _(dB) from a spec JSON, or 0 if absent._ |
|  char \* | [**wfm\_spec\_template\_json**](#function-wfm_spec_template_json) (void) <br>_A ready-to-edit example spec in the canonical_  _from-file schema._ |
|  char \* | [**wfm\_spec\_to\_json**](#function-wfm_spec_to_json) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous, int seed\_advance, double headroom) <br>_Serialise a spec to a JSON string (for_  _record)._ |




























## Detailed Description


Sequences a list of segments — each one a `synth` configuration plus an on-time and a trailing off-time gap — into a single IQ stream, optionally repeating the whole sequence or running forever. The composer owns one `synth` at a time (the active segment) and reuses the Phase-A engine verbatim, so every waveform type / SNR mode / MLS behaviour is identical to the single-waveform path; a one-segment spec is byte-identical to calling `synth` directly.


Lifecycle: wfm\_compose\_create -&gt; wfm\_compose\_execute\* -&gt; wfm\_compose\_destroy



```C++
wfm_source_t tone = {.type = 0, .freq = 1e5, .snr = 100.0};
wfm_source_t qpsk = {.type = 4, .sps = 8, .snr = 9.0};
wfm_segment_t segs[2] = {
    {.sources = &tone, .n_sources = 1, .fs = 1e6,
     .num_samples = 1000, .off_samples = 500},          // tone, then a gap
    {.sources = &qpsk, .n_sources = 1, .fs = 1e6,
     .num_samples = 4096, .off_samples = 0},            // qpsk
};
wfm_compose_state_t *c = wfm_compose_create(segs, 2, 0, 0);
float complex buf[4096];
size_t n;
while ((n = wfm_compose_execute(c, buf, 4096)) > 0) { ... }
wfm_compose_destroy(c);
```
 


    
## Public Types Documentation




### enum wfm\_\_compose\_8h\_1ab04a0655cd1e3bcac5e8f48c18df1a57 

_Per-field "draw uniformly each repeat" flags (_ `ranged` _bitmask)._
```C++
enum wfm__compose_8h_1ab04a0655cd1e3bcac5e8f48c18df1a57 {
    WFM_RANGE_FREQ = 1u << 0,
    WFM_RANGE_SNR = 1u << 1,
    WFM_RANGE_LEVEL = 1u << 2,
    WFM_RANGE_FEND = 1u << 3,
    WFM_RANGE_NUM_SAMPLES = 1u << 4,
    WFM_RANGE_OFF_SAMPLES = 1u << 5,
    WFM_RANGE_DELAY_SAMPLES = 1u << 6
};
```



A scalar field is a constant; a _ranged_ field carries a `[lo, hi]` span (the scalar holds `lo`, a companion `*_hi` holds `hi`) and is redrawn uniformly in `[lo, hi]` at the start of every repeat (composer epoch) — so a looped / continuous stream can vary Doppler (`freq`), arrival jitter (`off_samples`), etc. burst-to-burst while staying _reproducible_: the draw is a deterministic hash of the source seed, the epoch, the segment/source index, and the field, so `--record` stores the span (not a drawn value) and `--from-file` replays the same sequence byte-for-byte. Bits 0–3 live on `wfm_source_t.ranged`; bits 4–6 on `wfm_segment_t.ranged`. 


        

<hr>



### typedef wfm\_compose\_state\_t 

```C++
typedef struct wfm_compose_state wfm_compose_state_t;
```



Opaque composer state. 


        

<hr>



### enum wfm\_seed\_advance\_t 

_Per-repeat seed policy for a looped/continuous stream._ 
```C++
enum wfm_seed_advance_t {
    WFM_SEED_ADVANCE_NONE = 0,
    WFM_SEED_ADVANCE_NOISE = 1,
    WFM_SEED_ADVANCE_ALL = 2
};
```



A source's single `seed` feeds two RNGs: the PN LFSR (spreading code _and_ data bits — one register) and the AWGN generator. The clean cut is therefore signal (code+data) vs. noise, exposed as an ordered, cumulative level. 


        

<hr>
## Public Functions Documentation




### function wfm\_compose\_build\_synth 

_Construct + configure the synth for one resolved source._ 
```C++
wfm_synth_state_t * wfm_compose_build_synth (
    const wfm_source_t * src,
    double fs,
    size_t on_len,
    double freq,
    double snr,
    double f_end,
    unsigned epoch,
    int seed_advance,
    size_t instance
) 
```



THE single synth-construction path (create + chirp-span pin + bits/symbols/RRC attach + per-repeat NOISE reseed) shared by the streaming composer and the Plan stimulus cache, so a cached per-source render is byte-identical to the composed one. `freq/snr/f_end` are passed already ranged-resolved by the caller; `on_len` pins a chirp's sweep to the on-time; `epoch`/`seed_advance` (a [**wfm\_seed\_advance\_t**](wfm__compose_8h.md#enum-wfm_seed_advance_t)) drive the per-repeat seed policy — `epoch == 0` yields the unmodified seed. `instance` is the segment's `repeats` counter (0-based): a non-zero instance always reseeds the AWGN (fresh noise per burst instance, signal fixed, regardless of `seed_advance`); instance 0 is byte-identical to the pre-`repeats` behaviour.




**Returns:**

A heap synth (caller [**wfm\_synth\_destroy()**](wfm__synth__core_8h.md#function-wfm_synth_destroy)s it), or NULL on failure. 





        

<hr>



### function wfm\_compose\_create 

_Build a composer over a copy of_ `segs` _._
```C++
wfm_compose_state_t * wfm_compose_create (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous
) 
```





**Parameters:**


* `segs` Segment list (copied; caller keeps ownership). 
* `n_segs` Number of segments (&gt;= 1). 
* `repeat` Non-zero: loop the whole sequence after the last segment. 
* `continuous` Non-zero: never finish (implies repeat); execute always returns `max`. 



**Returns:**

Heap state, or NULL on bad args / allocation / synth failure. 




**Note:**

Caller must [**wfm\_compose\_destroy()**](wfm__compose_8h.md#function-wfm_compose_destroy) when done. 





        

<hr>



### function wfm\_compose\_destroy 

_Destroy a composer and its active synth._ 
```C++
void wfm_compose_destroy (
    wfm_compose_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function wfm\_compose\_execute 

_Emit up to_ `max` _samples of the composed stream._
```C++
size_t wfm_compose_execute (
    wfm_compose_state_t * state,
    float complex * out,
    size_t max
) 
```





**Returns:**

Number of samples written: &lt; `max` (or 0) signals the sequence finished (never, when `continuous`). 





        

<hr>



### function wfm\_compose\_from\_file 

_Build a composer from a JSON spec file._ 
```C++
wfm_compose_state_t * wfm_compose_from_file (
    const char * path
) 
```





**Returns:**

Composer state, or NULL on read/parse error. 





        

<hr>



### function wfm\_compose\_from\_json 

_Build a composer from a JSON spec string (for_  _from-file)._
```C++
wfm_compose_state_t * wfm_compose_from_json (
    const char * json
) 
```





**Returns:**

Composer state, or NULL on parse error / bad type / no segments. 





        

<hr>



### function wfm\_compose\_seed\_advance 

_The composer's current seed-advance mode (a_ `wfm_seed_advance_t` _)._
```C++
int wfm_compose_seed_advance (
    const wfm_compose_state_t * state
) 
```



The composer is the SSOT for it: `--from-file` sets it from the spec and the flag path sets it from `--seed-advance`, so a serialiser must read it back from here rather than from whichever half happened to supply it. 

**Parameters:**


* `state` Compose state (may be NULL → `WFM_SEED_ADVANCE_NONE`). 




        

<hr>



### function wfm\_compose\_segments 

_Borrow the composer's stored segment list (for_  _record / SigMF)._
```C++
const wfm_segment_t * wfm_compose_segments (
    const wfm_compose_state_t * state,
    size_t * n_out,
    int * repeat,
    int * continuous
) 
```





**Parameters:**


* `state` the composer. 
* `n_out` receives the segment count. 
* `repeat` receives the repeat flag (may be NULL). 
* `continuous` receives the continuous flag (may be NULL). 



**Returns:**

Pointer to the internal segments (owned by the composer; valid until wfm\_compose\_destroy). 





        

<hr>



### function wfm\_compose\_set\_seed\_advance 

_Choose how the seed advances on each repeat of a looped/continuous stream (a_ `wfm_seed_advance_t` _):_
```C++
void wfm_compose_set_seed_advance (
    wfm_compose_state_t * state,
    int mode
) 
```




* `WFM_SEED_ADVANCE_NONE` (default): byte-identical repeats.
* `WFM_SEED_ADVANCE_NOISE`: advance only the AWGN seed → a fresh noise realization each pass while the signal (LO / PN code / data / pulse) stays bit-identical (so a fixed preamble/code re-acquires every burst).
* `WFM_SEED_ADVANCE_ALL`: advance the whole seed → code, data, and noise all change (a fully stochastic stream).




Set before the first execute(); the first pass is always unchanged. An out-of-range mode is ignored. 

**Parameters:**


* `state` Compose state (may be NULL). 
* `mode` A wfm\_seed\_advance\_t value. 




        

<hr>



### function wfm\_compose\_spans 

_Replay the (epoch 0) instance timeline of a resolved segment list._ 
```C++
size_t wfm_compose_spans (
    const wfm_segment_t * segs,
    size_t n_segs,
    wfm_span_t * out,
    size_t cap
) 
```



Walks every segment's `repeats` instances, re-deriving each instance's drawn delay/on/off exactly as the streaming composer will (identical draw hash), and fills `out` with up to `cap` spans in stream order. Returns the TOTAL instance count regardless of `cap` — call once with cap 0 to size, then again with a buffer. Pass the RESOLVED segments ([**wfm\_compose\_segments()**](wfm__compose_8h.md#function-wfm_compose_segments) on a live composer) so intrinsic on-times (dsss) are already folded in.


Assumes every segment builds: a segment that fails at render time (invalid burst geometry) degrades to its gaps only, so positions after it would shift relative to this replay.




**Parameters:**


* `segs` Resolved segment array. 
* `n_segs` Segment count. 
* `out` Span buffer (may be NULL when cap is 0). 
* `cap` Capacity of out in spans. 



**Returns:**

Total number of instances in one pass of the spec. 





        

<hr>



### function wfm\_resolve\_noise 

_Resolve a segment list's noise model in place (Phase 4b)._ 
```C++
int wfm_resolve_noise (
    wfm_segment_t * segs,
    size_t n
) 
```



No-op for 1-source segments (keeps the bundled-synth path byte-identical). For a multi-source segment it sets one shared noise floor (from an explicit WFM\_SYNTH\_NOISE source, else the first snr-bearing source), cleans the signal sources, and appends a WFM\_SYNTH\_NOISE source at the floor — so the composer's accumulator just sums. May `realloc` each segment's `sources`. Idempotent.


`wfm_compose_create()` calls this on its private copy, so every face (CLI, JSON, Python) resolves identically.




**Returns:**

0 on success; -1 if a non-anchor source over-specifies (snr + level) or on allocation failure. 





        

<hr>



### function wfm\_snr\_over\_fs 

_SNR (dB) referred to fs, from a source's snr/snr\_mode/sps/type._ 
```C++
double wfm_snr_over_fs (
    int snr_mode,
    int type,
    int sps,
    size_t sf,
    double sym_span,
    double snr
) 
```



The single source of truth for the Es/No, Eb/No, and over-fs conventions (`snr_mode` 0 auto / 1 fs / 2 ebno / 3 esno). `wfm_resolve_noise()` uses it to place the shared noise floor at `level(anchor) − wfm_snr_over_fs(anchor)`, and the Plan stimulus engine reuses it to recompute the floor at an arbitrary swept SNR — so both agree to the bit.


For `type=dsss` the symbol is the outer _data_ symbol. For a BURST that spans `sf * sps` samples (sf chips, sps samples per chip). For a CONTINUOUS async stream the data clock is independent of the code, so the span is `fs / symbol_rate` samples — passed as `sym_span` (non-integer), which OVERRIDES the `sf·sps` reconstruction when non-zero. `auto` picks esno, and esno/ebno convert as `snr − 10·log10(span)` (BPSK payload, so the two coincide). Every other type ignores `sf` and `sym_span`.




**Parameters:**


* `snr_mode` 0 auto, 1 fs, 2 ebno, 3 esno. 
* `type` A WFM\_SYNTH\_\* waveform type (selects the auto convention). 
* `sps` Samples per symbol/chip (≥1; &lt;1 treated as 1). 
* `sf` Spreading factor — chips per data symbol (burst dsss; ≥1, &lt;1 treated as 1). 
* `sym_span` Continuous-dsss symbol span in samples (`fs/symbol_rate`); 0 = burst/non-dsss, derive from `sf·sps`. 
* `snr` The declared SNR in dB. 



**Returns:**

SNR over fs in dB. 





        

<hr>



### function wfm\_source\_attach\_dsss 

_Attach a dsss source's data to a freshly-created synth._ 
```C++
int wfm_source_attach_dsss (
    wfm_synth_state_t * syn,
    const wfm_source_t * src,
    double fs
) 
```



The single dsss-attach path, called by BOTH synth-construction faces (`wfm_compose_build_synth` and the standalone `wfm_source_to_synth`), so the two cannot drift on how a dsss stream is configured. Selects on `symbol_rate`: 0 → the burst form (`wfm_synth_set_dsss`); &gt; 0 → the continuous form (`wfm_synth_set_dsss_cont`) with `chips_per_symbol = (fs/sps)/symbol_rate`, taking the data from the payload when one is supplied (`bits`) and otherwise from the seeded PN. A no-op for a non-dsss source.




**Parameters:**


* `syn` A synth from [**wfm\_synth\_create()**](wfm__synth__core_8h.md#function-wfm_synth_create) with `wtype == WFM_SYNTH_DSSS`. 
* `src` The source (codes, payload, symbol\_rate, pn config). 
* `fs` Segment sample rate (Hz) — the continuous chip rate is fs/sps. 



**Returns:**

0 on success (or non-dsss no-op); -1 on invalid geometry. 





        

<hr>



### function wfm\_source\_attach\_frame 

_Attach an unspread source's bit pattern, framed or not._ 
```C++
int wfm_source_attach_frame (
    wfm_synth_state_t * syn,
    const wfm_source_t * src
) 
```



The `type=bits` counterpart of [**wfm\_source\_attach\_dsss()**](wfm__compose_8h.md#function-wfm_source_attach_dsss), and called from the same two places for the same reason. When the source carries a frame, the pattern handed to `wfm_synth_set_bits()` is `wfm_frame_bits()` of `[preamble x reps | sync | payload | crc]` rather than the payload alone — so the layout, the CRC's position and its bit order come from the one descriptor that the DSSS path and the receiver already read.


The frame CYCLES, exactly as an unframed pattern does: one descriptor fills whatever length is asked for, which is what turns a one-frame description into a multi-frame record.




**Parameters:**


* `syn` A synth from [**wfm\_synth\_create()**](wfm__synth__core_8h.md#function-wfm_synth_create) with `wtype == WFM_SYNTH_BITS`. 
* `src` The source (pattern, modulation, and any frame fields). 



**Returns:**

0 on success (or a non-bits/no-pattern no-op); -1 on failure. 





        

<hr>



### function wfm\_source\_create\_snr 

_Resolve a source's (snr, snr\_mode) into the pair to hand to_ `wfm_synth_create()` _._
```C++
double wfm_source_create_snr (
    const wfm_source_t * src,
    double fs,
    double snr,
    int * snr_mode
) 
```



`wfm_synth_create()` runs before a dsss source's codes are attached, so it cannot know the spreading factor its own esno would need. This helper — the one create-time entry point shared by the composer (`wfm_compose_build_synth`) and the standalone-Synth bridge (`wfm_source_to_synth`), so every face agrees to the bit — converts a dsss source's SNR to the over-fs reference (via `wfm_snr_over_fs`; the burst span is `sf = n_data_code`, a continuous stream uses `fs/symbol_rate`) and returns `snr_mode=fs`; every other type passes through unchanged.




**Parameters:**


* `src` The source (supplies type/sps/snr\_mode/n\_data\_code/ symbol\_rate). 
* `fs` Segment sample rate (Hz) — needed for a continuous dsss source's `fs/symbol_rate` span; ignored otherwise. 
* `snr` The declared SNR in dB, already ranged-resolved. 
* `snr_mode` Receives the snr\_mode for create. 



**Returns:**

The SNR in dB for create. 





        

<hr>



### function wfm\_source\_describe\_frame 

_Describe a source's frame: the fields, the stages, and their covers._ 
```C++
int wfm_source_describe_frame (
    const wfm_source_t * src,
    wfm_frame_desc_t * d
) 
```



The ONE place a `wfm_source_t`'s framing flags become a description, read by the `type=bits` assembler and the DSSS spreader alike, so the two cannot disagree about which stage covers what. For a DSSS burst the acquisition preamble is deliberately NOT a field: it is unmodulated and unspread, so it sits outside everything a stage can cover.




**Parameters:**


* `src` the source. 
* `d` receives the description. 



**Returns:**

0, or non-zero if the source cannot be described. 





        

<hr>



### function wfm\_source\_dsss\_nchips 

_Chips one DSSS BURST from this source occupies, description and all._ 
```C++
size_t wfm_source_dsss_nchips (
    const wfm_source_t * src
) 
```



What sizes a lone dsss segment's intrinsic on-time. It reads the same description the burst is assembled from, so a stage that lengthens the frame  a rate-1/2 inner code doubles it  lengthens the segment by the same arithmetic instead of by a second copy of it.




**Parameters:**


* `src` the source. 



**Returns:**

burst chips, or 0 for a non-dsss source, a CONTINUOUS dsss source (which has no intrinsic length), or an empty/refused geometry. 





        

<hr>



### function wfm\_source\_frame\_error 

_NULL when this source's frame fields can be honoured; else why not._ 
```C++
const char * wfm_source_frame_error (
    const wfm_source_t * src
) 
```



ONE rule, asked by all three faces — the wfmgen CLI before it generates, the standalone `Synth` through `wfm_source_to_synth`, and the composer through `wfm_compose_create` — because the alternative is what shipped: the flags were accepted, stored and readable back on every face, and applied on none of them, so a caller who asked for a framed waveform silently got an unframed one.


A frame needs a payload, and the unspread types that source their symbols from the PN LFSR (`bpsk`/`qpsk`/`pn`) have no length to bound one. So the frame is honoured where the payload is EXPLICIT — `type=bits` with a pattern, which `modulation` already maps to BPSK or QPSK — and refused with a reason everywhere else.




**Parameters:**


* `src` The source. 



**Returns:**

NULL if there is nothing wrong, else a static message. 





        

<hr>



### function wfm\_source\_has\_frame 

_Non-zero when this source describes a FRAME._ 
```C++
int wfm_source_has_frame (
    const wfm_source_t * src
) 
```



A preamble or a sync word is what says "framed". **Deliberately not `crc`**: it defaults to crc16 on every source (`[[module.wfm_compose.source.fields]]` and wfmgen alike), so reading it as intent would silently append a trailer to every unframed bit pattern anyone has ever generated. With neither a preamble nor a sync word, `crc` stays inert exactly as it always was.




**Parameters:**


* `src` The source; NULL reads as unframed. 




        

<hr>



### function wfm\_spec\_headroom 

_The top-level_ `headroom` _(dB) from a spec JSON, or 0 if absent._
```C++
double wfm_spec_headroom (
    const char * json
) 
```



Lets `--from-file` reproduce a recorded `--headroom`; the value is a writer gain, so it lives outside the composer state. 


        

<hr>



### function wfm\_spec\_template\_json 

_A ready-to-edit example spec in the canonical_  _from-file schema._
```C++
char * wfm_spec_template_json (
    void
) 
```



Returns a representative multi-segment template — an inline tone, an RRC-shaped QPSK-from-bits burst with a trailing gap, and a two-source additive `sum` mix — serialised with [**wfm\_spec\_to\_json()**](wfm__compose_8h.md#function-wfm_spec_to_json), so it is valid by construction and round-trips through [**wfm\_compose\_from\_json()**](wfm__compose_8h.md#function-wfm_compose_from_json) unchanged. It therefore doubles as a working starting point for `wfmgen --from-file`, not just documentation: dump it, edit the fields, feed it back.




**Returns:**

malloc'd JSON (caller frees), or NULL on allocation failure. 





        

<hr>



### function wfm\_spec\_to\_json 

_Serialise a spec to a JSON string (for_  _record)._
```C++
char * wfm_spec_to_json (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous,
    int seed_advance,
    double headroom
) 
```



`seed_advance` (a `wfm_seed_advance_t`) and `headroom` (dB of output backoff applied at the writer, not the composer) are each emitted as a top-level field only when non-default, so an unrecorded run and any older spec stay byte-identical. Read `headroom` back with [**wfm\_spec\_headroom()**](wfm__compose_8h.md#function-wfm_spec_headroom); the parser reads `seed_advance` straight onto the composer.


`seed_advance` is a parameter rather than something read from `segs` because it is a property of the whole stream, like `repeat`/`continuous`. Omitting it is what made a recorded run replay a DIFFERENT waveform (doppler#978): the key was parsed and never written, so the round-trip silently fell back to NONE and every loop after the first came out identical.




**Returns:**

malloc'd JSON (caller frees), or NULL on allocation failure. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

