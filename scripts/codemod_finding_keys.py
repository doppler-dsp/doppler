#!/usr/bin/env python3
"""Run once: key every validation finding and cite it by key (#2059).

**A one-shot migration, committed so it can be reviewed and re-run, not
a tool to keep using.** After it ran, `Report.find` takes a key and
refuses a typed number, so running it again finds nothing to do.

Before #2059 a report recorded a finding as ``R.find("F3", ...)`` and
cited it by typing ``F3`` into prose. The number is assigned by position,
so dropping a finding re-pointed every citation after it: #2056 left five
pointing at the wrong finding. This script rewrites every ``validate.py``:

- ``R.find("F3", verdict, text)`` becomes ``R.find("<key>", ...)``, with
  the key from `KEYS`;
- a typed ``F3`` in report text becomes ``{R.ref('<key>')}`` (the literal
  turns into an f-string, its braces escaped; a whole-literal ``"F3"`` cell
  becomes ``R.ref("<key>")``), and in a figure title ``{R.number(...)}``,
  because a PNG never passes through `render()`;
- a typed ``F3`` in a comment or docstring, which nothing renders, becomes
  ``finding `<key>```, the form ``make lint-finding-citations`` resolves.

A line the rewrite takes past 79 columns is split into implicitly
concatenated literals at a space (inside brackets only), and a comment or
docstring paragraph is re-wrapped; ``make format`` tidies the rest.

**Where the keys came from** (`KEYS`, and ``--suggest`` to reproduce the
first two columns). PR #2147 (commit 23a811033) keyed the same validators
when 58 of its 83 files sat elsewhere, and its keys were reviewed: a
finding whose TEXT matches one there (``difflib`` ratio, by text and never
by number, since several reports were reordered since) reuses that key.
A new finding gets a slug of its text. Every slug, and every reused key
whose finding has since changed meaning, was then read against the text
and hand-fixed where it lost or inverted the meaning; `HAND` lists them.

**What it refuses.** A typed number with no finding of that number in its
own report: that is a citation of ANOTHER report's finding, or of a label
that is not a finding at all, and a key resolves only within its own
report. Those were reworded to state the claim before this ran (the commit
before this one), so the rewrite stays mechanical.

Usage
-----
    python3 scripts/codemod_finding_keys.py --suggest   # print a KEYS table
    python3 scripts/codemod_finding_keys.py             # rewrite in place
"""

from __future__ import annotations

import ast
import difflib
import io
import re
import subprocess
import sys
import textwrap
import tokenize
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: The commit whose keys were reviewed and are reused by text match.
PRIOR = "23a811033"

#: The typed number this migrates. Deliberately the old, wide form: every
#: F-number a report could type, so nothing is left behind.
TYPED = re.compile(r"\bF(?!32\b|64\b)([1-9]\d?)\b")

