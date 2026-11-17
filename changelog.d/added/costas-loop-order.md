- **`Costas` can switch a live loop to third order, to hold a ramping
    carrier with no steady phase lag.** `set_order(3, bn)` moves the loop
    onto a `StateFilter`, keeping its frequency estimate, and
    `norm_rate` seeds the carrier's rate. Order 2 stays the default and
    is unchanged, bit for bit. The FLL assist is refused at order 3 until
    it is measured there. The state blob carries the order (`COSTAS` v4).
    See [Costas, order 3](docs/guide/tracking-loops.md#order-3-a-carrier-that-ramps).
