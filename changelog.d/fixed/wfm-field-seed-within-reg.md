- **A Field SEED, POLY or Gold tap wider than REG is refused.** The
    generators masked it to the register, so `pn:31:5:32` rendered 31 zeros
    and `pn:10:5:0:0x40` a register with no feedback. It is refused where
    the text is read, naming REG (#1624).
