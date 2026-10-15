- **An acquisition engine with a coherent depth above 1 now false-alarms at
    or under its `pfa`, not twice it (#2218).** The gate is priced over the
    Doppler-interpolated surface its maximum searches, a never-exceed bound
    that costs about 0.25 dB at a given `pd`. So an auto-sized engine may
    pick a deeper D; if it now reports `underpowered`, give it more SNR, a
    lower `pd` or a higher `pfa`.
