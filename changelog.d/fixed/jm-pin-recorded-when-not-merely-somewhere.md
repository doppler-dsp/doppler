- **`gen_jm_pin --check` now asks whether the pin move was announced by THIS
    change, so a rollback can no longer ship silently.** The check asserted
    that the pinned jm version appears somewhere in `CHANGELOG.md` on a line
    naming the pin. A **rollback** — "0.63.3 regressed us, go back to 0.55.3" —
    moves all three pin sites to a version the file already describes
    *arriving at*, so the scan found it and said nothing. Applied consistently
    is not the same as announced, which is the exact failure the assertion was
    written to close, one level in.

    It now compares the pin against the **merge base** (the same question and
    baseline the assertion ratchet uses: did *this branch* move it). When it
    moved, the new version must appear as a pin **destination** among the
    changelog lines the branch adds. History does not count, because history
    is what a rollback returns to.

    Two narrower scopes were tried first and both let the rollback through, for
    one reason: `[Unreleased]` is 198 KB here — doppler has not released since
    v0.42.0 — and holds every pin bump since 0.52.0, and `changelog.d/` is 109
    unassembled fragments. Each is this release *cycle's* history rather than
    this *branch's* statement.

    An untracked fragment is read as fully added, because writing a fresh
    `changelog.d/` file is the ordinary way to record a bump and `git diff`
    cannot see it — without that the gate would reject the workflow it asks
    for, which is how a gate gets switched off.

- **The same check no longer counts a version it is moving AWAY from.**
    Harvesting every semver on a pin line made 17 of 18 "recorded" versions
    left-hand sides of an `X → Y` entry.

    [#693](https://github.com/doppler-dsp/doppler/issues/693) proposed taking
    the last semver on the line; this file's own history refutes that twice.
    `pin 0.57.0 → 0.59.0, and the create-only headers 0.58.0 ships` ends on a
    version that is neither side of the move, and `pinned to 0.25.0 (from   0.24.0)` puts the destination **first**. The destination is now read from
    what the sentence does — an `X → Y` pair, else a bare `→ Y` / `to Y`, else
    the first semver with an explicit `(from X)` removed — which is right for
    all 22 pin lines in the tree.

    Proven by sabotage in both directions: an unannounced forward bump and an
    unannounced rollback each go red, and each passes once a fragment names
    the move. An unmoved pin stays green.
