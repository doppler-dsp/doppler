- **`docs/dev/issues.md` — the whole backlog, tiered by the kind of harm each
    issue does.** Six tiers, from *breaks for a user* down to *convergence and
    hygiene*, with a summary and status per issue. Most open issues carry no
    label at all, so this ordering **is** the triage rather than a view onto
    one that already existed.

    Generated, because a hand-maintained list of eighty-odd issues is the
    shape this repo has had to delete before: it rots in both directions at
    once, missing what was filed and still naming what was closed.
    `make issues` reconciles the committed tier map against the live issue
    list and **fails** on either — an open issue with no tier is untriaged and
    says so; a tier entry whose issue is closed is a stale row and says so.

    The judgement half lives in `docs/dev/issue-tiers.toml`, committed so a
    tier assignment is reviewed like code rather than living in someone's
    head. The rendering half is gated offline: `gen_issue_tracker.py --check`
    re-renders from that map and diffs the page, so a hand-edit is caught by
    `docs-drift-check` like every other generated region.

    `make issues` is deliberately **not** in CI and not in `docs-relink`. It
    is the only generator whose input is off the machine — it reads GitHub
    through `gh` — so a CI job built on it would fail on a rate limit rather
    than on the tree. Freshness cannot be checked offline, so the page states
    the date it was derived and the command that derives it, which is the
    date-plus-derivation this repo requires of any recorded live value.
