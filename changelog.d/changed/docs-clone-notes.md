Pages that run `make build`, `./build/...` or `docker compose` now say how to get the repository (`git clone --branch v<version>`, re-stamped each release), and the C quick start names `app.c` and `CMakeLists.txt` as the files its commands build. A new lint gate, `clone-notes`, fails any page that runs a repository-only command without that clone (#2857).

The gate also recognises `python scripts/…`, `cmake -B build`, `bash tests/…`, `uv run` and `~~~` fences, and requires the clone to come before the first such command; page discovery and the exempt-file reader are now one module (`scripts/_docs_pages.py`) shared by the repo-link, broker-link and clone-note gates.

A `docs-snippet:` marker must now sit directly above the fence it governs (a test over every page): inserting a paragraph between them used to detach it silently, so the gates ran, or compiled, a fence they were told to skip.
