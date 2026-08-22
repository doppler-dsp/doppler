**just-makeit pin 0.63.3 → 0.64.0 is BLOCKED, not deferred.**
0.64.0's gh-1085 zero-bound refusal makes `Telemetry.read()` and
`MemoryCapture.records()` raise `RuntimeError` on an empty ring — the steady
state of a non-blocking drain, and an empty array on 0.63.3. Both declare
`pass_capacity = true`, so the kernel receives the capacity and cannot
overrun; a zero bound is a documented value there, not an absence. Filed as
just-makeit#1091. The bump lands once that ships.