#: Each report's findings: its folder name, then number to key, in the
#: numbering the report had on main when this ran.
KEYS: dict[str, dict[str, str]] = {
    "acq": {
        "F1": "samples_consumed_unpinned",
        "F2": "noise_mode_zero_mentions_test",
        "F3": "auto_sizer_short_push_empty",
        "F4": "separate_burst_constructor",
        "F5": "cn0_dbhz_est_saturates_once",
        "F6": "searched_doppler_unreported",
        "F7": "pfa_over_delivered",
        "F8": "shaped_pd_optimistic",
        "F9": "burst_sizer_ignored_doppler_rate",
    },
    "burst_acq": {
        "F1": "no_detection_performance",
        "F2": "argument_delivery_thin",
        "F3": "underpowered_warning_untested",
        "F4": "door_null_and_empty_c_only",
    },
    "burst_capture": {
        "F1": "required_gap_short",
        "F2": "filter_on_cn0_not_margin",
        "F3": "reset_left_ring_behind_stream",
        "F4": "search_raw_undocumented_code",
        "F5": "zero_copy_consumer_face_not",
        "F6": "burst_window_copy_measured_short",
        "F7": "design_point_capture_fell_short",
    },
    "agc": {
        "F1": "settling_level_dependent",
        "F2": "inputs_destroyed_loop",
        "F3": "decim_preserved_steady_state_not",
        "F4": "long_prefix_truncated",
        "F5": "detector_state_avg_no_python",
        "F6": "object_no_notion_signal_presence",
    },
    "buffer": {
        "F1": "consume_unbounded_face_crash",
        "F2": "backed_ring_resize_not_zeroed",
        "F3": "header_claims_stale",
        "F4": "space_available_never_go_stale",
        "F5": "file_backed_ring_create_backed",
        "F6": "dropped_counts_refusals",
        "F7": "wait_no_timeout_spins",
        "F8": "re_attached_file_backed_ring",
    },
    "interleaver": {
        "F1": "object_own_permutation_unpinned",
        "F2": "receive_face_zero_coverage",
        "F3": "geometry_mismatch_silent",
        "F4": "partial_block_refused",
        "F5": "no_interleave_soft_no_state",
        "F6": "max_out_refusal_c_only",
        "F7": "half_closed_makeit_half_remains",
        "F8": "refusals_ten_hand_written_raise",
    },
    "detection": {
        "F1": "non_coherent_trio",
        "F2": "threshold_f_explained",
        "F3": "det_dwell_power_declared_twice",
        "F4": "marcum_convergence_claim",
        "F5": "acq_core_states_twice_lines",
        "F6": "det_threshold_det_inv_both",
    },
    "lockdet": {
        "F1": "compounding_holds",
        "F2": "nan_look_used_hold_lock",
        "F3": "non_finite_policy_shared",
        "F4": "lockdet_restore_has_config",
        "F5": "inverted_band_not_refused",
        "F6": "value_embedding_path_no_python",
    },
    "async_dsss_receiver": {
        "F1": "steps_max_out_not_a_bound",
        "F2": "differential_is_inert",
        "F3": "handoff_can_miss_the_carrier",
        "F4": "lock_and_release_c_only",
        "F5": "evm_agrees_with_error_rate",
    },
    "burst_demod": {
        "F1": "negative_verdict_too_short",
        "F2": "frame_offset_ever_observed",
        "F3": "n_symbols_unmentioned",
        "F4": "reset_called_no_either_language",
        "F5": "no_state_triplet_no_serializable",
    },
    "burst_despreader": {
        "F1": "header_example_false_ungated",
        "F2": "lock_stat_zero_means_two",
        "F3": "lock_gate_pfa_never_measured",
        "F4": "set_acq_unasserted",
        "F5": "locked_end_lock_metric_pinned",
    },
    "dsss_burst_receiver": {
        "F1": "bin_frequency_fold_restated_four",
        "F2": "coherent_refine_fails_residual",
        "F3": "partial_refine_discarded",
        "F4": "refine_clamp_lost_phase",
        "F5": "state_bytes_not_pure_function",
        "F6": "code_dominates_loss",
        "F7": "refine_margin_removed",
        "F8": "doppler_on_acq_grid",
        "F9": "spurious_ahead_discards",
        "F11": "push_discarded_rest_input_once",
    },
    "ppe": {
        "F1": "sub_bin_pin_too_wide",
        "F2": "nfft_field_comment_wrong",
        "F3": "snr_db_measured_no",
        "F4": "sub_bin_refinement_not_here",
        "F5": "ppe_has_no_state_triplet",
    },
    "mpsk": {
        "F1": "diff_penalty_asymptote",
        "F2": "decision_ahat_inline_helpers_no",
        "F3": "mpsk_core_folded_no_library",
        "F4": "rotated_diff_symbol_zero",
        "F5": "test_private_correlation_search",
    },
    "resamp": {
        "F1": "ctrl_observable_unbound",
        "F2": "ctrl_port_typed_complex",
        "F3": "ctrl_acc_doc_wrong_structure",
        "F4": "docblock_stated_slip_unit_output",
        "F5": "ctrl_differs_by_delay",
        "F6": "db_output_nyquist_folding_rule",
        "F7": "unity_window",
        "F8": "dp_resamp_dc_gain_named",
        "F9": "interp_inputs_doc_understated",
    },
    "lo": {
        "F1": "dbc_sfdr_claim_typical_value",
        "F2": "comment_beside_lo_step_ctrl",
        "F3": "lo_core_ifdef_avx512f_blocks",
        "F4": "both_max_doc_lines_stale",
        "F5": "lo_ctrl_port_was_float32",
        "F6": "lo_ctrl_cancel_stops_phase_inc",
        "F7": "lo_nco_accumulator_plus_lut",
        "F8": "lut_costs_half_phase_bin",
        "F9": "whole_inline_composition_api",
    },
    "nco": {
        "F1": "freq_error_one_sided",
        "F2": "ctrl_port_was_float32",
        "F3": "ctrl_cancelling_phase_inc_stops",
        "F4": "filed_faces_diverge_infinity",
        "F5": "requests_stop_oscillator",
        "F6": "carry_flag_not_counter",
        "F7": "ctrl_event_composite_sign",
        "F8": "nco_steer_scale_exists_no",
        "F9": "nco_core_claimed_steps_u32",
    },
    "corr2d": {
        "F1": "dp_corr2d_reset_pinned_vacuously",
        "F2": "even_nyquist_bin_split_corr2d",
        "F3": "sub_bin_pinned_integer",
        "F4": "nthreads_now_honored",
        "F5": "three_claims_no_python_face",
        "F6": "dwell_check_incomplete",
    },
    "detector2d": {
        "F1": "noise_modes_unexercised",
        "F2": "set_ref_no_coverage_either",
        "F3": "last_dump_comment_wrong",
        "F4": "entry_points_no_python_face",
        "F5": "dwell_zero_memoryerror",
    },
    "psd": {
        "F1": "kernel_claims_mostly_unpinned",
        "F2": "three_existing_checks_vacuous",
        "F3": "frame_kernel_c_only",
        "F4": "header_described_code_uses_nfft",
        "F5": "pad_zero_was_one",
        "F6": "exp_mode_alpha_outside_accepted",
        "F7": "occupied_bw_any_fraction",
        "F8": "window_no_gain_built_estimator",
        "F9": "band_power_returned_empty_array",
        "F10": "create_size_wrapped",
        "F11": "scalar_readout_means_both_no",
        "F12": "adjacent_bands_both_count_bin",
        "F13": "refusals_were_memoryerror",
        "F14": "stubs_not_say_readers_return",
        "F15": "linear_faces_differ",
        "F16": "windows_were_symmetric",
    },
    "spectrogram": {
        "F1": "byte_written_claim",
        "F2": "claims_verified_in_c",
        "F3": "bin_reads_no_lower_db",
        "F4": "foreign_blob_refused",
        "F5": "object_not_thread_safe",
        "F6": "mode_power_reserved_refused",
        "F7": "rows_inherited_symmetric",
        "F8": "nfft_power_of_two_row_width",
        "F9": "flush_stays_explicit",
        "F10": "speed_not_certified",
        "F11": "hop_refusal_masked_in_c",
        "F12": "db_row_fast_conversion_power",
    },
    "ccsds_tm": {
        "F1": "dp_ccsds_tm_randomise_docblock",
        "F2": "threshold_cost_depends_lead_no",
        "F3": "interleaver_edge_exact",
        "F4": "identity_claim_certified_not",
        "F5": "coverage_table_c_only",
    },
    "conv": {
        "F1": "viterbi_depth_quote_wrong",
        "F2": "claims_c_only_no_binding",
        "F4": "node_sync_margin_022",
    },
    "framer": {
        "F1": "header_read_against_tests",
        "F2": "flush_overran_buffer",
        "F3": "first_snapshot_not_store_hop",
        "F4": "framer_claims_c_only",
        "F5": "framer_owns_ring_exclusively",
        "F6": "drain_cost_not_measured",
    },
    "rs": {
        "F1": "rs_core_offered_rs_configuration",
        "F2": "rs_claims_c_only",
        "F3": "miscorrection_rate_flat",
        "F4": "test_rs_core_refusal_gate",
    },
    "wfmgen": {
        "F1": "leg_goal_pinned_no",
        "F2": "defaults_restated_twice",
        "F3": "goal2_evidence_could_run_nothing",
        "F4": "flags_unexercised",
        "F5": "limits_gate_not_discover_glob",
        "F6": "seed_poly_gold_tap_wider",
        "F7": "frame_from_json_null_no_reason",
        "F8": "impossible_case_claimed",
        "F9": "design_page_said_goal_held",
    },
    "carrier_nda": {
        "F1": "scale_invariance_quote",
        "F2": "threshold_cited_wrong_macro",
        "F3": "lock_default_docstring",
        "F4": "lock_detection_guide_stale",
        "F5": "face_parity_checks_two_sections",
        "F6": "first_bound_right",
        "F7": "half_symbol_closed_form_wrong",
        "F8": "h0_variance_exact_look_not",
        "F9": "get_nco_freq_readout",
        "F10": "disc_forceinline_no_binding",
        "F11": "fold_ambiguity_sharp_edge_worth",
        "F12": "track_vs_declare_envelopes",
    },
    "loop_filter": {
        "F1": "bn_loop_noise_bandwidth_claim",
        "F2": "bandwidth_error_is_law",
        "F3": "t_drops_out_of_error",
        "F4": "dp_loop_filter_step_doxygen",
        "F5": "dp_loop_filter_steps_calls",
        "F6": "domain_at_boundary",
        "F7": "init_leaves_integrator",
        "F8": "restore_carries_config",
        "F9": "init_unreachable_from_python",
        "F10": "no_anti_windup_no_output",
    },
    "mpsk_receiver": {
        "F1": "derive_helpers_c_only",
        "F2": "stable_false_lock_reads_defect",
        "F3": "fold_phase_ambiguity_permanent",
        "F4": "rate_recovered_not_demodulable",
        "F5": "psk8_loss_unmeasurable",
        "F6": "some_es_n0_cells_cannot",
        "F7": "header_claims_uncovered",
        "F8": "ser_theory_figure_report_bound",
    },
    "ratesync": {
        "F1": "header_headline_design_note",
        "F2": "step_ted_doc_contradicted",
        "F3": "measured_table_irreproducible",
        "F4": "bn_identical_claim",
        "F5": "iandd_rule_right_stated_evidence",
        "F6": "input_level_axis_not_monotone",
        "F7": "locked_not_separate_scaling",
        "F8": "scurve_zeros_per_symbol",
        "F9": "planner_output_unbound",
        "F10": "analytic_rrc_pulse_wfm_rrc",
        "F11": "get_clipped_overclaimed",
        "F12": "slope_depends_on_data",
        "F13": "amplitude_laws_differ",
        "F14": "slope_from_peak_assumes_sine",
        "F15": "normalised_slope_unity",
        "F16": "header_lower_self_noise_near",
        "F17": "dttl_low_snr_claim_unmeasurable",
    },
    "ema": {
        "F1": "primitive_exists",
        "F2": "dp_agc_steps_forms_detector",
        "F3": "ema_step_not_total",
        "F4": "coeff_above_one_passes",
        "F5": "product_form_expectations",
        "F6": "callers_inline_header",
    },
    "gold": {
        "F1": "header_family_size_wrong_both",
        "F2": "three_contract_claims_no_behind",
        "F3": "preferred_pair_family",
        "F4": "constants_vs_published",
    },
    "pn": {
        "F1": "table_said_verified_six_sixty",
        "F2": "claim_pinned_own_complement",
        "F3": "three_claims_python_face_cannot",
        "F4": "dp_pn_reset_called_no",
    },
    "wfm_compose": {
        "F1": "face_agreement_unasserted",
        "F2": "face_agreement_test_blind_unless",
        "F3": "summing_not_normalise",
        "F4": "build_render_not_on_python",
    },
    "wfm_frame": {
        "F1": "seq_to_bits_untested",
        "F2": "openness_design_staked",
        "F3": "frame_carries_no_check_answers",
        "F4": "dsss_nchips_via_synth",
    },
    "wfm_plan": {
        "F1": "ranged_gap_length_varies",
        "F2": "no_tell_restore_fast_path",
        "F3": "wfm_plan_three_c_only_claims",
        "F4": "doppler_source_served_exactly",
    },
    "wfm_reader": {
        "F1": "surface_python_binding_calls_no",
        "F2": "header_tag_unchecked",
        "F3": "headerless_type_unread",
        "F4": "fs_fc_answers_not_failures",
    },
    "wfm_synth": {
        "F1": "header_arithmetic_untested",
        "F2": "step_chirp_emitted_flat_cw",
        "F3": "four_public_entry_points_ten",
        "F4": "auto_resolved_twice",
        "F5": "noise_steps_defends_dead_path",
        "F6": "bits_symbol_map_identity_library",
    },
    "wfm_writer": {
        "F1": "quantiser_truncated",
        "F2": "python_properties_untested",
        "F3": "float_capture_reports_peak_not",
        "F4": "four_header_entry_points_not",
        "F5": "sample_type_sidecar_unread",
    },
}

