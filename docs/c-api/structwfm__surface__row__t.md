

# Struct wfm\_surface\_row\_t



[**ClassList**](annotated.md) **>** [**wfm\_surface\_row\_t**](structwfm__surface__row__t.md)



[More...](#detailed-description)

* `#include <wfm_surface.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const char \*const  \* | [**choices**](#variable-choices)  <br> |
|  const char \* | [**cli**](#variable-cli)  <br> |
|  int | [**has\_when**](#variable-has_when)  <br> |
|  size\_t | [**hi\_off**](#variable-hi_off)  <br> |
|  const char \* | [**json**](#variable-json)  <br> |
|  int | [**json\_bool**](#variable-json_bool)  <br> |
|  int | [**json\_omit**](#variable-json_omit)  <br> |
|  int | [**json\_required**](#variable-json_required)  <br> |
|  [**wfm\_sv\_kind\_t**](wfm__surface_8h.md#enum-wfm_sv_kind_t) | [**kind**](#variable-kind)  <br> |
|  size\_t | [**len\_off**](#variable-len_off)  <br> |
|  int | [**n\_choices**](#variable-n_choices)  <br> |
|  const char \* | [**name**](#variable-name)  <br> |
|  size\_t | [**off**](#variable-off)  <br> |
|  [**wfm\_surf\_owner\_t**](wfm__surface_8h.md#enum-wfm_surf_owner_t) | [**owner**](#variable-owner)  <br> |
|  unsigned | [**range\_bit**](#variable-range_bit)  <br> |
|  size\_t | [**reps\_off**](#variable-reps_off)  <br> |
|  int | [**unit\_interval**](#variable-unit_interval)  <br> |
|  int | [**when\_row**](#variable-when_row)  <br> |
|  int | [**when\_value**](#variable-when_value)  <br> |












































## Detailed Description


One field a face reads or writes by name: where it lives, what it holds, and how each face spells and bounds it. 


    
## Public Attributes Documentation




### variable choices 

```C++
const char* const* wfm_surface_row_t::choices;
```



WFM\_SV\_CHOICE: the names 


        

<hr>



### variable cli 

```C++
const char* wfm_surface_row_t::cli;
```



the flag; NULL = not on the CLI 


        

<hr>



### variable has\_when 

```C++
int wfm_surface_row_t::has_when;
```



written only when ... 


        

<hr>



### variable hi\_off 

```C++
size_t wfm_surface_row_t::hi_off;
```



`<name>_hi`, if ranged 


        

<hr>



### variable json 

```C++
const char* wfm_surface_row_t::json;
```



the scene/record key 


        

<hr>



### variable json\_bool 

```C++
int wfm_surface_row_t::json_bool;
```



written as true/false 


        

<hr>



### variable json\_omit 

```C++
int wfm_surface_row_t::json_omit;
```



not written at its default (or at 0) 


        

<hr>



### variable json\_required 

```C++
int wfm_surface_row_t::json_required;
```



absent/unknown: refuse 


        

<hr>



### variable kind 

```C++
wfm_sv_kind_t wfm_surface_row_t::kind;
```



the member's value kind 


        

<hr>



### variable len\_off 

```C++
size_t wfm_surface_row_t::len_off;
```



WFM\_SV\_SYMBOLS: the count 


        

<hr>



### variable n\_choices 

```C++
int wfm_surface_row_t::n_choices;
```



entries in `choices` 


        

<hr>



### variable name 

```C++
const char* wfm_surface_row_t::name;
```



the struct member 


        

<hr>



### variable off 

```C++
size_t wfm_surface_row_t::off;
```



offsetof the member 


        

<hr>



### variable owner 

```C++
wfm_surf_owner_t wfm_surface_row_t::owner;
```



which struct `off` is into 


        

<hr>



### variable range\_bit 

```C++
unsigned wfm_surface_row_t::range_bit;
```



WFM\_RANGE\_\*; 0 = not ranged 


        

<hr>



### variable reps\_off 

```C++
size_t wfm_surface_row_t::reps_off;
```



WFM\_SV\_FIELD: where \*REPS goes; 0 = the field does not repeat 


        

<hr>



### variable unit\_interval 

```C++
int wfm_surface_row_t::unit_interval;
```



require 0 &lt; v &lt;= 1 


        

<hr>



### variable when\_row 

```C++
int wfm_surface_row_t::when_row;
```



... this row's choice ... 


        

<hr>



### variable when\_value 

```C++
int wfm_surface_row_t::when_value;
```



... is this index 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_surface.h`

