# Phase III Slice IIIB.1 Checkpoint: Boundary Active List Scaffold

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIB1/20260502T073132Z`

This slice adds read-only convergence tracking scaffold state: each plate owns `ActiveBoundaryTriangles`, initialized from that plate's local boundary triangles at the start of a remesh window. The list is reset when plate-local topology is rebuilt. No distance-to-front, subduction matrix, polarity rule, filter behavior, projection behavior, or crust mutation was introduced.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Initial active lists equal source boundary triangles | pass | active 6002, source 6002, missing 0, non-boundary 0 |
| Active list stable across rigid steps before remesh | pass | `5b98ea910ff41fb4` -> `5b98ea910ff41fb4` |
| Active list resets at remesh | pass | serial 0 -> 1, hash `5b98ea910ff41fb4` -> `e49fd60c3ce273de` |
| Post-remesh active lists equal rebuilt boundary triangles | pass | active 9792, source 9792, missing 0, non-boundary 0 |
| Convergence tracking replay deterministic | pass | initial `5b98ea910ff41fb4`, pre `5b98ea910ff41fb4`, post `e49fd60c3ce273de` |
| Projection replay hash | pass | `a411b6aad7877a55` vs `a411b6aad7877a55` |
| Phase II state replay hash | pass | `3b4a85366dab80db` vs `3b4a85366dab80db` |
| Crust state replay hash | pass | `a4e4e99de216c31c` vs `a4e4e99de216c31c` |
| Phase II material ledger replay hash | pass | `bc3077100ba291b4` vs `bc3077100ba291b4` |
| Phase II baseline state hash unchanged | pass | baseline `3b4a85366dab80db`, IIIB.1 `3b4a85366dab80db` |
| Phase II baseline material ledger unchanged | pass | baseline `bc3077100ba291b4`, IIIB.1 `bc3077100ba291b4` |

## Tracking Audits

| Replay | Window | Step | Event | Reset serial | Active | Source boundary | Missing | Non-boundary active | Duplicate | Invalid | Empty plates | Hash |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | initial | 0 | 0 | 0 | 6002 | 6002 | 0 | 0 | 0 | 0 | 0 | `5b98ea910ff41fb4` |
| 0 | pre-remesh | 32 | 0 | 0 | 6002 | 6002 | 0 | 0 | 0 | 0 | 0 | `5b98ea910ff41fb4` |
| 0 | post-remesh | 32 | 1 | 1 | 9792 | 9792 | 0 | 0 | 0 | 0 | 0 | `e49fd60c3ce273de` |
| 1 | initial | 0 | 0 | 0 | 6002 | 6002 | 0 | 0 | 0 | 0 | 0 | `5b98ea910ff41fb4` |
| 1 | pre-remesh | 32 | 0 | 0 | 6002 | 6002 | 0 | 0 | 0 | 0 | 0 | `5b98ea910ff41fb4` |
| 1 | post-remesh | 32 | 1 | 1 | 9792 | 9792 | 0 | 0 | 0 | 0 | 0 | `e49fd60c3ce273de` |

## Phase II Baseline

Baseline artifact: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice55/verify_60k_20260502/metrics.jsonl`

| Metric | Baseline | IIIB.1 replay 0 | Result |
|---|---|---|---|
| State hash after resampling | `3b4a85366dab80db` | `3b4a85366dab80db` | pass |
| Material ledger hash | `bc3077100ba291b4` | `bc3077100ba291b4` | pass |

## Notes

- `ActiveBoundaryTriangles` is plate-local process tracking state, not projection authority and not global sample ownership.
- `state_hash` and `crust_state_hash` intentionally remain independent of `convergence_tracking_hash` so Phase II and IIIA evidence stays comparable.
- The hash changes at remesh because the remesh-window reset serial is included; this makes reset observable even in a degenerate case where a rebuilt active list has identical membership.
- IIIB.1 does not add distance-to-front, subduction matrix entries, polarity decisions, or any filter behavior.

## Recommendation

IIIB.1 passes. Pause for user review before IIIB.2 (per-triangle distance-to-front).
