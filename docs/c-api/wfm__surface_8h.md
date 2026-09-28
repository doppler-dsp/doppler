

# File wfm\_surface.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_surface.h**](wfm__surface_8h.md)

[Go to the source code of this file](wfm__surface_8h_source.md)



* `#include <stddef.h>`
* `#include "doppler/wfm/wfm_compose.h"`
* `#include "doppler/wfm/wfm_names.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md) <br> |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_surface\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d**](#enum-wfm__surface_8h_1abc5c98fcc1211af2b80116dd6e0a035d)  <br> |
| enum  | [**wfm\_surf\_owner\_t**](#enum-wfm_surf_owner_t)  <br> |
| enum  | [**wfm\_sv\_kind\_t**](#enum-wfm_sv_kind_t)  <br> |






## Public Static Attributes

| Type | Name |
| ---: | :--- |
|  const [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md) | [**WFM\_SURFACE**](#variable-wfm_surface)  <br> |









































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_SURFACE\_HELP\_AMPLITUDE**](wfm__surface_8h.md#define-wfm_surface_help_amplitude)  `"  --level DB[:DB] Source power in dBFS (&lt;= 0; 0 is unit power). (default 0.0)\n"`<br> |
| define  | [**WFM\_SURFACE\_HELP\_BITS**](wfm__surface_8h.md#define-wfm_surface_help_bits)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DOPPLER**](wfm__surface_8h.md#define-wfm_surface_help_doppler)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DSSS\_BURST**](wfm__surface_8h.md#define-wfm_surface_help_dsss_burst)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_DSSS\_CONT**](wfm__surface_8h.md#define-wfm_surface_help_dsss_cont)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_NOISE**](wfm__surface_8h.md#define-wfm_surface_help_noise)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_PN**](wfm__surface_8h.md#define-wfm_surface_help_pn)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_PULSE**](wfm__surface_8h.md#define-wfm_surface_help_pulse)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_SIGNAL**](wfm__surface_8h.md#define-wfm_surface_help_signal)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_SYMBOLS**](wfm__surface_8h.md#define-wfm_surface_help_symbols)  `/* multi line expression */`<br> |
| define  | [**WFM\_SURFACE\_HELP\_TYPE**](wfm__surface_8h.md#define-wfm_surface_help_type)  `/* multi line expression */`<br> |

## Public Types Documentation




### enum wfm\_\_surface\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d 

```C++
enum wfm__surface_8h_1abc5c98fcc1211af2b80116dd6e0a035d {
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
    WFM_SURFACE_source_crc,
    WFM_SURFACE_source_symbol_rate,
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
    WFM_SV_SYMBOLS
};
```




<hr>
## Public Static Attributes Documentation




### variable WFM\_SURFACE 

```C++
const wfm_surface_row_t WFM_SURFACE[WFM_SURFACE_N];
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

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_surface.h`

