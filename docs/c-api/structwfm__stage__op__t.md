

# Struct wfm\_stage\_op\_t



[**ClassList**](annotated.md) **>** [**wfm\_stage\_op\_t**](structwfm__stage__op__t.md)



_How one kind of stage actually transforms bits._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t(\* | [**emit**](#variable-emit)  <br> |
|  int(\* | [**in\_unit**](#variable-in_unit)  <br> |
|  uint32\_t | [**kind**](#variable-kind)  <br> |
|  int(\* | [**undo**](#variable-undo)  <br> |












































## Detailed Description


The description is pure data and names a stage by [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t); this is where the arithmetic for that kind comes from. The split is a LAYERING requirement, not a taste: `ccsds_tm` must depend on this file to describe a CADU, so this file must not call `ccsds_tm`'s kernels, or the two components form a cycle. The kernels arrive as a table instead, from whichever component owns them.


It is also what makes the description open. A caller with a stage doppler has never heard of supplies its own entry rather than waiting for an enum to grow.


Exactly one of the two is set. `in_unit` rewrites the stage's span where it lies; `emit` consumes the assembled frame and produces a different stream.


**A stage's derived field is the LAST field of its cover**, which is what lets one in-place signature serve a CRC, an outer code and a randomiser alike: the op receives the whole span, reads the information at its head and writes the check symbols into its tail. [**wfm\_frame\_desc\_layout**](wfm__frame_8h.md#function-wfm_frame_desc_layout) refuses a description that breaks it. 


    
## Public Attributes Documentation




### variable emit 

```C++
size_t(* wfm_stage_op_t::emit) (const wfm_stage_t *st, const uint8_t *in, size_t n, uint8_t *out, size_t max_out, void *user);
```



Consume `n` bits at `in` and write the new stream to `out`. Returns the bits written, or 0 on refusal. `out` may overlap `in:` the frame is assembled in the TAIL of the caller's buffer and the stream is written from its head, so an implementation must read each input bit before writing the output bits that displace it — which is the order any expanding code writes in anyway. 


        

<hr>



### variable in\_unit 

```C++
int(* wfm_stage_op_t::in_unit) (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user);
```



Rewrite `n` bits at `bits`, in place. Returns 0 on success. 


        

<hr>



### variable kind 

```C++
uint32_t wfm_stage_op_t::kind;
```



The kind this entry implements; matched against [**wfm\_stage\_t**](structwfm__stage__t.md)'s. 


        

<hr>



### variable undo 

```C++
int(* wfm_stage_op_t::undo) (const wfm_stage_t *st, uint8_t *bits, size_t n, wfm_frame_stage_rx_t *rx, void *user);
```



Undo the stage over its span on the RECEIVE side, correcting `bits` in place and reporting what was found. Returns 0 on success, -1 if the span is the wrong shape for this stage.


A stage with no `undo` is not an error — it is a stage the receiver does not reverse HERE. The inner code is the case: it is streaming and emits its decisions `depth` bits late, so it is undone before frame synchronisation and a frame checker never sees channel symbols. [**wfm\_frame\_check**](wfm__frame_8h.md#function-wfm_frame_check) reports such a stage as not-checked rather than as passed, which are different answers. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

