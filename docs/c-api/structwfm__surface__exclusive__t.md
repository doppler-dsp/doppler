

# Struct wfm\_surface\_exclusive\_t



[**ClassList**](annotated.md) **>** [**wfm\_surface\_exclusive\_t**](structwfm__surface__exclusive__t.md)



[More...](#detailed-description)

* `#include <wfm_surface.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**a**](#variable-a)  <br> |
|  int | [**b**](#variable-b)  <br> |
|  const char \* | [**cli\_why**](#variable-cli_why)  <br> |
|  const char \* | [**json\_why**](#variable-json_why)  <br> |
|  const char \* | [**obj\_why**](#variable-obj_why)  <br> |












































## Detailed Description


One pair of rows no face may take together. 


    
## Public Attributes Documentation




### variable a 

```C++
wfm_surface_exclusive_t::a;
```



First row of the pair: WFM\_SURFACE\_&lt;owner&gt;\_&lt;name&gt;. 


        

<hr>



### variable b 

```C++
int wfm_surface_exclusive_t::b;
```



second row: WFM\_SURFACE\_&lt;owner&gt;\_&lt;name&gt; 


        

<hr>



### variable cli\_why 

```C++
const char* wfm_surface_exclusive_t::cli_why;
```



CLI refusal; NULL: not both flags 


        

<hr>



### variable json\_why 

```C++
const char* wfm_surface_exclusive_t::json_why;
```



scene refusal; NULL: not both keys 


        

<hr>



### variable obj\_why 

```C++
const char* wfm_surface_exclusive_t::obj_why;
```



object refusal; NULL: not both set 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_surface.h`

