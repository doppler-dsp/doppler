- **An unbuildable DSSS burst is refused at create, not degraded to a silent
    gap.** Frame bits with no spreading code, or an outer code whose payload is
    not 223\*depth octets, used to compose successfully and emit nothing — on
    the CLI, a zero-length capture with exit 0. Both now name the problem and
    fail: the stage rules that already guarded an unspread frame were never
    reached on the spread path.