#: Keys named by hand rather than taken as found, and why. Reused keys
#: whose finding's meaning changed since #2147 would now say the opposite
#: of the finding; the generated slugs of new findings lost the point.
#: Why a reused key was renamed: #2147 gave two reports the same key, and
#: a citation outside a report resolves against all of them at once.
_SHARED = "#2147's {} is another report's key too; keys are unique"

HAND: dict[tuple[str, str], str] = {
    ("acq", "F7"): "#2147's pfa_over_delivered: the text was rewritten "
    "when the gap was FIXED, so the match fell under 0.5; the slug "
    "detector_delivered_1_8x_false names a number that has moved",
    ("corr2d", "F4"): "#2147's nthreads_ignored inverts it: nthreads is "
    "real since #2666; slug nthreads_once_documented",
    ("spectrogram", "F2"): "#2147's claims_c_only_no_python_face inverts "
    "it: #2171 added the Python face; slug every_claim_verified_c_where",
    ("spectrogram", "F4"): "#2147's blob_restores_into_other_config "
    "inverts it: #2102 made the blob refused; slug "
    "blob_another_window_beta_mode",
    ("async_dsss_receiver", "F1"): "new; slug steps_max_out_returns_0_not",
    ("async_dsss_receiver", "F2"): "new; slug differential_inert_reaches",
    ("async_dsss_receiver", "F3"): "new; slug first_hand_off_can_land_off",
    ("async_dsss_receiver", "F4"): "new; slug lock_detector_s_hysteresis",
    ("async_dsss_receiver", "F5"): "new; slug over_bursts_settled_error_rate",
    ("lockdet", "F4"): _SHARED.format("restore_carries_config"),
    ("ppe", "F5"): _SHARED.format("no_state_triplet_no_serializable"),
    ("framer", "F4"): _SHARED.format("claims_c_only_no_binding"),
    ("rs", "F2"): _SHARED.format("claims_c_only_no_binding"),
    ("wfm_plan", "F3"): _SHARED.format("three_claims_python_face_cannot"),
    ("lo", "F5"): _SHARED.format("ctrl_port_was_float32"),
    ("lo", "F6"): _SHARED.format("ctrl_cancelling_phase_inc_stops"),
}

