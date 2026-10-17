# Parallel Asynchronous-Data DSSS Receivers

M out of N ($\leq 10$) possible signals are simultaneously on air with at minimum differing code phase and carrier phase trajectory. Each signal may go off air and be replaced by a new signal at any moment. All signals share a common, periodic,
simultaneous multi-epoch data-free time block lasting many code epochs.

The carrier frequency, while bounded in range by $\approx T^{-1}_E$ (one inverse code epoch), may take on any value within said range.

$$\left|f_m\right| \leq \Delta f_c \approx R_s \approx 1/T_E$$

## Receiver operation

- The initial application is for continuously emitted CCSDS command link-like signals
    - $R_c=5$ Mcps
    - $N_E = R_c \times T_E = 1023$ chips per epoch
    - $E_s/N_0 \geq 5$ dB
- A slot is defined by a carrier offset $f_m$ and code phase with SNR the natural\
    tie-breaker should signals be otherwise indistinguishable.
- The "bookkeeper" tracks status of all receivers which are all always online
    either searching or tracking. Search resumes on signal loss or upon command
    tuned to a particular offset specified by bookkeeper, no frequency acquisition
    search is performed by the receivers except on command on the slow-time axis
    in the data-free region
- Each receiver makes it's current code phase and carrier estimate available
    for query
- Receivers are fed from a common DDC that removes any common carrier offset
    and resamples to twice the chip rate.
