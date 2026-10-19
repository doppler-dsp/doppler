- **`AsyncDsssReceiver(reacquire=True)` searches again by itself.** It
    releases on symbol lock alone, after `lost_confirm_s`, and resets to
    searching instead of entering `lost`, so an always-online receiver finds
    the next emitter with no `reset()` -- what the parallel pool's receivers
    will need. A new `lifecycle` property names the state (`"searching"` ...
    `"lost"`) through a jm enum.