#: Calls whose string arguments are drawn into a figure, never rendered.
_FIGURE_CALLS = {
    "set_title",
    "suptitle",
    "set_xlabel",
    "set_ylabel",
    "text",
    "annotate",
    "figtext",
    "set_label",
    "legend",
}

#: Words a slug drops: they carry no meaning in a name.
_STOP = {
    "the",
    "a",
    "an",
    "is",
    "are",
    "was",
    "were",
    "of",
    "to",
    "in",
    "on",
    "and",
    "or",
    "it",
    "its",
    "this",
    "that",
    "by",
    "for",
    "with",
    "as",
    "be",
    "at",
    "from",
    "which",
    "so",
    "but",
    "has",
    "have",
    "had",
    "there",
    "here",
}

_WIDTH = 79


def slug(text: str) -> str:
    """A key from a finding's text: its first meaningful words.

    >>> slug("**The windows are the symmetric (N-1) form**, not periodic")
    'windows_symmetric_n_1_form_not'
    """
    text = re.sub(r"\{[^}]*\}", " ", text)
    words = [
        w for w in re.findall(r"[a-z0-9]+", text.lower()) if w not in _STOP
    ]
    out = ""
    for w in words:
        cand = f"{out}_{w}" if out else w
        if len(cand) > 32:
            break
        out = cand
    return out if re.match(r"[a-z]", out) else f"f_{out}"[:32]


