

# Struct dll\_state\_t



[**ClassList**](annotated.md) **>** [**dll\_state\_t**](structdll__state__t.md)



_DLL state._ [More...](#detailed-description)

* `#include <dll_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**acc\_e**](#variable-acc_e)  <br> |
|  float complex | [**acc\_l**](#variable-acc_l)  <br> |
|  float complex | [**acc\_o**](#variable-acc_o)  <br> |
|  float complex | [**acc\_p**](#variable-acc_p)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**chip\_pos**](#variable-chip_pos)  <br> |
|  const uint8\_t \* | [**code**](#variable-code)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  double | [**inv\_sps**](#variable-inv_sps)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  double | [**lock\_alpha**](#variable-lock_alpha)  <br> |
|  size\_t | [**lock\_count**](#variable-lock_count)  <br> |
|  size\_t | [**lock\_nz**](#variable-lock_nz)  <br> |
|  double | [**lock\_stat**](#variable-lock_stat)  <br> |
|  double | [**lock\_sum**](#variable-lock_sum)  <br> |
|  double | [**lock\_thresh**](#variable-lock_thresh)  <br> |
|  int | [**locked**](#variable-locked)  <br> |
|  size\_t | [**n\_looks**](#variable-n_looks)  <br> |
|  double | [**noise\_ema**](#variable-noise_ema)  <br> |
|  double | [**noise\_guard**](#variable-noise_guard)  <br> |
|  double | [**off\_chips**](#variable-off_chips)  <br> |
|  int | [**owns\_code**](#variable-owns_code)  <br> |
|  uint32\_t | [**rng**](#variable-rng)  <br> |
|  double | [**seed\_chip**](#variable-seed_chip)  <br> |
|  double | [**seg\_chips**](#variable-seg_chips)  <br> |
|  size\_t | [**seg\_idx**](#variable-seg_idx)  <br> |
|  double | [**seg\_norm**](#variable-seg_norm)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  double | [**spacing**](#variable-spacing)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  double | [**sum\_e**](#variable-sum_e)  <br> |
|  double | [**sum\_l**](#variable-sum_l)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**dll\_create()**](dll__core_8h.md#function-dll_create) (copies the code), or embed by value and [**dll\_init()**](dll__core_8h.md#function-dll_init) (borrows the caller's code). The loop filter `lf` is a public sub-component so the inline composition helpers can drive it; treat the correlator accumulators and code-phase fields as internal. 


    
## Public Attributes Documentation




### variable acc\_e 

```C++
float complex dll_state_t::acc_e;
```



early correlator accumulator. 


        

<hr>



### variable acc\_l 

```C++
float complex dll_state_t::acc_l;
```



late correlator accumulator. 


        

<hr>



### variable acc\_o 

```C++
float complex dll_state_t::acc_o;
```



offset (noise) correlator accumulator. 


        

<hr>



### variable acc\_p 

```C++
float complex dll_state_t::acc_p;
```



prompt correlator accumulator. 


        

<hr>



### variable bn 

```C++
double dll_state_t::bn;
```



loop noise bandwidth (retained). 


        

<hr>



### variable chip\_pos 

```C++
double dll_state_t::chip_pos;
```



current prompt code phase, chips. 


        

<hr>



### variable code 

```C++
const uint8_t* dll_state_t::code;
```



spreading code, one period (0/1 chips). 


        

<hr>



### variable code\_rate 

```C++
double dll_state_t::code_rate;
```



chips advanced per nominal chip (~1.0). 


        

<hr>



### variable inv\_sps 

```C++
double dll_state_t::inv_sps;
```



1 / sps (per-sample chip advance scale). 


        

<hr>



### variable last\_error 

```C++
double dll_state_t::last_error;
```



last discriminator output (loop stress). 


        

<hr>



### variable lf 

```C++
loop_filter_state_t dll_state_t::lf;
```



2nd-order code PI loop. 


        

<hr>



### variable lock\_alpha 

```C++
double dll_state_t::lock_alpha;
```



EMA coefficient 1/L\_eff (L\_eff &gt;&gt; n\_looks). 


        

<hr>



### variable lock\_count 

```C++
size_t dll_state_t::lock_count;
```



looks accumulated in the current window. 


        

<hr>



### variable lock\_nz 

```C++
size_t dll_state_t::lock_nz;
```



noise looks folded in (cumulative-mean boot). 


        

<hr>



### variable lock\_stat 

```C++
double dll_state_t::lock_stat;
```



last statistic R = sqrt(2 sum\|P\|^2/E\|O\|^2). 


        

<hr>



### variable lock\_sum 

```C++
double dll_state_t::lock_sum;
```



running sum\|P\_k\|^2 over the current window. 


        

<hr>



### variable lock\_thresh 

```C++
double dll_state_t::lock_thresh;
```



CFAR threshold eta on R (det\_threshold\_nc). 


        

<hr>



### variable locked 

```C++
int dll_state_t::locked;
```



last lock decision (R &gt; eta). 


        

<hr>



### variable n\_looks 

```C++
size_t dll_state_t::n_looks;
```



non-coherent integration depth N. 


        

<hr>



### variable noise\_ema 

```C++
double dll_state_t::noise_ema;
```



EMA of offset power; estimates E\|O\|^2. 


        

<hr>



### variable noise\_guard 

```C++
double dll_state_t::noise_guard;
```



chips around P/E/L the offset must avoid. 


        

<hr>



### variable off\_chips 

```C++
double dll_state_t::off_chips;
```



this look's offset code phase, whole chips. 


        

<hr>



### variable owns\_code 

```C++
int dll_state_t::owns_code;
```



1 if [**dll\_destroy()**](dll__core_8h.md#function-dll_destroy) frees `code`. 


        

<hr>



### variable rng 

```C++
uint32_t dll_state_t::rng;
```



xorshift32 state for the random offset. 


        

<hr>



### variable seed\_chip 

```C++
double dll_state_t::seed_chip;
```



create-time code phase, for reset. 


        

<hr>



### variable seg\_chips 

```C++
double dll_state_t::seg_chips;
```



code phase per partial segment = sf/segments. 


        

<hr>



### variable seg\_idx 

```C++
size_t dll_state_t::seg_idx;
```



current partial index within the epoch. 


        

<hr>



### variable seg\_norm 

```C++
double dll_state_t::seg_norm;
```



nominal samples per segment (prompt scale). 


        

<hr>



### variable segments 

```C++
size_t dll_state_t::segments;
```



partial correlations per epoch (1 = full). 


        

<hr>



### variable sf 

```C++
size_t dll_state_t::sf;
```



code length (chips per period). 


        

<hr>



### variable spacing 

```C++
double dll_state_t::spacing;
```



early/late tap offset, chips (e.g. 0.5). 


        

<hr>



### variable sps 

```C++
size_t dll_state_t::sps;
```



samples per chip. 


        

<hr>



### variable sum\_e 

```C++
double dll_state_t::sum_e;
```



non-coherent early sum over the epoch. 


        

<hr>



### variable sum\_l 

```C++
double dll_state_t::sum_l;
```



non-coherent late sum over the epoch. 


        

<hr>



### variable zeta 

```C++
double dll_state_t::zeta;
```



damping factor (retained). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

