

# File wfm\_surface.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_surface.h**](wfm__surface_8h.md)

[Go to the source code of this file](wfm__surface_8h_source.md)



* `#include <stddef.h>`
* `#include "doppler/wfm/wfm_compose.h"`
* `#include "doppler/wfm/wfm_names.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_surface\_exclusive\_t**](structwfm__surface__exclusive__t.md) <br> |
| struct | [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md) <br> |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_surface\_8h\_1a385c44f6fb256e5716a2302a5b940388**](#enum-wfm__surface_8h_1a385c44f6fb256e5716a2302a5b940388)  <br> |
| enum  | [**wfm\_json\_level\_t**](#enum-wfm_json_level_t)  <br> |
| enum  | [**wfm\_surf\_owner\_t**](#enum-wfm_surf_owner_t)  <br> |
| enum  | [**wfm\_sv\_kind\_t**](#enum-wfm_sv_kind_t)  <br> |






## Public Static Attributes

| Type | Name |
| ---: | :--- |
|  const char \*const  \*const | [**WFM\_JSON\_KEYS**](#variable-wfm_json_keys)   = `/* multi line expression */`<br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_FIELD**](#variable-wfm_json_keys_field)   = `/* multi line expression */`<br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_FRAME**](#variable-wfm_json_keys_frame)   = `/* multi line expression */`<br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_INLINE\_SEGMENT**](#variable-wfm_json_keys_inline_segment)  <br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_ROOT**](#variable-wfm_json_keys_root)   = `/* multi line expression */`<br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_SOURCE**](#variable-wfm_json_keys_source)  <br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_STAGE**](#variable-wfm_json_keys_stage)   = `/* multi line expression */`<br> |
|  const char \*const | [**WFM\_JSON\_KEYS\_SUM\_SEGMENT**](#variable-wfm_json_keys_sum_segment)   = `/* multi line expression */`<br> |
|  const [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md) | [**WFM\_SURFACE**](#variable-wfm_surface)  <br> |
|  const [**wfm\_surface\_exclusive\_t**](structwfm__surface__exclusive__t.md) | [**WFM\_SURFACE\_EXCLUSIVE**](#variable-wfm_surface_exclusive)   = `/* multi line expression */`<br> |
















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**wfm\_surface\_row\_is\_set**](#function-wfm_surface_row_is_set) (const [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md) \* r, const void \* base) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_SURFACE\_HELP\_AMPLITUDE**](wfm__surface_8h.md#define-wfm_surface_help_amplitude)  `"  --level DB[:DB] Source power in dBFS (&lt;= 0; 0 is unit power). (default 0.0)\n"`<br> |
| define  | [**WFM\_SURFACE\_HELP\_BITS**](wfm__surface_8h.md#define-wfm_surface_help_bits)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_CODED**](wfm__surface_8h.md#define-wfm_surface_help_coded)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DOPPLER**](wfm__surface_8h.md#define-wfm_surface_help_doppler)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DSSS\_BURST**](wfm__surface_8h.md#define-wfm_surface_help_dsss_burst)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DSSS\_CONT**](wfm__surface_8h.md#define-wfm_surface_help_dsss_cont)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_NOISE**](wfm__surface_8h.md#define-wfm_surface_help_noise)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_PN**](wfm__surface_8h.md#define-wfm_surface_help_pn)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_PULSE**](wfm__surface_8h.md#define-wfm_surface_help_pulse)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_SIGNAL**](wfm__surface_8h.md#define-wfm_surface_help_signal)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_SYMBOLS**](wfm__surface_8h.md#define-wfm_surface_help_symbols)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_TYPE**](wfm__surface_8h.md#define-wfm_surface_help_type)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_N\_EXCLUSIVE**](wfm__surface_8h.md#define-wfm_surface_n_exclusive)  `1`<br> |
| define  | [**WFM\_SURFACE\_REPS\_WHY\_CLI**](wfm__surface_8h.md#define-wfm_surface_reps_why_cli)  `"only --acq-code repeats (a preamble): drop the \*REPS"`<br> |
| define  | [**WFM\_SURFACE\_REPS\_WHY\_JSON**](wfm__surface_8h.md#define-wfm_surface_reps_why_json)  `"only \"acq\_code\" repeats (a preamble): drop the \*REPS"`<br> |

## Public Types Documentation




### enum wfm\_\_surface\_8h\_1a385c44f6fb256e5716a2302a5b940388 

```C++
enum wfm__surface_8h_1a385c44f6fb256e5716a2302a5b940388 {
    WFM_SURFACE_source_type,
    WFM_SURFACE_source_freq,
    WFM_SURFACE_source_snr,
    WFM_SURFACE_source_snr_mode,
    WFM_SURFACE_source_seed,
    WFM_SURFACE_source_sps,
    WFM_SURFACE_source_pn_length,
    WFM_SURFACE_source_pn_poly,
    WFM_SURFACE_source_lfsr,
    WFM_SURFACE_source_level,
    WFM_SURFACE_source_background,
    WFM_SURFACE_source_f_end,
    WFM_SURFACE_source_span,
    WFM_SURFACE_source_doppler,
    WFM_SURFACE_source_doppler_rate,
    WFM_SURFACE_source_carrier_hz,
    WFM_SURFACE_source_doppler_lifetime,
    WFM_SURFACE_source_modulation,
    WFM_SURFACE_source_pulse,
    WFM_SURFACE_source_rrc_beta,
    WFM_SURFACE_source_rrc_span,
    WFM_SURFACE_source_symbols,
    WFM_SURFACE_source_acq_code,
    WFM_SURFACE_source_data_code,
    WFM_SURFACE_source_symbol_rate,
    WFM_SURFACE_source_dsss_code_only,
    WFM_SURFACE_source_frame,
    WFM_SURFACE_source_data_from_file,
    WFM_SURFACE_source_data,
    WFM_SURFACE_source_data_len,
    WFM_SURFACE_source_fill,
    WFM_SURFACE_segment_fs,
    WFM_SURFACE_segment_num_samples,
    WFM_SURFACE_segment_off_samples,
    WFM_SURFACE_segment_repeats,
    WFM_SURFACE_segment_delay_samples,
    WFM_SURFACE_segment_gap_noise,
    WFM_SURFACE_N
};
```




<hr>



### enum wfm\_json\_level\_t 

```C++
enum wfm_json_level_t {
    WFM_JSON_ROOT,
    WFM_JSON_INLINE_SEGMENT,
    WFM_JSON_SUM_SEGMENT,
    WFM_JSON_SOURCE,
    WFM_JSON_FRAME,
    WFM_JSON_FIELD,
    WFM_JSON_STAGE,
    WFM_JSON_N_LEVELS
};
```




<hr>



### enum wfm\_surf\_owner\_t 

```C++
enum wfm_surf_owner_t {
    WFM_SURF_SOURCE,
    WFM_SURF_SEGMENT
};
```




<hr>



### enum wfm\_sv\_kind\_t 

```C++
enum wfm_sv_kind_t {
    WFM_SV_DOUBLE,
    WFM_SV_INT,
    WFM_SV_SIZE,
    WFM_SV_U32,
    WFM_SV_U64,
    WFM_SV_CHOICE,
    WFM_SV_SYMBOLS,
    WFM_SV_FIELD,
    WFM_SV_BESPOKE
};
```




<hr>
## Public Static Attributes Documentation




### variable WFM\_JSON\_KEYS 

```C++
const char* const* const WFM_JSON_KEYS[WFM_JSON_N_LEVELS];
```




<hr>



### variable WFM\_JSON\_KEYS\_FIELD 

```C++
const char* const WFM_JSON_KEYS_FIELD[];
```




<hr>



### variable WFM\_JSON\_KEYS\_FRAME 

```C++
const char* const WFM_JSON_KEYS_FRAME[];
```




<hr>



### variable WFM\_JSON\_KEYS\_INLINE\_SEGMENT 

```C++
const char* const WFM_JSON_KEYS_INLINE_SEGMENT[];
```




<hr>



### variable WFM\_JSON\_KEYS\_ROOT 

```C++
const char* const WFM_JSON_KEYS_ROOT[];
```




<hr>



### variable WFM\_JSON\_KEYS\_SOURCE 

```C++
const char* const WFM_JSON_KEYS_SOURCE[];
```




<hr>



### variable WFM\_JSON\_KEYS\_STAGE 

```C++
const char* const WFM_JSON_KEYS_STAGE[];
```




<hr>



### variable WFM\_JSON\_KEYS\_SUM\_SEGMENT 

```C++
const char* const WFM_JSON_KEYS_SUM_SEGMENT[];
```




<hr>



### variable WFM\_SURFACE 

```C++
const wfm_surface_row_t WFM_SURFACE[WFM_SURFACE_N];
```




<hr>



### variable WFM\_SURFACE\_EXCLUSIVE 

```C++
const wfm_surface_exclusive_t WFM_SURFACE_EXCLUSIVE[1];
```




<hr>
## Public Static Functions Documentation




### function wfm\_surface\_row\_is\_set 

```C++
static inline int wfm_surface_row_is_set (
    const wfm_surface_row_t * r,
    const void * base
) 
```




<hr>
## Macro Definition Documentation





### define WFM\_SURFACE\_HELP\_AMPLITUDE 

```C++
#define WFM_SURFACE_HELP_AMPLITUDE `"  --level DB[:DB] Source power in dBFS (<= 0; 0 is unit power). (default 0.0)\n"`
```




<hr>



### define WFM\_SURFACE\_HELP\_BITS 

```C++
#define WFM_SURFACE_HELP_BITS `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_CODED 

