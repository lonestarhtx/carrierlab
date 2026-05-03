# Phase III Slice IIID.4 Report - Slab Break Topology Detach Dry Run

Generated: `2026-05-03 17:30:52` UTC

## Scope

IIID.4 consumes IIID.3 accepted destination-mass records and produces a dry-run Slab Break detach plan for the source terrane. It emits removed-triangle sets and old-to-new survivor index maps only. It is read-only: no topology detach, suture, uplift, plate motion change, resampling, or carrier-authority mutation occurs in this slice.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID4/20260503T173005Z/metrics.jsonl`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Slab-break dry-run plan deterministic | pass | replay A `6b1382c0d8553ce7`, replay B `6b1382c0d8553ce7`, plans 1 / 1 |
| Removed set equals terrane and survivor topology valid | pass | removed 4, surviving 9988, invalid plans 0, non-destroying source plans 0 |
| Pure-oceanic negative emits no detach plans | pass | decisions 1, collision candidates 0, plans 0 |
| No lab multi-hit policy influence | pass | policy-resolved multi-hit counts 0 / 0 / 0 / 0 |

## Fixture Results

| Fixture | Replay | Step | Matrix hits | Collision candidates | Mass accepted | Plans | Valid | Invalid | Destroy source | Removed tris | Surviving tris | Patch tris | Policy multi-hits | Read-only stable | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| source_patch_detach_plan | 0 | 2 | 346 | 1 | 1 | 1 | 1 | 0 | 0 | 4 | 9988 | 1 | 0 | yes | `6b1382c0d8553ce7` |
| source_patch_detach_plan | 1 | 2 | 346 | 1 | 1 | 1 | 1 | 0 | 0 | 4 | 9988 | 1 | 0 | yes | `6b1382c0d8553ce7` |
| pure_oceanic_negative | 0 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | yes | `254f0c284eb787da` |
| pure_oceanic_negative | 1 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | yes | `254f0c284eb787da` |

## Representative Plan

- Source-patch replay A: pair `1`, source 1 -> destination 0, removed 4 triangles, surviving 9988 triangles, cut boundary edges 5, valid yes, plan hash `b8edd0c91693692b`

## Interpretation

The source-patch fixture sculpts an evidence-adjacent seed patch on the source plate after IIID.2 collision evidence exists. The connected continental terrane may expand beyond that seed through normal IIID.1 traversal; the dry-run gate therefore requires the removed terrane to contain the seed patch, preserve surviving source topology, and match the terrane record exactly. Duplicate accepted mass records that reference the same source-terrane removal set are counted and deduped into one plan.

All plan calls are audited before and after against projection, state, crust, and convergence hashes. Stable before/after hashes show this slice observes and plans from collision evidence without making topology, motion, or authority changes.

## Verdict

PASS. IIID.4 slab-break detachment planning is deterministic and read-only for the exercised fixtures. IIID.5 may plan suture attachment, but must still avoid topology mutation until the later integrated event slice.
