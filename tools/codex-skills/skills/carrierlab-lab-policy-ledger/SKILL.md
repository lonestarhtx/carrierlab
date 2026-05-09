---
name: carrierlab-lab-policy-ledger
description: Track, review, and disclose CarrierLab named lab policies when the paper or thesis is silent or underspecified. Use when adding, removing, approving, questioning, or consolidating lab policies in Phase III/IIIE, especially remesh, multi-hit, q1/q2, zGamma, topology, transform-like, or live-cadence behavior.
---

# CarrierLab Lab Policy Ledger

## Purpose

Prevent an invented CarrierLab rule from quietly becoming "paper discipline."

Use this skill whenever behavior depends on source silence, source ambiguity, or a previous CarrierLab guardrail that may not be paper-cited.

## Classify Authority

For every policy candidate, classify it as one:

- `source_explicit`: thesis/CGF says the rule.
- `source_implicit`: source strongly implies the rule but does not spell it out.
- `source_silent`: source does not decide the case.
- `source_conflict`: source text conflicts or mixes levels.
- `carrierlab_accidental`: an earlier CarrierLab check or convention was added without explicit approval.

## Required Ledger Fields

Record these in the slice report or design checkpoint:

| Field | Required content |
|---|---|
| Policy name | Short stable label. |
| Authority class | One of the classes above. |
| Source citation | Page image / paper section / report line, or "source silent". |
| Behavior | What the implementation does. |
| Forbidden-policy risk | Prior-owner, projection authority, centroid/random/synthetic, Stage 1.5 fallback, etc. |
| Audit fields | Per-record flag/counter/hash/report evidence. |
| Opt-out/baseline | Whether historical diagnostic baseline remains reproducible. |
| Consolidation disclosure | One paragraph for final IIIE/Phase report. |

## Decision Rules

- If a rule is `carrierlab_accidental`, prefer removing/demoting it over adding more policy around it.
- If source is silent and behavior changes simulation output, require explicit user approval before implementation.
- If a rule only adds observability and does not affect behavior, label it diagnostic, not policy.
- If a rule reuses prior ownership, projection-derived authority, or Stage 1.5 fallback, treat it as a stop condition unless the user explicitly asks for a non-paper comparison path.

## Review Questions

- Is this actually paper-cited, or did we inherit it from our own report language?
- Does the policy decide ownership/material/elevation, or only record diagnostics?
- Can the policy be audited per record, not just by aggregate count?
- Does consolidation need to list this as a named lab policy?
- Would removing this rule make CarrierLab more paper-literal?
