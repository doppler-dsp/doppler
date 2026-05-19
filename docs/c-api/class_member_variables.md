
# Class Member Variables



## a

* **acc** ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md), [**acc\_f32\_state\_t**](structacc__f32__state__t.md))
* **alpha** ([**agc\_state\_t**](structagc__state__t.md))
* **amplitude\_db** ([**dp\_peak\_t**](structdp__peak__t.md))


## b

* **buf** ([**delay\_state\_t**](structdelay__state__t.md))
* **bank** ([**resamp\_state\_t**](structresamp__state__t.md))


## c

* **clip\_db** ([**agc\_state\_t**](structagc__state__t.md))
* **capacity** ([**delay\_state\_t**](structdelay__state__t.md))
* **center\_freq** ([**dp\_header\_t**](structdp__header__t.md))
* **centre** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **ctrl\_acc** ([**resamp\_state\_t**](structresamp__state__t.md))


## d

* **decim** ([**agc\_state\_t**](structagc__state__t.md))
* **delay** ([**fir\_state\_t**](structfir__state__t.md))
* **decim\_iad** ([**resamp\_state\_t**](structresamp__state__t.md))
* **decim\_tfd** ([**resamp\_state\_t**](structresamp__state__t.md))
* **delay\_buf** ([**resamp\_state\_t**](structresamp__state__t.md))
* **delay\_cap** ([**resamp\_state\_t**](structresamp__state__t.md))
* **delay\_head** ([**resamp\_state\_t**](structresamp__state__t.md))
* **delay\_mask** ([**resamp\_state\_t**](structresamp__state__t.md))


## e

* **even\_buf** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **even\_cap** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **even\_head** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **even\_mask** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))


## f

* **flags** ([**dp\_header\_t**](structdp__header__t.md))
* **freq\_norm** ([**dp\_peak\_t**](structdp__peak__t.md))
* **fir\_on\_even** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))


## g

* **g\_last** ([**agc\_state\_t**](structagc__state__t.md))
* **gain\_db** ([**agc\_state\_t**](structagc__state__t.md))


## h

* **head** ([**delay\_state\_t**](structdelay__state__t.md))
* **h** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **has\_pending** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))


## l

* **loop\_bw** ([**agc\_state\_t**](structagc__state__t.md))
* **log2\_phases** ([**resamp\_state\_t**](structresamp__state__t.md))


## m

* **mask** ([**delay\_state\_t**](structdelay__state__t.md))
* **magic** ([**dp\_header\_t**](structdp__header__t.md))


## n

* **num\_taps** ([**delay\_state\_t**](structdelay__state__t.md), [**fir\_state\_t**](structfir__state__t.md), [**hbdecim\_state\_t**](structhbdecim__state__t.md), [**resamp\_state\_t**](structresamp__state__t.md))
* **num\_samples** ([**dp\_header\_t**](structdp__header__t.md))
* **nx** ([**fft2d\_state\_t**](structfft2d__state__t.md))
* **ny** ([**fft2d\_state\_t**](structfft2d__state__t.md))
* **n** ([**fft\_state\_t**](structfft__state__t.md))
* **norm\_freq** ([**lo\_state\_t**](structlo__state__t.md), [**nco\_state\_t**](structnco__state__t.md))
* **nmax** ([**nco\_state\_t**](structnco__state__t.md))
* **num\_phases** ([**resamp\_state\_t**](structresamp__state__t.md))


## o

* **odd\_buf** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **odd\_head** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))


## p

* **p\_avg** ([**agc\_state\_t**](structagc__state__t.md))
* **protocol** ([**dp\_header\_t**](structdp__header__t.md))
* **plan\_f32** ([**fft2d\_state\_t**](structfft2d__state__t.md), [**fft\_state\_t**](structfft__state__t.md))
* **plan\_f64** ([**fft2d\_state\_t**](structfft2d__state__t.md), [**fft\_state\_t**](structfft__state__t.md))
* **pending** ([**hbdecim\_state\_t**](structhbdecim__state__t.md))
* **phase** ([**lo\_state\_t**](structlo__state__t.md), [**nco\_state\_t**](structnco__state__t.md), [**resamp\_state\_t**](structresamp__state__t.md))
* **phase\_inc** ([**lo\_state\_t**](structlo__state__t.md), [**nco\_state\_t**](structnco__state__t.md), [**resamp\_state\_t**](structresamp__state__t.md))


## r

* **ref\_db** ([**agc\_state\_t**](structagc__state__t.md))
* **reserved** ([**dp\_header\_t**](structdp__header__t.md))
* **rtaps** ([**fir\_state\_t**](structfir__state__t.md))
* **rate** ([**resamp\_state\_t**](structresamp__state__t.md))


## s

* **sample\_rate** ([**dp\_header\_t**](structdp__header__t.md))
* **sample\_type** ([**dp\_header\_t**](structdp__header__t.md))
* **sequence** ([**dp\_header\_t**](structdp__header__t.md))
* **stream\_id** ([**dp\_header\_t**](structdp__header__t.md))
* **sign** ([**fft2d\_state\_t**](structfft2d__state__t.md), [**fft\_state\_t**](structfft__state__t.md))
* **scratch** ([**fir\_state\_t**](structfir__state__t.md))
* **scratch\_cap** ([**fir\_state\_t**](structfir__state__t.md))


## t

* **timestamp\_ns** ([**dp\_header\_t**](structdp__header__t.md))
* **taps** ([**fir\_state\_t**](structfir__state__t.md))


## u

* **upsample** ([**resamp\_state\_t**](structresamp__state__t.md))


## v

* **version** ([**dp\_header\_t**](structdp__header__t.md))




