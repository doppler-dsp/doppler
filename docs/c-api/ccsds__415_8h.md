

# File ccsds\_415.h



[**FileList**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_415.h**](ccsds__415_8h.md)

[Go to the source code of this file](ccsds__415_8h_source.md)

_The CCSDS 415.1-B-1 specification point the async DSSS receiver is specified for, stated once: its numbers, the derived rates a scene at it needs, and (through the receiver's and the pool's_ `Ccsds415` _views) the receiver built there._[More...](#detailed-description)

* `#include <stdbool.h>`
* `#include <stddef.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_ccsds\_415\_rx\_t**](structdp__ccsds__415__rx__t.md) <br>_The async DSSS receiver's create arguments at the point, every one but the code: what_ [_**dp\_ccsds\_415\_rx()**_](ccsds__415_8h.md#function-dp_ccsds_415_rx) _fills._ |
| struct | [**dp\_ccsds\_415\_scene\_t**](structdp__ccsds__415__scene__t.md) <br>_The numbers a scene at the point is built from, at one carrier: what_ [_**dp\_ccsds\_415\_scene()**_](ccsds__415_8h.md#function-dp_ccsds_415_scene) _returns._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_ccsds\_415\_chip\_rate**](#function-dp_ccsds_415_chip_rate) (double fc) <br>_Chips a second at carrier_ `fc:` _F\_fwd x 31 / (221 x 96) (415.1-B-1 table 4-1). Fast math off: the same bits everywhere. Python: doppler.ccsds.ccsds415\_chip\_rate()._ |
|  double | [**dp\_ccsds\_415\_cn0**](#function-dp_ccsds_415_cn0) (double esn0\_db) <br>_C/N0, dB-Hz, of an Es/N0 at the point's symbol rate. Python: doppler.ccsds.ccsds415\_cn0()._  |
|  int | [**dp\_ccsds\_415\_rx**](#function-dp_ccsds_415_rx) ([**dp\_ccsds\_415\_rx\_t**](structdp__ccsds__415__rx__t.md) \* a, size\_t code\_len, double carrier\_freq\_hz, double lost\_confirm\_s, bool refine, const char \*\* why) <br>_Fills_ `a` _with the receiver's create arguments at the point, or refuses with the reason._ |
|  [**dp\_ccsds\_415\_scene\_t**](structdp__ccsds__415__scene__t.md) | [**dp\_ccsds\_415\_scene**](#function-dp_ccsds_415_scene) (double fc, double spc, double start\_hz) <br>_The scene's derived numbers at carrier_ `fc` _, computed once so a C program and its Python twin read the same bits._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_CCSDS\_415\_CODE\_ONLY\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_code_only_sym)  `275u`<br> |
| define  | [**DP\_CCSDS\_415\_FC\_HI**](ccsds__415_8h.md#define-dp_ccsds_415_fc_hi)  `2.8e9`<br> |
| define  | [**DP\_CCSDS\_415\_FC\_LO**](ccsds__415_8h.md#define-dp_ccsds_415_fc_lo)  `2.2e9`<br> |
| define  | [**DP\_CCSDS\_415\_FLOOR\_ESN0\_DB**](ccsds__415_8h.md#define-dp_ccsds_415_floor_esn0_db)  `4.0`<br> |
| define  | [**DP\_CCSDS\_415\_FRAME\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_frame_sym)  `1000u`<br> |
| define  | [**DP\_CCSDS\_415\_FS**](ccsds__415_8h.md#define-dp_ccsds_415_fs)  `11e6`<br> |
| define  | [**DP\_CCSDS\_415\_LOCK\_S**](ccsds__415_8h.md#define-dp_ccsds_415_lock_s)  `20.0`<br> |
| define  | [**DP\_CCSDS\_415\_RX\_FC\_MAX**](ccsds__415_8h.md#define-dp_ccsds_415_rx_fc_max)  `([**DP\_CCSDS\_415\_FS**](ccsds__415_8h.md#define-dp_ccsds_415_fs) \* (221.0 \* 96.0) / 31.0)`<br> |
| define  | [**DP\_CCSDS\_415\_RX\_FC\_MIN**](ccsds__415_8h.md#define-dp_ccsds_415_rx_fc_min)  `((double)[**DP\_CCSDS\_415\_SF**](ccsds__415_8h.md#define-dp_ccsds_415_sf) \* [**DP\_CCSDS\_415\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_sym) \* (221.0 \* 96.0) / 31.0)`<br> |
| define  | [**DP\_CCSDS\_415\_R\_ENV\_HZ\_S**](ccsds__415_8h.md#define-dp_ccsds_415_r_env_hz_s)  `11.0`<br> |
| define  | [**DP\_CCSDS\_415\_SF**](ccsds__415_8h.md#define-dp_ccsds_415_sf)  `1023u`<br> |
| define  | [**DP\_CCSDS\_415\_SPAN\_HZ**](ccsds__415_8h.md#define-dp_ccsds_415_span_hz)  `1500.0`<br> |
| define  | [**DP\_CCSDS\_415\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_sym)  `3000.0`<br> |

## Detailed Description


The point's numbers are rows of docs/design/async-dsss-receiver.md §1's table, which names each source, and the frame's are §1.4's; none of them is chosen. The receiver's other arguments, in [**dp\_ccsds\_415\_rx()**](ccsds__415_8h.md#function-dp_ccsds_415_rx), are the receiver binding's defaults. The receiver's runs at this point are docs/design/async-dsss-receiver-measurements.md; its certification record holds at another point, 3.069 Mcps, and not at this one (async-dsss-receiver.md §1, The specification, whose paragraph on what the certification covers says so). A test, a validator, a bench or an example that runs at the point reads these names rather than restating them, so two programs cannot measure two points under one name.


code Gold, 1,023 chips: the mission's, so it is an argument chip rate F\_fwd x 31 / (221 x 96) table 4-1 carrier F\_fwd 2.2 to 2.8 GHz owner symbols 3,000 sym/s, asynchronous to the code owner capture 11 MSa/s, complex owner Doppler within +/-1,500 Hz owner its rate R\_env, 11 Hz/s §1.8 floor Es/N0 4 dB owner frame 1,000 symbols, the first 275 code alone owner, §1.4


The functions (native/src/ccsds\_415/, a hand-owned c\_dep as ccsds\_tm is, so a receiver links the point and not the TM coding chain) are computed with fast math off (the precedent is [**dp\_wfm\_dsss\_cont\_edge()**](wfm__dsp_8h.md#function-dp_wfm_dsss_cont_edge), wfm\_dsp.c): under -ffast-math a division is rewritten differently by GCC and by Clang, and a rate one ulp apart moves a bit error between two programs that should agree. The chip rate and the scene are IEEE arithmetic alone, so they are the same bits on every compiler: test\_ccsds\_415.c pins them to literals, and the Python chip rate is checked bit for bit. C/N0 goes through the platform's log10, so it is held to 1e-12, not bit for bit.


A C caller uses the names declared here, `dp_ccsds_415_*`. The `ccsds` module's [**dp\_ccsds415\_chip\_rate()**](ccsds__core_8h.md#function-dp_ccsds415_chip_rate) and [**dp\_ccsds415\_cn0()**](ccsds__core_8h.md#function-dp_ccsds415_cn0) ([**ccsds\_core.h**](ccsds__core_8h.md)) are the same two functions under the Python module's names, thin aliases that call these: one function each, not two.



```C++
#include <doppler/ccsds/ccsds_415.h>
#include <stdio.h>

dp_ccsds_415_scene_t s = dp_ccsds_415_scene (DP_CCSDS_415_FC_LO, 2.0, -13.75);
printf ("%.6f chips/s, ratio %.6f\n", s.chip_rate, s.ratio);
// 3214555.052790 chips/s, ratio 1.710968
```
 


    
## Public Functions Documentation




### function dp\_ccsds\_415\_chip\_rate 

_Chips a second at carrier_ `fc:` _F\_fwd x 31 / (221 x 96) (415.1-B-1 table 4-1). Fast math off: the same bits everywhere. Python: doppler.ccsds.ccsds415\_chip\_rate()._
```C++
double dp_ccsds_415_chip_rate (
    double fc
) 
```





**Parameters:**


* `fc` The carrier, F\_fwd, Hz. 



**Returns:**

The chip rate, chips/s.



```C++
printf ("%.6f\n", dp_ccsds_415_chip_rate (2.8e9));   // 4091251.885370
```
 


        

<hr>



### function dp\_ccsds\_415\_cn0 

_C/N0, dB-Hz, of an Es/N0 at the point's symbol rate. Python: doppler.ccsds.ccsds415\_cn0()._ 
```C++
double dp_ccsds_415_cn0 (
    double esn0_db
) 
```





**Parameters:**


* `esn0_db` Es/N0, dB. 



**Returns:**

`esn0_db + 10 log10(DP_CCSDS_415_SYM)`, dB-Hz.



```C++
printf ("%.4f\n", dp_ccsds_415_cn0 (DP_CCSDS_415_FLOOR_ESN0_DB)); // 38.7712
```
 


        

<hr>



### function dp\_ccsds\_415\_rx 

_Fills_ `a` _with the receiver's create arguments at the point, or refuses with the reason._
```C++
int dp_ccsds_415_rx (
    dp_ccsds_415_rx_t * a,
    size_t code_len,
    double carrier_freq_hz,
    double lost_confirm_s,
    bool refine,
    const char ** why
) 
```



The caller's choices are the code's length, the carrier, the lost confirm time and the refine stage; everything else is the point's ([**dp\_ccsds\_415\_rx\_t**](structdp__ccsds__415__rx__t.md) says which). Refused, each with its own sentence in `why:` a code that is not DP\_CCSDS\_415\_SF chips; a carrier that is not finite, below DP\_CCSDS\_415\_RX\_FC\_MIN or above DP\_CCSDS\_415\_RX\_FC\_MAX; a negative or NaN `lost_confirm_s`.




**Parameters:**


* `a` Filled on success; untouched on a refusal. 
* `code_len` The code's chips; the point's is 1,023. 
* `carrier_freq_hz` The carrier, F\_fwd, Hz. 
* `lost_confirm_s` Both flags down for longer than this enters lost; 0 never does. 
* `refine` The refine stage on or off. 
* `why` Set to the reason on a refusal; may be NULL. 



**Returns:**

0, or -1 with `why` set.



```C++
dp_ccsds_415_rx_t a;
const char       *why = NULL;
if (dp_ccsds_415_rx (&a, 1023, 2.0e9, 0.0, true, &why) != 0)
  printf ("%s\n", why);
// carrier_freq_hz is below DP_CCSDS_415_RX_FC_MIN (about 2.1004 GHz):
// its chip rate is under 1,023 x 3,000, so a symbol is shorter than a
// code period
```
 


        

<hr>



### function dp\_ccsds\_415\_scene 

_The scene's derived numbers at carrier_ `fc` _, computed once so a C program and its Python twin read the same bits._
```C++
dp_ccsds_415_scene_t dp_ccsds_415_scene (
    double fc,
    double spc,
    double start_hz
) 
```



`spc` and `start_hz` are the scene's choices, not the point's: the emitter's samples a chip and the Doppler it starts at. Fast math is off, so the result does not depend on the compiler. Its Python face is `doppler.ccsds.ccsds415_scene()`, which returns a `Ccsds415Scene` record ([**dp\_ccsds415\_scene()**](ccsds__core_8h.md#function-dp_ccsds415_scene) in `doppler/ccsds/ccsds_core.h`).




**Parameters:**


* `fc` The carrier, Hz. 
* `spc` The emitter's samples a chip, &gt; 0. 
* `start_hz` The Doppler at the scene's first sample, Hz. 



**Returns:**

The scene's numbers.



```C++
dp_ccsds_415_scene_t s = dp_ccsds_415_scene (2.2e9, 2.0, -13.75);
printf ("%.17g %.17g\n", s.start_ppm, s.rate_ppm_s);
// -0.0062499999999999995 0.0050000000000000001
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_CCSDS\_415\_CODE\_ONLY\_SYM 

```C++
#define DP_CCSDS_415_CODE_ONLY_SYM `275u`
```



Of a frame's symbols, the code-only ones that open it. 


        

<hr>



### define DP\_CCSDS\_415\_FC\_HI 

```C++
#define DP_CCSDS_415_FC_HI `2.8e9`
```



The band's high end, F\_fwd, Hz. 


        

<hr>



### define DP\_CCSDS\_415\_FC\_LO 

```C++
#define DP_CCSDS_415_FC_LO `2.2e9`
```



The band's low end, F\_fwd, Hz. 


        

<hr>



### define DP\_CCSDS\_415\_FLOOR\_ESN0\_DB 

```C++
#define DP_CCSDS_415_FLOOR_ESN0_DB `4.0`
```



The sensitivity floor, Es/N0 dB. 


        

<hr>



### define DP\_CCSDS\_415\_FRAME\_SYM 

```C++
#define DP_CCSDS_415_FRAME_SYM `1000u`
```



Symbols a frame. 


        

<hr>



### define DP\_CCSDS\_415\_FS 

```C++
#define DP_CCSDS_415_FS `11e6`
```



The capture rate, complex samples a second. 


        

<hr>



### define DP\_CCSDS\_415\_LOCK\_S 

```C++
#define DP_CCSDS_415_LOCK_S `20.0`
```



The lock allocation from arrival, s: 415.1-B-1 §7.4.1's acquisition time, and the owner's "typically allocated 20 seconds to lock". 


        

<hr>



### define DP\_CCSDS\_415\_RX\_FC\_MAX 

```C++
#define DP_CCSDS_415_RX_FC_MAX `( DP_CCSDS_415_FS * (221.0 * 96.0) / 31.0)`
```



The highest carrier the point builds a receiver at, Hz: where the chip rate reaches the capture rate, DP\_CCSDS\_415\_FS. Above it a chip is shorter than a sample. About 7.528 GHz. 


        

<hr>



### define DP\_CCSDS\_415\_RX\_FC\_MIN 

```C++
#define DP_CCSDS_415_RX_FC_MIN `((double) DP_CCSDS_415_SF * DP_CCSDS_415_SYM * (221.0 * 96.0) / 31.0)`
```



The lowest carrier the point builds a receiver at, Hz: where a symbol (1 / DP\_CCSDS\_415\_SYM s) is one code period (DP\_CCSDS\_415\_SF chips). Below it the chip rate is under SF x SYM, a symbol is shorter than a code period, and the receiver's carrier error, read over a code period free of symbol transitions, has no such period. About 2.1004 GHz. 


        

<hr>



### define DP\_CCSDS\_415\_R\_ENV\_HZ\_S 

```C++
#define DP_CCSDS_415_R_ENV_HZ_S `11.0`
```



R\_env, the residual Doppler's rate the receiver meets, Hz/s (async-dsss-receiver.md §1.8: the pass model's timing envelope times Earth's rotation factor, rounded up). The pass model is the test fixture's (dp\_s415\_r\_env(), native/tests/dp\_s415\_test.h), and a test holds the two equal, so this is derived, never fitted. 


        

<hr>



### define DP\_CCSDS\_415\_SF 

```C++
#define DP_CCSDS_415_SF `1023u`
```



Chips a code period. 


        

<hr>



### define DP\_CCSDS\_415\_SPAN\_HZ 

```C++
#define DP_CCSDS_415_SPAN_HZ `1500.0`
```



The Doppler uncertainty about best lock, one side, Hz. 


        

<hr>



### define DP\_CCSDS\_415\_SYM 

```C++
#define DP_CCSDS_415_SYM `3000.0`
```



Data symbols a second. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415.h`

