- **`make gen-c-api` no longer wipes `docs/c-api` before it has a render to
    put there.** The target built by deleting the committed tree first, so a
    build that failed in between -- a venv being re-synced under a commit
    hook, on 2026-09-02 -- left 594 pages and the hand-written `index.md`
    gone. It now renders beside the tree and swaps by rename; failure is
    not destructive, and `test_gen_c_api_atomic.py` holds it to that.
