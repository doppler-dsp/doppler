

# Struct acc\_trace\_state\_t



[**ClassList**](annotated.md) **>** [**acc\_trace\_state\_t**](structacc__trace__state__t.md)



_AccTrace state. Allocate with_ [_**acc\_trace\_create()**_](acc__trace__core_8h.md#function-acc_trace_create) _._

* `#include <acc_trace_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double \* | [**acc**](#variable-acc)  <br> |
|  double | [**alpha**](#variable-alpha)  <br> |
|  uint64\_t | [**count**](#variable-count)  <br> |
|  [**acc\_trace\_mode\_t**](acc__trace__core_8h.md#enum-acc_trace_mode_t) | [**mode**](#variable-mode)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |












































## Public Attributes Documentation




### variable acc 

```C++
double* acc_trace_state_t::acc;
```



Running trace, length n (double). 


        

<hr>



### variable alpha 

```C++
double acc_trace_state_t::alpha;
```



EMA smoothing factor (exp mode). 


        

<hr>



### variable count 

```C++
uint64_t acc_trace_state_t::count;
```



Frames folded in so far. 


        

<hr>



### variable mode 

```C++
acc_trace_mode_t acc_trace_state_t::mode;
```



Reduction mode. 


        

<hr>



### variable n 

```C++
size_t acc_trace_state_t::n;
```



Trace length (bins). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_trace/acc_trace_core.h`

