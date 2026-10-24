- **just-makeit pin 0.109.0 → 0.110.0: `Writer.annotate(None)` raises
    `TypeError`.** The scene is declared `required = true` (jm gh-2360), so
    the binding refuses `None` with a message naming `scene`. Before, `None`
    reached C as NULL and came back as `ValueError`. The stub now types it
    `scene: object`, not `object | None`. 0.109.1's comment-only fix to the
    capsule-lender teardown comments comes with it.
