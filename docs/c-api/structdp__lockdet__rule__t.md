

# Struct dp\_lockdet\_rule\_t



[**ClassList**](annotated.md) **>** [**dp\_lockdet\_rule\_t**](structdp__lockdet__rule__t.md)



_A lock detector's RULE: its levels and verify counts, without its running count or decision._ [More...](#detailed-description)

* `#include <lockdet_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**down\_thresh**](#variable-down_thresh)  <br> |
|  uint32\_t | [**n\_down**](#variable-n_down)  <br> |
|  uint32\_t | [**n\_up**](#variable-n_up)  <br> |
|  double | [**up\_thresh**](#variable-up_thresh)  <br> |












































## Detailed Description


What a caller plotting a lock statistic needs beside it: the level the statistic must exceed to count a hit, the level it must fall under to count a miss, and how many consecutive of each move the decision. A statistic that crosses `up_thresh` fewer than `n_up` times in a row declares nothing; the decision probe shows where the rule actually fired, and this says why. 


    
## Public Attributes Documentation




### variable down\_thresh 

```C++
double dp_lockdet_rule_t::down_thresh;
```



miss when the metric is below this. 
 


        

<hr>



### variable n\_down 

```C++
uint32_t dp_lockdet_rule_t::n_down;
```



consecutive misses that drop. 
 


        

<hr>



### variable n\_up 

```C++
uint32_t dp_lockdet_rule_t::n_up;
```



consecutive hits that declare. 
 


        

<hr>



### variable up\_thresh 

```C++
double dp_lockdet_rule_t::up_thresh;
```



hit when the metric is above this. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/lockdet/lockdet_core.h`

