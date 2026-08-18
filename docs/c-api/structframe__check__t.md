

# Struct frame\_check\_t



[**ClassList**](annotated.md) **>** [**frame\_check\_t**](structframe__check__t.md)



_What_ [_**frame\_check**_](frame__core_8h.md#function-frame_check) _found, summed across the stages it reversed._[More...](#detailed-description)

* `#include <frame_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**checked**](#variable-checked)  <br> |
|  uint32\_t | [**corrected**](#variable-corrected)  <br> |
|  uint32\_t | [**ok**](#variable-ok)  <br> |
|  int | [**passed**](#variable-passed)  <br> |
|  uint32\_t | [**stages**](#variable-stages)  <br> |
|  uint32\_t | [**symbols**](#variable-symbols)  <br> |
|  uint32\_t | [**units**](#variable-units)  <br> |












































## Detailed Description


One record rather than one per stage, because a caller doing frame accounting wants a verdict and a cost. `units` and `ok` count CHECKS — one for a CRC, one per codeword for an interleaved outer code — so `ok == units` is the verdict and `symbols` is what it cost to get there.


`corrected` and `symbols` are the honest measure of how hard a link is running: `ok == units` with a rising `symbols` is margin being spent, and it is spent before it is lost. A CRC cannot report that at all, which is why an outer code is a strictly better detector and not merely a stronger one. 


    
## Public Attributes Documentation




### variable checked 

```C++
uint32_t frame_check_t::checked;
```



how many were reversed HERE (see below) 
 


        

<hr>



### variable corrected 

```C++
uint32_t frame_check_t::corrected;
```



how many needed and received repair 
 


        

<hr>



### variable ok 

```C++
uint32_t frame_check_t::ok;
```



how many came out good 
 


        

<hr>



### variable passed 

```C++
int frame_check_t::passed;
```



every check good: 1 yes, 0 no. `passed`, not `pass`: the obvious name is a Python keyword 
 


        

<hr>



### variable stages 

```C++
uint32_t frame_check_t::stages;
```



stages in the description 
 


        

<hr>



### variable symbols 

```C++
uint32_t frame_check_t::symbols;
```



symbol errors repaired 
 


        

<hr>



### variable units 

```C++
uint32_t frame_check_t::units;
```



checks performed across them 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame/frame_core.h`

