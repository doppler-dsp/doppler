#!/usr/bin/env python3
"""Every CI container image must trace back to ``.github/ci-images.env``.

Why this exists
---------------
The pinned digest used to live in **two** places that had to agree: the pin
file, and six literal ``container: image:`` refs in ``ci.yml``. Nothing could
move both — ``ci-image.yml`` writes the pin file, and the platform refuses any
push from ``GITHUB_TOKEN`` that touches ``.github/workflows/**`` — so the
nightly's repin branch was born failing lint and the repin path had never once
completed (doppler#1215).

Now the workflows name no digest at all. A ``pin`` job reads the pin file and
publishes the refs as job outputs, and every containerised job consumes
``${{ needs.pin.outputs.* }}``.

What this replaced, and why it could not stay
---------------------------------------------
The old scan walked ``image:`` lines and required each to be one of the two
pinned digests — but it **skipped** any ``${{ … }}`` expression, on the
reasoning that "what it resolves to is a matrix ``image:`` line, which this
same scan sees and checks as a literal". Removing the literals voids that
reasoning exactly: the scan would walk six expressions, check none of them,
and print OK. A gate that keeps passing because it stopped looking is the
failure this repo keeps rediscovering, so the expression is now *resolved*
rather than skipped.

The rules
---------
Each image reference must be one of:

* a **literal** equal to a digest in the pin file (still legal, still the
  thing that rejects a mutable tag — a tag is mutable by definition, so the
  image a PR passed on need not be the one it merges with);
* ``${{ matrix.<key> }}`` — resolved through that job's ``include`` entries,
  each result re-checked by these same rules;
* ``${{ needs.<job>.outputs.<name> }}`` where ``<job>`` is in this job's
  ``needs``, declares that output, and is a genuine pin job: some step's
  ``run:`` reads ``.github/ci-images.env`` and writes ``$GITHUB_OUTPUT``.

Anything else fails. Refusing to guess is deliberate and is inherited from
the scan this replaces: a check that cannot resolve an interpolation must say
so, not wave it through.

Finding **zero** references is also a failure. That is the specific way the
old scan would have died quietly, and a scan that matches nothing has not
passed — it has not run.

Usage: ci_image_refs_check.py [WORKFLOW_DIR] [PIN_FILE]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

#: ``${{ ctx.a.b }}`` with arbitrary inner spacing, and nothing else in the
#: string — a ref that concatenates an expression with text is not something
#: this gate will guess at.
EXPR = re.compile(r"^\$\{\{\s*(?P<body>[^}]+?)\s*\}\}$")
PIN_ENV = ".github/ci-images.env"


def pinned_digests(pin: Path) -> dict[str, str]:
    """Return ``{key: image_ref}`` for the ``CI_IMAGE_<n>`` rows."""
    out: dict[str, str] = {}
    for line in pin.read_text().splitlines():
        line = line.strip()
        if line.startswith("CI_IMAGE_") and "=" in line:
            k, v = line.split("=", 1)
            # Fingerprints and the source hash are not image refs.
            if k.removeprefix("CI_IMAGE_").isdigit():
                out[k] = v
    return out


def _steps_read_the_pin(job: dict) -> bool:
    """Does some step of this job read the pin file and emit an output?

    Deliberately behavioural rather than name-based: a job called ``pin`` that
    hardcodes a digest must not satisfy this, and one called anything else
    that genuinely reads the file must.
    """
    for step in job.get("steps") or []:
        run = step.get("run") or ""
        if PIN_ENV in run and "GITHUB_OUTPUT" in run:
            return True
    return False


class Checker:
    def __init__(self, wf: Path, pins: dict[str, str]) -> None:
        self.wf = wf
        self.pins = pins
        self.values = set(pins.values())
        self.errors: list[str] = []
        self.checked = 0

    def check_file(self, path: Path) -> None:
        try:
            doc = yaml.safe_load(path.read_text())
        except yaml.YAMLError as exc:  # pragma: no cover - malformed workflow
            self.errors.append(f"{path.name}: cannot parse: {exc}")
            return
        if not isinstance(doc, dict):
            return
        jobs = doc.get("jobs") or {}
        for name, job in jobs.items():
            if not isinstance(job, dict):
                continue
            for ref, where in self._refs(name, job):
                self.checked += 1
                self._check_ref(ref, f"{path.name}: {where}", job, jobs)

    def _refs(self, name: str, job: dict):
        """Yield every ``(image_ref, description)`` this job can run in."""
        container = job.get("container")
        if isinstance(container, str):
            yield container, f"jobs.{name}.container"
        elif isinstance(container, dict) and "image" in container:
            yield container["image"], f"jobs.{name}.container.image"
        for sname, svc in (job.get("services") or {}).items():
            if isinstance(svc, dict) and "image" in svc:
                yield svc["image"], f"jobs.{name}.services.{sname}.image"

    def _matrix_include(self, job: dict) -> list[dict]:
        strategy = job.get("strategy") or {}
        matrix = strategy.get("matrix") or {}
        inc = matrix.get("include") or []
        return [e for e in inc if isinstance(e, dict)]

    def _check_ref(
        self, ref: object, where: str, job: dict, jobs: dict
    ) -> None:
        if not isinstance(ref, str):
            self.errors.append(f"{where}: image is not a string")
            return

        m = EXPR.match(ref.strip())
        if m is None:
            if ref in self.values:
                return
            self.errors.append(
                f"{where}: names {ref!r}, which is not one of the digests in "
                f"{PIN_ENV}"
                + ("" if "@sha256:" in ref else " (and is a mutable tag)")
            )
            return

        body = m.group("body")
        parts = body.split(".")

        if parts[0] == "matrix" and len(parts) == 2:
            key = parts[1]
            entries = self._matrix_include(job)
            if not entries:
                self.errors.append(
                    f"{where}: resolves through matrix.{key}, but the job has "
                    "no matrix include entries to resolve it against"
                )
                return
            found = False
            for e in entries:
                if key in e:
                    found = True
                    self.checked += 1
                    self._check_ref(
                        e[key], f"{where} -> matrix.include[{key}]", job, jobs
                    )
            if not found:
                self.errors.append(
                    f"{where}: no matrix include entry defines {key!r}"
                )
            return

        if parts[0] == "needs" and len(parts) == 4 and parts[2] == "outputs":
            producer, out = parts[1], parts[3]
            declared = job.get("needs") or []
            if isinstance(declared, str):
                declared = [declared]
            if producer not in declared:
                self.errors.append(
                    f"{where}: reads needs.{producer}, which is not in this "
                    f"job's needs — it would resolve to empty"
                )
                return
            pj = jobs.get(producer)
            if not isinstance(pj, dict):
                self.errors.append(f"{where}: no job named {producer!r}")
                return
            if out not in (pj.get("outputs") or {}):
                self.errors.append(
                    f"{where}: job {producer!r} declares no output {out!r}"
                )
                return
            if not _steps_read_the_pin(pj):
                self.errors.append(
                    f"{where}: job {producer!r} never reads {PIN_ENV} — the "
                    "digest would come from somewhere this gate cannot see"
                )
            return

        self.errors.append(
            f"{where}: cannot resolve {ref!r}. This gate refuses to guess at "
            "an interpolation; use a pin-file digest, matrix.<key>, or "
            "needs.<job>.outputs.<name>."
        )


def main(argv: list[str]) -> int:
    wf = Path(argv[1] if len(argv) > 1 else ".github/workflows")
    pin = Path(argv[2] if len(argv) > 2 else PIN_ENV)

    if not pin.is_file():
        print(f"ci-image-refs-check: {pin} is missing")
        return 1
    pins = pinned_digests(pin)
    if not pins:
        print(f"ci-image-refs-check: {pin} names no CI_IMAGE_<n> rows")
        return 1

    c = Checker(wf, pins)
    for path in sorted(wf.glob("*.yml")) + sorted(wf.glob("*.yaml")):
        c.check_file(path)

    if c.checked == 0:
        print(
            "ci-image-refs-check: found NO image references in "
            f"{wf} — a scan that matches nothing has not passed, it has "
            "not run."
        )
        return 1

    if c.errors:
        print("ci-image-refs-check: FAIL")
        for e in c.errors:
            print(f"  {e}")
        print()
        print(f"  Every image must trace back to {PIN_ENV}, which is the one")
        print("  place the nightly rebuild can write. See doppler#1215.")
        return 1

    print(
        f"ci-image-refs-check: OK — {c.checked} image reference(s), all "
        f"resolving to {PIN_ENV}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