def _static(node: ast.AST) -> str:
    """A string node's literal text, each expression as ``{}``."""
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    if isinstance(node, ast.JoinedStr):
        return "".join(
            v.value if isinstance(v, ast.Constant) else "{}"
            for v in node.values
        )
    if isinstance(node, ast.BinOp):
        return _static(node.left) + _static(node.right)
    if isinstance(node, ast.IfExp):
        return _static(node.body)
    return ""


def _finds(source: str) -> list[tuple[int, str, str]]:
    """Every ``.find("Fn", verdict, text)``: line, number, static text."""
    out = []
    for n in ast.walk(ast.parse(source)):
        if (
            isinstance(n, ast.Call)
            and isinstance(n.func, ast.Attribute)
            and n.func.attr == "find"
            and len(n.args) == 3
            and isinstance(n.args[0], ast.Constant)
            and isinstance(n.args[0].value, str)
        ):
            out.append((n.lineno, n.args[0].value, _static(n.args[2])))
    return sorted(out)


def _git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(ROOT), *args],
        check=True,
        capture_output=True,
        text=True,
    ).stdout


def validators() -> list[Path]:
    """Every tracked ``validate.py``."""
    return [ROOT / p for p in _git("ls-files", "*validate.py").split()]


def suggest() -> int:
    """Print a `KEYS` table: #2147's key by text match, else a slug."""
    prior = {
        p.split("/")[-2]: p
        for p in _git("show", "--name-only", "--format=", PRIOR).split()
        if p.endswith("validate.py")
    }
    for path in validators():
        name = path.parent.name
        old = []
        if name in prior:
            src = _git("show", f"{PRIOR}:{prior[name]}")
            old = [(t, k) for _, k, t in _finds(src)]
        print(f'    "{name}": {{')
        seen = set()
        for _, num, text in _finds(path.read_text(encoding="utf-8")):
            if num in seen:
                continue
            seen.add(num)
            best = max(
                (
                    (difflib.SequenceMatcher(None, text, t).ratio(), k)
                    for t, k in old
                ),
                default=(0.0, ""),
            )
            key, how = (
                (best[1], f"reused, ratio {best[0]:.2f}")
                if best[0] >= 0.5
                else (slug(text), "slug")
            )
            print(f'        "{num}": "{key}",  # {how}')
        print("    },")
    return 0


# ── the rewrite ──────────────────────────────────────────────────────────


def _parents(tree: ast.AST) -> dict[ast.AST, ast.AST]:
    return {c: p for p in ast.walk(tree) for c in ast.iter_child_nodes(p)}


def _is_docstring(node: ast.AST, parents: dict) -> bool:
    expr = parents.get(node)
    if not isinstance(expr, ast.Expr):
        return False
    owner = parents.get(expr)
    # A bare string statement anywhere is prose nothing renders, which is
    # what a docstring is for this purpose.
    return owner is not None


