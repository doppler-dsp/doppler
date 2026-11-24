

# File wfm\_compose\_bridge.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm\_compose**](dir_6d794a7fa9423fe9f7b14be42b83e035.md) **>** [**wfm\_compose\_bridge.h**](wfm__compose__bridge_8h.md)

[Go to the source code of this file](wfm__compose__bridge_8h_source.md)



* `#include "doppler/wfm/wfm_compose.h"`
* `#include "doppler/wfm_synth/wfm_synth_core.h"`
* `#include "doppler/frame/frame_core.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**dp\_wfm\_frame\_copy**](#function-dp_wfm_frame_copy) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \*) <br> |
|  void | [**dp\_wfm\_frame\_free**](#function-dp_wfm_frame_free) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \*) <br> |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**dp\_wfm\_frame\_refuse\_text**](#function-dp_wfm_frame_refuse_text) (const char \*, const char \*\* why) <br> |
|  char \* | [**dp\_wfm\_frame\_to\_json**](#function-dp_wfm_frame_to_json) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \*) <br> |
|  double | [**dp\_wfm\_segment\_fs**](#function-dp_wfm_segment_fs) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \*, double) <br> |
|  size\_t | [**dp\_wfm\_source\_acq\_code\_bits**](#function-dp_wfm_source_acq_code_bits) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double, uint8\_t \*, size\_t) <br> |
|  size\_t | [**dp\_wfm\_source\_bits\_refuse\_text**](#function-dp_wfm_source_bits_refuse_text) (const char \*, uint8\_t \*, size\_t, const char \*\*) <br> |
|  int | [**dp\_wfm\_source\_code\_rate**](#function-dp_wfm_source_code_rate) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  int | [**dp\_wfm\_source\_constellation**](#function-dp_wfm_source_constellation) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  size\_t | [**dp\_wfm\_source\_crc\_refuse\_text**](#function-dp_wfm_source_crc_refuse_text) (const char \*, uint8\_t \*, size\_t, const char \*\*) <br> |
|  size\_t | [**dp\_wfm\_source\_data\_bits**](#function-dp_wfm_source_data_bits) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double, uint8\_t \*, size\_t) <br> |
|  size\_t | [**dp\_wfm\_source\_data\_code\_bits**](#function-dp_wfm_source_data_code_bits) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double, uint8\_t \*, size\_t) <br> |
|  size\_t | [**dp\_wfm\_source\_fill\_bits**](#function-dp_wfm_source_fill_bits) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double, uint8\_t \*, size\_t) <br> |
|  double | [**dp\_wfm\_source\_fs**](#function-dp_wfm_source_fs) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  int | [**dp\_wfm\_source\_mac**](#function-dp_wfm_source_mac) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  size\_t | [**dp\_wfm\_source\_mac\_header\_bits**](#function-dp_wfm_source_mac_header_bits) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double, uint8\_t \*, size\_t) <br> |
|  const char \* | [**dp\_wfm\_source\_set\_code\_rate**](#function-dp_wfm_source_set_code_rate) ([**wfm\_source\_t**](structwfm__source__t.md) \*, int) <br> |
|  const char \* | [**dp\_wfm\_source\_set\_constellation**](#function-dp_wfm_source_set_constellation) ([**wfm\_source\_t**](structwfm__source__t.md) \*, int) <br> |
|  const char \* | [**dp\_wfm\_source\_set\_wlan**](#function-dp_wfm_source_set_wlan) ([**wfm\_source\_t**](structwfm__source__t.md) \*, int) <br> |
|  size\_t | [**dp\_wfm\_source\_sync\_refuse\_text**](#function-dp_wfm_source_sync_refuse_text) (const char \*, uint8\_t \*, size\_t, const char \*\*) <br> |
|  [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* | [**dp\_wfm\_source\_to\_synth**](#function-dp_wfm_source_to_synth) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  const char \* | [**dp\_wfm\_source\_to\_synth\_error**](#function-dp_wfm_source_to_synth_error) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |
|  size\_t | [**dp\_wfm\_source\_window**](#function-dp_wfm_source_window) (const [**wfm\_source\_t**](structwfm__source__t.md) \*, double) <br> |




























## Public Functions Documentation




### function dp\_wfm\_frame\_copy 

```C++
wfm_frame_desc_t * dp_wfm_frame_copy (
    const wfm_frame_desc_t *
) 
```




<hr>



### function dp\_wfm\_frame\_free 

```C++
void dp_wfm_frame_free (
    wfm_frame_desc_t *
) 
```




<hr>



### function dp\_wfm\_frame\_refuse\_text 

```C++
wfm_frame_desc_t * dp_wfm_frame_refuse_text (
    const char *,
    const char ** why
) 
```




<hr>



### function dp\_wfm\_frame\_to\_json 

```C++
char * dp_wfm_frame_to_json (
    const wfm_frame_desc_t *
) 
```




<hr>



### function dp\_wfm\_segment\_fs 

```C++
double dp_wfm_segment_fs (
    const wfm_segment_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_acq\_code\_bits 

```C++
size_t dp_wfm_source_acq_code_bits (
    const wfm_source_t *,
    double,
    uint8_t *,
    size_t
) 
```




<hr>



### function dp\_wfm\_source\_bits\_refuse\_text 

```C++
size_t dp_wfm_source_bits_refuse_text (
    const char *,
    uint8_t *,
    size_t,
    const char **
) 
```




<hr>



### function dp\_wfm\_source\_code\_rate 

```C++
int dp_wfm_source_code_rate (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_constellation 

```C++
int dp_wfm_source_constellation (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_crc\_refuse\_text 

```C++
size_t dp_wfm_source_crc_refuse_text (
    const char *,
    uint8_t *,
    size_t,
    const char **
) 
```




<hr>



### function dp\_wfm\_source\_data\_bits 

```C++
size_t dp_wfm_source_data_bits (
    const wfm_source_t *,
    double,
    uint8_t *,
    size_t
) 
```




<hr>



### function dp\_wfm\_source\_data\_code\_bits 

```C++
size_t dp_wfm_source_data_code_bits (
    const wfm_source_t *,
    double,
    uint8_t *,
    size_t
) 
```




<hr>



### function dp\_wfm\_source\_fill\_bits 

```C++
size_t dp_wfm_source_fill_bits (
    const wfm_source_t *,
    double,
    uint8_t *,
    size_t
) 
```




<hr>



### function dp\_wfm\_source\_fs 

```C++
double dp_wfm_source_fs (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_mac 

```C++
int dp_wfm_source_mac (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_mac\_header\_bits 

```C++
size_t dp_wfm_source_mac_header_bits (
    const wfm_source_t *,
    double,
    uint8_t *,
    size_t
) 
```




<hr>



### function dp\_wfm\_source\_set\_code\_rate 

```C++
const char * dp_wfm_source_set_code_rate (
    wfm_source_t *,
    int
) 
```




<hr>



### function dp\_wfm\_source\_set\_constellation 

```C++
const char * dp_wfm_source_set_constellation (
    wfm_source_t *,
    int
) 
```




<hr>



### function dp\_wfm\_source\_set\_wlan 

```C++
const char * dp_wfm_source_set_wlan (
    wfm_source_t *,
    int
) 
```




<hr>



### function dp\_wfm\_source\_sync\_refuse\_text 

```C++
size_t dp_wfm_source_sync_refuse_text (
    const char *,
    uint8_t *,
    size_t,
    const char **
) 
```




<hr>



### function dp\_wfm\_source\_to\_synth 

```C++
dp_wfm_synth_state_t * dp_wfm_source_to_synth (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_to\_synth\_error 

```C++
const char * dp_wfm_source_to_synth_error (
    const wfm_source_t *,
    double
) 
```




<hr>



### function dp\_wfm\_source\_window 

```C++
size_t dp_wfm_source_window (
    const wfm_source_t *,
    double
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm_compose/wfm_compose_bridge.h`

