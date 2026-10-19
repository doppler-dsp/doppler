- **`AsyncDsssReceiver` refuses a hand-over past half the sample rate.** A
    seed in range whose refined residual carried it to or past fs/2 used to
    track a Doppler the receiver cannot report; the hand-over now holds the
    refined value to the seed domain, counts it in `refused_hits`, and
    searches again.