def _context(node: ast.AST, parents: dict) -> str:
    """``key``, ``doc``, ``number`` or ``ref`` for one string node."""
    p = parents.get(node)
    if (
        isinstance(p, ast.Call)
        and isinstance(p.func, ast.Attribute)
        and p.func.attr == "find"
        and p.args
        and p.args[0] is node
    ):
        return "key"
    if _is_docstring(node, parents):
        return "doc"
    cur = node
    while cur in parents:
        cur = parents[cur]
        if (
            isinstance(cur, ast.Call)
            and isinstance(cur.func, ast.Attribute)
            and cur.func.attr in _FIGURE_CALLS
        ):
            return "number"
    return "ref"


def _string_nodes(tree: ast.AST, parents: dict) -> list:
    """(start, end, context) for every outermost string node."""
    out = []
    for n in ast.walk(tree):
        if isinstance(n, ast.JoinedStr) or (
            isinstance(n, ast.Constant)
            and isinstance(n.value, str)
            and not isinstance(parents.get(n), ast.JoinedStr)
        ):
            out.append(
                (
                    (n.lineno, n.col_offset),
                    (n.end_lineno, n.end_col_offset),
                    _context(n, parents),
                )
            )
    return out


def _literal_tokens(source: str) -> list[tuple[tuple, tuple, str, int]]:
    """(start, end, text, bracket depth) per string literal or comment.

    An f-string is one literal here, from its start token to its matching
    end token, and its own braces do not count toward bracket depth.
    """
    toks = list(tokenize.generate_tokens(io.StringIO(source).readline))
    lines = source.splitlines(keepends=True)
    offs = [0]
    for ln in lines:
        offs.append(offs[-1] + len(ln))

    def pos(rc: tuple[int, int]) -> int:
        return offs[rc[0] - 1] + rc[1]

    out, depth, i = [], 0, 0
    fs_start = getattr(tokenize, "FSTRING_START", -1)
    fs_end = getattr(tokenize, "FSTRING_END", -2)
    while i < len(toks):
        t = toks[i]
        if t.type == fs_start:
            nest, j = 0, i
            while True:
                if toks[j].type == fs_start:
                    nest += 1
                elif toks[j].type == fs_end:
                    nest -= 1
                    if nest == 0:
                        break
                j += 1
            s, e = t.start, toks[j].end
            out.append((s, e, source[pos(s) : pos(e)], depth))
            i = j + 1
            continue
        if t.type in (tokenize.STRING, tokenize.COMMENT):
            out.append((t.start, t.end, t.string, depth))
        elif t.type == tokenize.OP and t.string in "([{":
            depth += 1
        elif t.type == tokenize.OP and t.string in ")]}":
            depth -= 1
        i += 1
    return out


def _split_prefix(tok: str) -> tuple[str, str, str]:
    """``(prefix, quote, body)`` of a string literal's source text."""
    m = re.match(r"([A-Za-z]*)('''|\"\"\"|'|\")", tok)
    prefix, quote = m.group(1), m.group(2)
    return prefix, quote, tok[len(prefix) + len(quote) : -len(quote)]


def _top_level(body: str, is_f: bool) -> list[bool]:
    """Per character: is it literal text (not inside an f-expression)?"""
    flags, depth, i = [], 0, 0
    while i < len(body):
        c = body[i]
        if is_f and depth == 0 and body.startswith(("{{", "}}"), i):
            flags += [True, True]
            i += 2
            continue
        if is_f and c == "{":
            depth += 1
        flags.append(depth == 0)
        if is_f and c == "}" and depth:
            depth -= 1
        i += 1
    return flags


def _cite_doc(text: str, keys: dict[str, str], where: str) -> str:
    """Rewrite typed numbers in prose nothing renders as key citations."""

    def key(num: str) -> str:
        if f"F{num}" not in keys:
            raise SystemExit(f"{where}: F{num} names no finding here")
        return f"`{keys[f'F{num}']}`"

    # A run already introduced by "finding(s)": key each number in it.
    run = re.compile(
        r"(\b[Ff]indings?\s+)((?:\*\*)?F\d+(?:\*\*)?"
        r"(?:(?:\s*,\s*(?:and\s+)?|\s+and\s+)(?:\*\*)?F\d+(?:\*\*)?)*)"
    )
    text = run.sub(
        lambda m: (
            m.group(1)
            + re.sub(
                r"(?:\*\*)?F(\d+)(?:\*\*)?",
                lambda n: key(n.group(1)),
                m.group(2),
            )
        ),
        text,
    )

    def lone(m: re.Match[str]) -> str:
        before = text[: m.start()].rstrip(" *")
        cap = not before or before.endswith((".", "#", "\n", '"""'))
        return f"{'Finding' if cap else 'finding'} {key(m.group(1))}"

    return re.sub(
        r"(?:\*\*)?\bF(?!32\b|64\b)([1-9]\d?)\b(?:\*\*)?", lone, text
    )


