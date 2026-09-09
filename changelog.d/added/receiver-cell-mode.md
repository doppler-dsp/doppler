- **`CellAsyncDsssReceiver`** (C: `async_dsss_receiver_create_cell`, part of
    #1283): the receiver a searcher's cell drives — design §12.22–12.24 as a
    mode of `AsyncDsssReceiver`, by turning stages off: no refine, the `Dll`
    held and steered by rate to a held phase corrected once an interval by a
    gain times its interval-mean discriminator. On the searcher's own stream
    it holds 0.0056 / 0.0082 chip at 45 / 40 dB-Hz, 1.9× / 2.7× under the
    hand-off flavour's closed `Dll` on the same seed, at its BER at 45 dB-Hz
    (design §12.26; the 40 dB-Hz carrier cycle slips both share are #1289).
