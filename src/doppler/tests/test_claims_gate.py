"""``scripts/check_claims.py``, proven by sabotage on a scratch tree.

Each test builds a minimal ``native/tests`` tree, shows the gate green on it,
then breaks ONE thing and requires the gate to go red naming it: a claim
without a control, a control outside a claim, a repeated claim id, a new
unadopted test, a stale line in each of the two lists.
"""

from __future__ import annotations

import sys

from doppler.tests._repo import repo_root

sys.path.insert(0, str(repo_root() / "scripts"))
from typing import TYPE_CHECKING

import check_claims

if TYPE_CHECKING:
    from pathlib import Path

GOOD = """\
#include "dp_test.h"
int main (void)
{
  DP_CLAIM ("a", "a holds");
  DP_CHECK (1);
  DP_CONTROL ("a wrong case", 0);
  DP_TEST_END ("t");
}
"""


def tree(tmp_path: Path, files: dict[str, str], unadopted="", no_control=""):
    (tmp_path / "native" / "tests").mkdir(parents=True)
    (tmp_path / "scripts").mkdir()
    for name, text in files.items():
        (tmp_path / "native" / "tests" / name).write_text(text, "utf-8")
    (tmp_path / "scripts" / ".claims-unadopted").write_text(unadopted, "utf-8")
    (tmp_path / "scripts" / ".claims-no-control").write_text(
        no_control, "utf-8"
    )
    return tmp_path


def findings(root: Path) -> list[str]:
    return check_claims.check(root)[0]


def test_a_claim_with_a_control_is_green(tmp_path):
    assert findings(tree(tmp_path, {"test_x.c": GOOD})) == []


def test_a_claim_without_a_control_is_red(tmp_path):
    text = GOOD.replace('  DP_CONTROL ("a wrong case", 0);\n', "")
    (msg,) = findings(tree(tmp_path, {"test_x.c": text}))
    assert "claim 'a' has no DP_CONTROL" in msg


def test_a_refusal_pair_counts_as_the_control(tmp_path):
    text = GOOD.replace(
        'DP_CONTROL ("a wrong case", 0);', 'DP_REFUSAL (1, 1, "made good");'
    )
    assert findings(tree(tmp_path, {"test_x.c": text})) == []


def test_a_macro_named_in_a_comment_is_not_a_control(tmp_path):
    text = GOOD.replace(
        'DP_CONTROL ("a wrong case", 0);', "/* DP_CONTROL (nothing) */"
    )
    (msg,) = findings(tree(tmp_path, {"test_x.c": text}))
    assert "has no DP_CONTROL" in msg


def test_a_control_before_any_claim_is_red(tmp_path):
    text = GOOD.replace("  DP_CLAIM", '  DP_CONTROL ("stray", 0);\n  DP_CLAIM')
    msgs = findings(tree(tmp_path, {"test_x.c": text}))
    assert any("before any DP_CLAIM" in m for m in msgs)


def test_a_repeated_claim_id_is_red(tmp_path):
    text = GOOD.replace(
        "  DP_TEST_END",
        '  DP_CLAIM ("a", "again");\n  DP_CHECK (1);\n'
        '  DP_CONTROL ("w", 0);\n  DP_TEST_END',
    )
    msgs = findings(tree(tmp_path, {"test_x.c": text}))
    assert any("used twice" in m for m in msgs)


def test_a_new_test_that_opens_no_claim_is_red(tmp_path):
    old = 'int main (void) { DP_CHECK (1); DP_TEST_END ("o"); }\n'
    root = tree(
        tmp_path,
        {"test_old.c": old, "test_new.c": old},
        unadopted="native/tests/test_old.c  # not adopted yet\n",
    )
    (msg,) = findings(root)
    assert "test_new.c" in msg and "shrink-only" in msg


def test_an_unadopted_line_that_now_has_claims_is_red(tmp_path):
    root = tree(
        tmp_path,
        {"test_x.c": GOOD},
        unadopted="native/tests/test_x.c  # was not adopted\n",
    )
    (msg,) = findings(root)
    assert "now opens claims; delete the line" in msg


def test_an_unadopted_line_naming_no_file_is_red(tmp_path):
    root = tree(
        tmp_path,
        {"test_x.c": GOOD},
        unadopted="native/tests/gone.c  # deleted\n",
    )
    (msg,) = findings(root)
    assert "names no file" in msg


def test_a_no_control_entry_needs_a_reason_and_a_claim_that_needs_it(tmp_path):
    text = GOOD.replace('  DP_CONTROL ("a wrong case", 0);\n', "")
    ok = tree(
        tmp_path / "ok",
        {"test_x.c": text},
        no_control="native/tests/test_x.c:a  # the control is in a helper\n",
    )
    assert findings(ok) == []
    bare = tree(
        tmp_path / "bare",
        {"test_x.c": text},
        no_control="native/tests/test_x.c:a\n",
    )
    assert any("gives no reason" in m for m in findings(bare))
    stale = tree(
        tmp_path / "stale",
        {"test_x.c": GOOD},
        no_control="native/tests/test_x.c:a  # no longer needed\n",
    )
    assert any("now has a control" in m for m in findings(stale))


def test_a_file_that_breaks_the_rules_on_purpose_says_so(tmp_path):
    text = "/* claims-gate: exempt (a negative test) */\n" + GOOD.replace(
        '  DP_CONTROL ("a wrong case", 0);\n', ""
    )
    assert findings(tree(tmp_path, {"test_x.c": text})) == []
