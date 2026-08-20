

# Struct node\_sync\_t



[**ClassList**](annotated.md) **>** [**node\_sync\_t**](structnode__sync__t.md)



_What one alignment hypothesis scored, and what the runner-up did._ [More...](#detailed-description)

* `#include <viterbi_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**errors**](#variable-errors)  <br> |
|  size\_t | [**margin**](#variable-margin)  <br> |
|  size\_t | [**next**](#variable-next)  <br> |
|  unsigned | [**phase**](#variable-phase)  <br> |
|  size\_t | [**symbols**](#variable-symbols)  <br> |












































## Detailed Description


`errors` against `symbols` IS the channel symbol error rate when the hypothesis is right, because in sync the decoder corrects the channel and the re-encoded stream differs from the received one exactly where the channel put an error.


Out of sync the decoder is searching a trellis its input does not lie on. The count then runs at a large fraction of the symbols — but **not at a half**, and the difference is worth stating because a half is what a coin-flip argument predicts and it is wrong: the decoder is a maximum LIKELIHOOD search, so it finds the codeword that agrees with the misaligned stream as well as any codeword can. Measured on a clean stream: **24 % of symbols for CCSDS K=7 r=1/2, 23 % for the same code uninverted, 18 % for a K=5 r=1/3** — against 0 % for the right alignment, which is the separation the decision actually rests on.


`margin` is what a caller acts on. The absolute count moves with Es/N0 and says nothing on its own; the DIFFERENCE between the best and the next best is the evidence that the search decided. 


    
## Public Attributes Documentation




### variable errors 

```C++
size_t node_sync_t::errors;
```



its disagreements 
 


        

<hr>



### variable margin 

```C++
size_t node_sync_t::margin;
```



`next - errors`; 0 when nothing separated 
 


        

<hr>



### variable next 

```C++
size_t node_sync_t::next;
```



the best competing hypothesis's 
 


        

<hr>



### variable phase 

```C++
unsigned node_sync_t::phase;
```



winning offset, `0 .. c->n-1` 
 


        

<hr>



### variable symbols 

```C++
size_t node_sync_t::symbols;
```



symbols SCORED per hypothesis, which is fewer than the window — see [**node\_sync\_scored\_symbols**](viterbi__core_8h.md#function-node_sync_scored_symbols) 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/viterbi/viterbi_core.h`

