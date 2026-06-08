# Clean Restart Plan

Date: 2026-06-08

Purpose: restart CarrierLab's PTP carrier recreation without deleting current work, using the current implementation as an archive and the V2 contract as the new source of truth.

## Decision

Do not keep patching the current Phase III/IIIE path as the main proof path.

Start a new minimal carrier core beside the archived implementation. Pull old utilities forward only after they satisfy `docs/rebaseline/ptp-carrier-contract-v2.md`.

## Non-Destructive Archive

Before source replacement:

1. Preserve the current code and dirty worktree.
2. Commit or branch/tag the archive intentionally once the user chooses the exact archive strategy.
3. Do not delete old commandlets during the first restart slice.
4. Mark old stages/checkpoints as historical evidence in docs, not active proof.

Suggested archive branch/tag names:

- branch: `archive/pre-rebaseline-carrierlab`
- tag: `archive-pre-rebaseline-2026-06-08`

Do this only after the current dirty files are either committed intentionally or explicitly discarded by the user.

## New Code Boundary

Recommended new namespace/module surface:

- `FCarrierCoreV2`
- `FCarrierSubstrateV2`
- `FCarrierMaterialV2`
- `FCarrierPlateV2`
- `FCarrierProjectionV2`
- `FCarrierMetricsV2`

Recommended commandlets:

- `CarrierLabV2Stage0Commandlet`
- `CarrierLabV2Stage1Commandlet`
- `CarrierLabV2PreRemeshAuditCommandlet`

Avoid reusing Phase III commandlet names in the new proof path.

## Phase A: Pre-Code Contract Lock

Inputs:

- `docs/rebaseline/ptp-carrier-contract-v2.md`
- `docs/paper-carrier-extraction.md`
- `docs/paper-resampling-extraction.md`
- `docs/rock3-metadata-and-ptp-carrier-synthesis.md`

Deliverables:

- one source-anchor table for V2 requirements
- one checkpoint template
- one metric schema

Exit criteria:

- every V2 requirement is either PTP/thesis-sourced, derived, or lab policy
- no current-code API is treated as mandatory

## Phase B: V2 Stage 0 Cold Start

Implement only:

- deterministic sphere substrate
- global spherical triangulation
- initial plate partition
- plate-local duplicated/re-indexed/re-compacted triangulations
- plate-local material records
- center-to-sample ray projection
- raw hit classification
- projected readout metrics

Do not implement:

- motion
- resampling
- gap fill
- subduction
- collision
- rifting
- erosion
- actor/control-panel UI

Exit criteria:

- zero non-degenerate miss
- zero non-degenerate overlap
- all boundary-degenerate cases counted
- material authority and projected output reported separately
- same-seed replay stable

## Phase C: V2 Stage 1 Rigid Motion

Implement only:

- geodetic plate motion
- physical rotation of plate-local vertices
- rotation of required plate-local vector fields if present
- projection against moved plate-local geometry
- independent drift oracle

Do not implement:

- remeshing
- mutation
- repair/fill
- process-state filtering

Exit criteria:

- moved authority matches independent motion oracle
- projection reads from moved geometry
- material continuity is stable before resampling exists
- same-seed replay stable

## Phase D: Process-State Gate Before Remesh

Implement/audit:

- convergence contact detection
- subduction/collision eligibility
- triangle/process marks
- filtered-hit oracle
- boundary provenance maps

Exit criteria:

- overlap resolution is process-state driven
- no centroid/random/synthetic tie-break is needed for the paper-faithful path
- pre-remesh commandlet can verify filtering behavior on fixtures

## Phase E: V2 Remesh

Implement only after Phase D:

- thesis pre-treatment
- ray sampling after filtering subducting/colliding triangles
- q1/q2/qGamma divergent gap fill
- plate-local topology rebuild
- process-state reset/invalidation
- material accounting gates

Exit criteria:

- divergent gaps are filled only by named q1/q2/qGamma process
- subducting/colliding triangles are excluded before candidate selection
- no prior-owner fallback exists
- per-plate material/area/mass deltas are bounded and explained

## What To Reuse From Current Code

Likely reusable after review:

- JSON/CSV/Markdown report writing patterns
- replay hash patterns
- map export/contact-sheet ideas
- independent oracle style
- selected math helpers only if source-clean and simple
- commandlet harness style

Do not reuse blindly:

- Stage 1.5 resolver policies
- Phase III/IIIE remesh patch logic
- UI/live actor state as proof
- any hidden fallback or diagnostic policy wired into primary behavior

## First Implementation Slice

First code slice should be tiny:

1. Add V2 substrate/material/plate structs.
2. Build deterministic tiny fixture, for example 1k samples and 4 plates.
3. Duplicate plate-local topology.
4. Run t=0 projection.
5. Emit a report with raw hit counts, misses, overlaps, boundary-degenerate counts, and material-authority/projected-output separation.

No maps unless the numeric report passes.

## Stop Conditions

Stop and revise the contract if:

- V2 Stage 0 requires any prior global owner to pass.
- V2 Stage 0 has unexplained non-degenerate misses/overlaps.
- V2 Stage 1 cannot verify motion through an independent oracle.
- Remesh design appears before process-state filtering is available.
- A visual result is used to justify correctness before numeric gates pass.

## Expected Outcome

This restart should produce a much smaller proof path:

- fewer commandlets
- fewer policy branches
- less visual/UI coupling
- stronger material authority
- cleaner checkpoints
- easier final claim: faithful reproduction, mechanical divergence, or source underspecification
