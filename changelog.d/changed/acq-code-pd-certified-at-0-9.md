- **Acquisition's Pd model is certified for a DSSS code at the default
    `pd = 0.9`.** The code was measured at one design point, 0.65. It now
    runs at 0.3, 0.6 and 0.9, like every template, and the CTest spot check
    asserts the 0.9 row. The model holds at 0.9: 0.935 delivered against
    0.908 predicted. Its margin thins there, from +0.10 at 0.6 to +0.03.
