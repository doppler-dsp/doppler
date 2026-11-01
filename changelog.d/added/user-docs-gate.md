- **Every public class now owes its user story, and a gate holds it.**
    `make user-docs-check` (on `lint` and docs-check) requires an API entry,
    a guide mention and an example for each class, and a gallery figure or a
    written reason for each example. The 140 gaps that predate it sit in
    `docs/.user-docs-exempt`, which only shrinks. See
    [Adding an algorithm](docs/dev/contributing/adding-algorithms.md), phase 9.