def _ref_body(body: str, is_f: bool, q: str, keys, how, where) -> str:
    """`body` as an f-string body with each typed number a placeholder."""
    inner = '"' if q.startswith("'") else "'"
    if not is_f:
        body = body.replace("{", "{{").replace("}", "}}")
    flags = _top_level(body, True)
    out, last = [], 0
    for m in TYPED.finditer(body):
        if not flags[m.start()]:
            continue
        num = m.group(0)
        if num not in keys:
            raise SystemExit(f"{where}: {num} names no finding here")
        out.append(body[last : m.start()])
        out.append(f"{{R.{how}({inner}{keys[num]}{inner})}}")
        last = m.end()
    out.append(body[last:])
    return "".join(out)


def _atoms(body: str) -> list[str]:
    """`body` cut where a literal may split: after a space, never in a
    placeholder or an escape."""
    atoms, cur, depth, i = [], "", 0, 0
    while i < len(body):
        c = body[i]
        if depth == 0 and body.startswith(("{{", "}}"), i):
            cur += body[i : i + 2]
            i += 2
            continue
        if c == "\\" and depth == 0:
            cur += body[i : i + 2]
            i += 2
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        cur += c
        if c == " " and depth == 0:
            atoms.append(cur)
            cur = ""
        i += 1
    if cur:
        atoms.append(cur)
    return atoms


def _piece(prefix: str, q: str, body: str) -> str:
    """One literal: an f-string only if it holds a placeholder."""
    p = prefix.replace("f", "").replace("F", "")
    if re.search(r"(?<!\{)\{(?!\{)", body.replace("{{", "")):
        return f"{p}f{q}{body}{q}"
    return f"{p}{q}{body.replace('{{', '{').replace('}}', '}')}{q}"


def _layout(prefix, q, body, first_col, cont_col, room_after) -> list[str]:
    """Pieces of one literal, packed greedily into 79 columns."""
    pieces, cur = [], ""
    col = first_col
    for a in _atoms(body):
        trial = _piece(prefix, q, cur + a)
        if cur and col + len(trial) + room_after > _WIDTH:
            pieces.append(_piece(prefix, q, cur))
            cur, col = a, cont_col
        else:
            cur += a
    pieces.append(_piece(prefix, q, cur))
    return pieces


def _rewrap(lines: list[str], idx: int, lead: str) -> list[str]:
    """Re-wrap the paragraph of lines holding `idx` under `lead`."""
    lo = idx
    while (
        lo > 0 and lines[lo - 1].startswith(lead) and lines[lo - 1].strip(" #")
    ):
        if lines[lo - 1][len(lead) :].startswith((" ", "-", "*")):
            break
        lo -= 1
    hi = idx
    while (
        hi + 1 < len(lines)
        and lines[hi + 1].startswith(lead)
        and lines[hi + 1][len(lead) :].strip()
        and not lines[hi + 1][len(lead) :].startswith((" ", "-", "*"))
    ):
        hi += 1
    words = " ".join(ln[len(lead) :].strip() for ln in lines[lo : hi + 1])
    wrapped = textwrap.wrap(
        words,
        _WIDTH - len(lead),
        break_long_words=False,
        break_on_hyphens=False,
    )
    return lines[:lo] + [lead + w for w in wrapped] + lines[hi + 1 :]


def _rewrap_literal(tok: str, col: int) -> str:
    """Re-wrap, within a multi-line literal, each paragraph that overflows.

    Bounded by the literal: a paragraph never runs past its closing quotes
    into the code after it.
    """
    lines = tok.split("\n")
    for _ in range(len(lines)):
        width = [
            len(ln) + (col if i == 0 else 0) for i, ln in enumerate(lines)
        ]
        bad = next(
            (i for i in range(1, len(lines) - 1) if width[i] > _WIDTH), None
        )
        if bad is None:
            break
        ln = lines[bad]
        body = lines[1:-1]
        new = _rewrap(body, bad - 1, ln[: len(ln) - len(ln.lstrip())])
        if new == body:
            break
        lines = [lines[0], *new, lines[-1]]
    return "\n".join(lines)


