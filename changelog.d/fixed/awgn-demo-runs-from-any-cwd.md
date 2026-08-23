- **The runtime image's own documented first command failed:
    `docker run ... python awgn_demo.py` died on `FileNotFoundError`.**
    `awgn_demo.py` saved its figure to `docs/assets/awgn_demo.png`, a
    repo-relative prefix that exists nowhere but a doppler checkout, while
    all 60 sibling figure-writing examples emit a bare filename into the cwd
    and let `make gallery` move it — which is also why `awgn_demo.png` had
    never appeared in that target's `mv` list. Reported as #954, on the
    published 0.43.2 image.

    Two gates were watching and neither could see it, both now fixed.
    `test_examples.py` created `docs/assets/` inside its throwaway cwd before
    running each script, accommodating the one script that needed it rather
    than testing for it; the cwd is bare now, so an example that cannot run
    from an arbitrary directory fails. `scripts/smoke-image.sh runtime`
    checked `import doppler` and a version print — not what the image is for
    — so it stayed green through a release whose advertised command was
    broken; it now runs the demo, reading *which* demo out of the doc snippet
    the install page includes, so the smoke cannot check one command while
    the page advertises another.
