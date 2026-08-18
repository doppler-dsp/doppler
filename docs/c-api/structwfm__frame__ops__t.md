

# Struct wfm\_frame\_ops\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md)



_The kernels an assembly runs, and whatever state they carry._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**n\_op**](#variable-n_op)  <br> |
|  const [**wfm\_stage\_op\_t**](structwfm__stage__op__t.md) \* | [**op**](#variable-op)  <br> |
|  void \* | [**user**](#variable-user)  <br> |












































## Detailed Description


Looked up by kind, and EXTENDS the built-ins rather than replacing them — so a table supplying an outer code does not have to restate the CRC. A stage whose kind is in neither table is a **refusal**, never a silent skip: a stage that quietly did not run produces a frame that still assembles, still decodes against itself, and syncs to nothing. 


    
## Public Attributes Documentation




### variable n\_op 

```C++
unsigned wfm_frame_ops_t::n_op;
```



entries in `op` 
 


        

<hr>



### variable op 

```C++
const wfm_stage_op_t* wfm_frame_ops_t::op;
```



table, looked up by kind 
 


        

<hr>



### variable user 

```C++
void* wfm_frame_ops_t::user;
```



handed to every op it calls 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

