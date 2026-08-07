

# Struct wfm\_keyword\_t



[**ClassList**](annotated.md) **>** [**wfm\_keyword\_t**](structwfm__keyword__t.md)



[More...](#detailed-description)

* `#include <wfm_keywords.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**count**](#variable-count)  <br> |
|  size\_t | [**elem\_size**](#variable-elem_size)  <br> |
|  char | [**tag**](#variable-tag)  <br> |
|  char | [**type**](#variable-type)  <br> |
|  uint8\_t \* | [**value**](#variable-value)  <br> |












































## Detailed Description


One decoded keyword. `value` is `count * elem_size` bytes in HOST order; for type `A` it is `count` characters and is NOT NUL-terminated. 


    
## Public Attributes Documentation




### variable count 

```C++
size_t wfm_keyword_t::count;
```



element count (characters for 'A'). 


        

<hr>



### variable elem\_size 

```C++
size_t wfm_keyword_t::elem_size;
```



bytes per element (1 for 'A'). 


        

<hr>



### variable tag 

```C++
char wfm_keyword_t::tag[WFM_KW_MAX_TAG+1];
```



NUL-terminated tag. 


        

<hr>



### variable type 

```C++
char wfm_keyword_t::type;
```



element type: B I L X F D A. 


        

<hr>



### variable value 

```C++
uint8_t* wfm_keyword_t::value;
```



value bytes, host order; owned by the holder. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_keywords.h`