def migrate(path: Path) -> tuple[int, int, list[str]]:
    """Rewrite one validator; returns (keys, citations, notes)."""
    name = path.parent.name
    keys = KEYS[name]
    source = path.read_text(encoding="utf-8")
    tree = ast.parse(source)
    parents = _parents(tree)
    nodes = _string_nodes(tree, parents)
    has_global_r = any(
        isinstance(n, ast.Assign)
        and any(isinstance(t, ast.Name) and t.id == "R" for t in n.targets)
        for n in tree.body
    )
    funcs = [
        n
        for n in ast.walk(tree)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
    ]

    def ctx(start: tuple[int, int]) -> str:
        best = None
        for s, e, c in nodes:
            if s <= start < e and (best is None or s >= best[0]):
                best = (s, c)
        return best[1] if best else "ref"

    def r_in_scope(line: int) -> bool:
        if has_global_r:
            return True
        inner = [
            f for f in funcs if f.lineno <= line <= (f.end_lineno or line)
        ]
        return any(
            a.arg == "R"
            for f in inner
            for a in f.args.args + f.args.kwonlyargs
        )

    lines = source.splitlines(keepends=True)
    edits: list[tuple[tuple, tuple, str]] = []
    n_keys = n_cites = 0
    notes: list[str] = []
    for start, end, tok, depth in _literal_tokens(source):
        where = f"{path.relative_to(ROOT)}:{start[0]}"
        if tok.startswith("#"):
            if not TYPED.search(tok):
                continue
            new = _cite_doc(tok, keys, where)
            n_cites += len(TYPED.findall(tok))
            edits.append((start, end, new))
            continue
        c = ctx(start)
        prefix, q, body = _split_prefix(tok)
        if c == "key":
            if TYPED.fullmatch(body):
                edits.append((start, end, f"{prefix}{q}{keys[body]}{q}"))
                n_keys += 1
            continue
        is_f = "f" in prefix.lower()
        hits = [
            m
            for m in TYPED.finditer(body)
            if _top_level(body, is_f)[m.start()]
        ]
        if not hits:
            continue
        n_cites += len(hits)
        if c == "doc" or len(q) == 3:
            if c != "doc":
                notes.append(f"{where}: triple-quoted report text, as prose")
            new = _cite_doc(tok, keys, where)
            edits.append((start, end, _rewrap_literal(new, start[1])))
            continue
        how = "number" if c == "number" else "ref"
        if not r_in_scope(start[0]):
            notes.append(f"{where}: no `R` in scope -- fix by hand")
        if c == "ref" and TYPED.fullmatch(body) and not is_f:
            edits.append((start, end, f'R.ref("{keys[body]}")'))
            continue
        newbody = _ref_body(body, is_f, q, keys, how, where)
        line = lines[start[0] - 1]
        tail = len(line.rstrip("\n")) - end[1]
        col0 = start[1]
        lead = len(line) - len(line.lstrip())
        cont = col0 if line[:col0].strip() == "" else lead + 4
        pieces = _layout(prefix, q, newbody, col0, cont, tail)
        if len(pieces) > 1 and depth == 0:
            notes.append(f"{where}: split outside brackets -- fix by hand")
        edits.append((start, end, ("\n" + " " * cont).join(pieces)))

    # Apply from the end, so earlier positions stay valid.
    text = "".join(lines)
    offs = [0]
    for ln in lines:
        offs.append(offs[-1] + len(ln))
    for s, e, new in sorted(edits, reverse=True):
        a, b = offs[s[0] - 1] + s[1], offs[e[0] - 1] + e[1]
        text = text[:a] + new + text[b:]

    # Re-wrap any comment or docstring paragraph the rewrite overflowed.
    out = text.splitlines()
    for _ in range(len(out)):
        bad = next(
            (
                i
                for i, ln in enumerate(out)
                if len(ln) > _WIDTH
                and ln.lstrip().startswith("# ")
                and any(f"`{k}`" in ln for k in keys.values())
                and "R.ref(" not in ln
                and "R.number(" not in ln
            ),
            None,
        )
        if bad is None:
            break
        ln = out[bad]
        lead = ln[: len(ln) - len(ln.lstrip())]
        if ln.lstrip().startswith("# "):
            lead += "# "
        new = _rewrap(out, bad, lead)
        if new == out:
            notes.append(f"{path.relative_to(ROOT)}:{bad + 1}: overlong")
            break
        out = new
    path.write_text("\n".join(out) + "\n", encoding="utf-8")
    return n_keys, n_cites, notes


def main(argv: list[str]) -> int:
    if "--suggest" in argv[1:]:
        return suggest()
    for (name, num), why in HAND.items():
        assert num in KEYS[name], (name, num, why)
    for table in KEYS.values():
        for key in table.values():
            assert re.fullmatch(r"[a-z][a-z0-9_]*", key) and len(key) <= 32
    total_k = total_c = 0
    for path in validators():
        k, c, notes = migrate(path)
        total_k += k
        total_c += c
        for n in notes:
            print(f"  NOTE {n}")
    print(f"codemod_finding_keys: {total_k} find() keyed, {total_c} cited")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
