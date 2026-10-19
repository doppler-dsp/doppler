- **`AsyncDsssReceiver(reacquire=True)` searches again by itself.** It
    releases on symbol lock alone, after `lost_confirm_s`, and resets to
    searching instead of entering `lost`, so an always-online receiver finds
    the next emitter with no `reset()`; the parallel pool's receivers run this
    way. A new `lifecycle` property names the state (`"searching"` ...
    `"lost"`) through a jm enum.
