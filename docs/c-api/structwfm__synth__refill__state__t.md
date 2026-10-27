

# Struct wfm\_synth\_refill\_state\_t



[**ClassList**](annotated.md) **>** [**wfm\_synth\_refill\_state\_t**](structwfm__synth__refill__state__t.md)



_A frame source's own state triplet, over its_ `user` _pointer._[More...](#detailed-description)

* `#include <wfm_synth_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  void(\* | [**get\_state**](#variable-get_state)  <br> |
|  const char \*(\* | [**refusal**](#variable-refusal)  <br> |
|  int(\* | [**set\_state**](#variable-set_state)  <br> |
|  size\_t(\* | [**state\_bytes**](#variable-state_bytes)  <br> |












































## Detailed Description


What lets a synth that pulls its frames serialize: the synth carries the frame in play, and nests the source's position as this triplet's self-validating sub-blob ([**dp\_state.h**](dp__state_8h.md)). `state_bytes` returning 0 is a refusal, and `refusal` says why (a static sentence). A table, not four arguments, because one is shared by every source of a kind (static storage: the synth keeps the pointer). See [**dp\_wfm\_synth\_set\_refill\_state()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_refill_state). 


    
## Public Attributes Documentation




### variable get\_state 

```C++
void(* wfm_synth_refill_state_t::get_state) (const void *user, void *blob);
```



Write the source's position into `blob`, which holds exactly [**state\_bytes(user)**](structwfm__synth__refill__state__t.md#variable-state_bytes) bytes. 


        

<hr>



### variable refusal 

```C++
const char *(* wfm_synth_refill_state_t::refusal) (const void *user);
```



A static sentence saying why [**state\_bytes()**](structwfm__synth__refill__state__t.md#variable-state_bytes) returned 0; may be NULL (the synth then gives a generic reason). 


        

<hr>



### variable set\_state 

```C++
int(* wfm_synth_refill_state_t::set_state) (void *user, const void *blob);
```



Restore the source's position from `blob`; DP\_OK on success, any other value rejects the blob (the synth then returns DP\_ERR\_INVALID and keeps its frame). 


        

<hr>



### variable state\_bytes 

```C++
size_t(* wfm_synth_refill_state_t::state_bytes) (const void *user);
```



Bytes the source's position serializes to (envelope included), or 0 to refuse serialization. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm_synth/wfm_synth_core.h`