```C++
#define WFM_SURFACE_HELP_CODED `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_DOPPLER 

```C++
#define WFM_SURFACE_HELP_DOPPLER `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_DSSS\_BURST 

```C++
#define WFM_SURFACE_HELP_DSSS_BURST `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_DSSS\_CONT 

```C++
#define WFM_SURFACE_HELP_DSSS_CONT `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_NOISE 

```C++
#define WFM_SURFACE_HELP_NOISE `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_PN 

```C++
#define WFM_SURFACE_HELP_PN `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_PULSE 

```C++
#define WFM_SURFACE_HELP_PULSE `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_SIGNAL 

```C++
#define WFM_SURFACE_HELP_SIGNAL `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_SYMBOLS 

```C++
#define WFM_SURFACE_HELP_SYMBOLS `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_HELP\_TYPE 

```C++
#define WFM_SURFACE_HELP_TYPE `/* multi line expression */`
```




<hr>



### define WFM\_SURFACE\_N\_EXCLUSIVE 

```C++
#define WFM_SURFACE_N_EXCLUSIVE `1`
```




<hr>



### define WFM\_SURFACE\_REPS\_WHY\_CLI 

```C++
#define WFM_SURFACE_REPS_WHY_CLI `"only --acq-code repeats (a preamble): drop the *REPS"`
```




<hr>



### define WFM\_SURFACE\_REPS\_WHY\_JSON 

```C++
#define WFM_SURFACE_REPS_WHY_JSON `"only \"acq_code\" repeats (a preamble): drop the *REPS"`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_surface.h`

